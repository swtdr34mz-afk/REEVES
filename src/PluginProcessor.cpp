/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (REEVES) */
#include "PluginProcessor.h"
#include "PanelDialog.h"
#include "Rev2Syx.h"
#include "PluginEditor.h"

using namespace rv;

/* ------------------------------------------------------------------------------- parameters --- */
juce::AudioProcessorValueTreeState::ParameterLayout ReevesProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (int l = 0; l < 2; l++) {
        auto group = std::make_unique<juce::AudioProcessorParameterGroup>(l ? "layerB" : "layerA", l ? "Layer B" : "Layer A", " | ");
        for (int p = 0; p < NLAYER; p++) {
            const ParamDef &d = LAYER[p];
            juce::String name = juce::String(l ? "B " : "A ") + d.name;
            group->addChild(std::make_unique<juce::AudioParameterInt>(
                juce::ParameterID{layerId(l, p), 1}, name, d.lo, d.hi, d.def,
                juce::AudioParameterIntAttributes().withStringFromValueFunction([p](int v, int) {
                    char t[32];
                    paramText(p, v, t, sizeof t);
                    return juce::String(t);
                })));
        }
        layout.add(std::move(group));
    }
    for (int p = 0; p < NPROGRAM; p++) {
        const ParamDef &d = PROGRAM[p];
        layout.add(std::make_unique<juce::AudioParameterInt>(
            juce::ParameterID{d.id, 1}, d.name, d.lo, d.hi, d.def,
            juce::AudioParameterIntAttributes().withStringFromValueFunction([p](int v, int) {
                const ParamDef &q = PROGRAM[p];
                if (q.kind == K_CHOICE)
                    return juce::String(q.choice[juce::jlimit(q.lo, q.hi, v)]);
                static const char *const N[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
                return juce::String(N[v % 12]) + juce::String(v / 12 - 2);
            })));
    }
    layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"voices", 1}, "Voices",
                                                            juce::StringArray{"8 Voice", "16 Voice"}, 1));
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"host_sync", 1}, "Host Sync", true));
    layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"quality", 1}, "Quality",
                                                            juce::StringArray{"ECO (2x)", "HIGH (4x)"}, 1));
    layout.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID{"transpose", 1}, "Transpose", -2, 2, 0));
    /* LOW SPLIT (on the MIDI notes, LowSplit.h) */
    layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"lsplit", 1}, "Low Split",
                                                            juce::StringArray{"Off", "-1 Oct", "-2 Oct", "-3 Oct"}, 0));
    layout.add(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID{"lsplit_point", 1}, "Low Split Point", 24, 96, 60,
        juce::AudioParameterIntAttributes().withStringFromValueFunction([](int v, int) {
            static const char *const N[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
            return juce::String(N[v % 12]) + juce::String(v / 12 - 2);   /* (as Cubase names them: note 60 is C3) */
        })));
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"low_hold", 1}, "Low Hold", false));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"volume", 1}, "Master Volume", juce::NormalisableRange<float>(0.0f, 1.0f), 0.7f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction([](float v, int) {
            return v <= 0.0001f ? juce::String("-inf") : juce::String(20.0f * std::log10(v * v * 2.0f), 1) + " dB";
        })));
    return layout;
}

ReevesProcessor::ReevesProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "REEVES", createLayout())
{
    for (int l = 0; l < 2; l++)
        for (int p = 0; p < NLAYER; p++)
            lparam[l][p] = apvts.getParameter(layerId(l, p));
    for (int p = 0; p < NPROGRAM; p++)
        pparam[p] = apvts.getParameter(PROGRAM[p].id);
    loadUserPrograms();
    live = FACTORY[0];
    applyProgram(FACTORY[0], true);
}

int ReevesProcessor::value(int l, int p) const { return (int)std::lround(lparam[l][p]->convertFrom0to1(lparam[l][p]->getValue())); }
int ReevesProcessor::progValue(int p) const { return (int)std::lround(pparam[p]->convertFrom0to1(pparam[p]->getValue())); }
void ReevesProcessor::setValue(int l, int p, int v)
{
    auto *q = lparam[l][p];
    v = juce::jlimit(LAYER[p].lo, LAYER[p].hi, v);
    q->beginChangeGesture();
    q->setValueNotifyingHost(q->convertTo0to1((float)v));
    q->endChangeGesture();
}
void ReevesProcessor::setProgValue(int p, int v)
{
    auto *q = pparam[p];
    q->beginChangeGesture();
    q->setValueNotifyingHost(q->convertTo0to1((float)juce::jlimit(PROGRAM[p].lo, PROGRAM[p].hi, v)));
    q->endChangeGesture();
}

void ReevesProcessor::command(Cmd c, int layer, int a, int b, int v)
{
    int s1, n1, s2, n2;
    fifo.prepareToWrite(1, s1, n1, s2, n2);
    if (n1 > 0)
        cmds[(size_t)s1] = {(int)c, layer, a, b, v};
    else if (n2 > 0)
        cmds[(size_t)s2] = {(int)c, layer, a, b, v};
    fifo.finishedWrite(n1 + n2);
}

/* ------------------------------------------------------------------------------- programs --- */
void ReevesProcessor::applyProgram(const Program &p0, bool toHost)
{
    Program p = p0;
    for (int l = 0; l < 2; l++)               /* (the poly sequencer's PLAY: off with a new program) */
        if (p.layer[l][L_SEQ_TYPE] == ST_POLY)
            p.layer[l][L_SEQ_ON] = 0;
    {
        juce::SpinLock::ScopedLockType lock(progLock);
        pending = p;
        live = p;
    }
    pendingSet.store(true);
    if (toHost) {
        for (int l = 0; l < 2; l++)
            for (int i = 0; i < NLAYER; i++) {
                auto *q = lparam[l][i];
                q->setValueNotifyingHost(q->convertTo0to1((float)p.layer[l][i]));
            }
        for (int i = 0; i < NPROGRAM; i++)
            if (i != G_EDIT_B)
                pparam[i]->setValueNotifyingHost(pparam[i]->convertTo0to1((float)p.prog[i]));
    }
}

void ReevesProcessor::setCurrentProgram(int index)
{
    if (index < 0 || index >= getNumPrograms())
        return;
    program = index;
    compare = false;
    applyProgram(index < NFACTORY ? FACTORY[index] : userPrograms[(size_t)(index - NFACTORY)], true);
    updateHostDisplay(ChangeDetails().withProgramChanged(true));
}

const juce::String ReevesProcessor::getProgramName(int index)
{
    if (index >= 0 && index < NFACTORY)
        return juce::String(FACTORY[index].name);
    if (index >= NFACTORY && index < getNumPrograms())
        return juce::String(userPrograms[(size_t)(index - NFACTORY)].name);
    return {};
}

juce::String ReevesProcessor::programName() const
{
    juce::SpinLock::ScopedLockType lock(progLock);
    return juce::String(live.name);
}
void ReevesProcessor::setProgramName(const juce::String &n)
{
    juce::SpinLock::ScopedLockType lock(progLock);
    std::snprintf(live.name, NAME_LEN, "%s", n.substring(0, NAME_LEN - 1).toRawUTF8());
}
SeqData ReevesProcessor::seqCopy(int l) const
{
    juce::SpinLock::ScopedLockType lock(progLock);
    return live.seq[l];
}
std::array<int8_t, 16> ReevesProcessor::chordCopy(int l) const
{
    juce::SpinLock::ScopedLockType lock(progLock);
    std::array<int8_t, 16> c;
    for (int i = 0; i < 16; i++)
        c[(size_t)i] = live.chord[l][i];
    return c;
}

Program ReevesProcessor::currentProgram() const
{
    Program p;
    {
        juce::SpinLock::ScopedLockType lock(progLock);
        p = live;
    }
    for (int l = 0; l < 2; l++)
        for (int i = 0; i < NLAYER; i++)
            p.layer[l][i] = (int16_t)value(l, i);
    for (int i = 0; i < NPROGRAM; i++)
        p.prog[i] = (int16_t)progValue(i);
    return p;
}

void ReevesProcessor::setCompare(bool on)
{
    if (on == compare)
        return;
    if (on) {
        editKept = currentProgram();
        compare = true;
        applyProgram(program < NFACTORY ? FACTORY[program] : userPrograms[(size_t)(program - NFACTORY)], true);
    } else {
        compare = false;
        applyProgram(editKept, true);
    }
}

juce::File ReevesProcessor::programFolder()
{
    return pdlg::docsOrAppData().getChildFile("REEVES").getChildFile("Programs");
}

void ReevesProcessor::programToXml(const Program &p, juce::XmlElement &x)
{
    x.setAttribute("name", juce::String(p.name));
    x.setAttribute("version", 1);
    for (int l = 0; l < 2; l++)
        for (int i = 0; i < NLAYER; i++)
            x.setAttribute(layerId(l, i), (int)p.layer[l][i]);
    for (int i = 0; i < NPROGRAM; i++)
        x.setAttribute(PROGRAM[i].id, (int)p.prog[i]);
    juce::MemoryBlock mb(p.seq, sizeof p.seq);
    x.setAttribute("seq", mb.toBase64Encoding());
    juce::MemoryBlock cb(p.chord, sizeof p.chord);
    x.setAttribute("chord", cb.toBase64Encoding());
}

bool ReevesProcessor::programFromXml(const juce::XmlElement &x, Program &p)
{
    programInit(p);
    std::snprintf(p.name, NAME_LEN, "%s", x.getStringAttribute("name", "Program").substring(0, NAME_LEN - 1).toRawUTF8());
    for (int l = 0; l < 2; l++)
        for (int i = 0; i < NLAYER; i++)
            p.layer[l][i] = (int16_t)juce::jlimit(LAYER[i].lo, LAYER[i].hi, x.getIntAttribute(layerId(l, i), LAYER[i].def));
    for (int i = 0; i < NPROGRAM; i++)
        p.prog[i] = (int16_t)juce::jlimit(PROGRAM[i].lo, PROGRAM[i].hi, x.getIntAttribute(PROGRAM[i].id, PROGRAM[i].def));
    juce::MemoryBlock mb;
    if (mb.fromBase64Encoding(x.getStringAttribute("seq")) && mb.getSize() == sizeof p.seq)
        std::memcpy(p.seq, mb.getData(), sizeof p.seq);
    juce::MemoryBlock cb;
    if (cb.fromBase64Encoding(x.getStringAttribute("chord")) && cb.getSize() == sizeof p.chord)
        std::memcpy(p.chord, cb.getData(), sizeof p.chord);
    for (int l = 0; l < 2; l++) {
        auto &s = p.seq[l];
        if (s.length > PSEQ_STEPS)
            s.length = PSEQ_STEPS;
    }
    return true;
}

void ReevesProcessor::loadUserPrograms()
{
    userPrograms.clear();
    userGroup.clear();
    auto folder = programFolder();
    auto load = [&](const juce::File &dir, const juce::String &group) {
        auto files = dir.findChildFiles(juce::File::findFiles, false, "*.reeves");
        files.sort();
        for (auto &f : files)
            if (auto xml = juce::XmlDocument::parse(f))
                if (xml->hasTagName("REEVESPROGRAM")) {
                    Program p;
                    if (programFromXml(*xml, p)) {
                        userPrograms.push_back(p);
                        userGroup.push_back(group);
                    }
                }
    };
    load(folder, {});                    /* the user's own first, then each imported bank */
    auto dirs = folder.findChildFiles(juce::File::findDirectories, false);
    dirs.sort();
    for (auto &d : dirs)
        load(d, d.getFileName());
}

int ReevesProcessor::importRev2(const juce::File &syx)
{
    juce::MemoryBlock mb;
    if (!syx.loadFileAsData(mb) || mb.getSize() == 0)
        return 0;
    auto found = rev2ReadSyx((const uint8_t *)mb.getData(), mb.getSize());
    if (found.empty())
        return 0;
    auto dir = programFolder().getChildFile(juce::File::createLegalFileName(syx.getFileNameWithoutExtension()));
    dir.createDirectory();
    int n = 0, k = 0;
    for (auto &r : found) {
        /* the file named after its place on the Rev2 (bank 1-4, program 0-127), so the bank keeps its order */
        juce::String at = r.bank >= 0 ? juce::String(r.bank + 1) + "-" + juce::String(r.number).paddedLeft('0', 3)
                                      : "E-" + juce::String(k).paddedLeft('0', 3);
        k++;
        juce::XmlElement xml("REEVESPROGRAM");
        programToXml(r.program, xml);
        auto f = dir.getChildFile(juce::File::createLegalFileName(at + " " + juce::String(r.program.name)) + ".reeves");
        if (xml.writeTo(f))
            n++;
    }
    loadUserPrograms();
    updateHostDisplay(ChangeDetails().withProgramChanged(true));
    return n;
}

int ReevesProcessor::writeProgram(const juce::String &name)
{
    auto clean = name.trim().substring(0, NAME_LEN - 1);
    if (clean.isEmpty())
        return -1;
    Program p = currentProgram();
    std::snprintf(p.name, NAME_LEN, "%s", clean.toRawUTF8());
    juce::XmlElement xml("REEVESPROGRAM");
    programToXml(p, xml);
    auto folder = programFolder();
    folder.createDirectory();
    if (!xml.writeTo(folder.getChildFile(juce::File::createLegalFileName(clean) + ".reeves")))
        return -1;
    setProgramName(clean);
    loadUserPrograms();
    for (size_t i = 0; i < userPrograms.size(); i++)
        if (juce::String(userPrograms[i].name) == clean) {
            program = NFACTORY + (int)i;
            updateHostDisplay(ChangeDetails().withProgramChanged(true));
            return program;
        }
    return -1;
}

/* ------------------------------------------------------------------------------- the block --- */
bool ReevesProcessor::isBusesLayoutSupported(const BusesLayout &layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void ReevesProcessor::prepareToPlay(double sampleRate, int)
{
    uiMidi.reset(sampleRate);
    engine.prepare(sampleRate);
    lowSplit.forget(true);
    Program cur = currentProgram();             /* (it takes the lock itself: not inside it) */
    {
        juce::SpinLock::ScopedLockType lock(progLock);
        pending = cur;
    }
    pendingSet.store(true);
    started = false;
    sentVoices = -1;
    lastGain = -1.0f;
}

void ReevesProcessor::handleMidi(const juce::MidiMessage &m)
{
    if (m.isNoteOn())
        lowSplit.noteDown(engine, m.getNoteNumber(), m.getFloatVelocity(), transpose);
    else if (m.isNoteOff())
        lowSplit.noteUp(engine, m.getNoteNumber());
    else if (m.isPitchWheel())
        engine.bend((float)(m.getPitchWheelValue() - 8192) / 8192.0f);
    else if (m.isChannelPressure())
        engine.pressure((float)m.getChannelPressureValue() / 127.0f);
    else if (m.isAftertouch())
        engine.pressure((float)m.getAfterTouchValue() / 127.0f);
    else if (m.isSustainPedalOn() || m.isSustainPedalOff())
        engine.sustain(m.isSustainPedalOn());
    else if (m.isController()) {
        int cc = m.getControllerNumber();
        float v = (float)m.getControllerValue() / 127.0f;
        switch (cc) {
        case 1: engine.modWheel(v); break;
        case 2: engine.breath(v); break;
        case 4: engine.foot(v); break;
        case 11: engine.expression(v); break;
        case 120:
            engine.panic();
            lowSplit.forget(true);
            break;
        case 123:
            engine.allOff();
            lowSplit.forget(false);
            break;
        case 121:
            engine.bend(0.0f);
            engine.modWheel(0.0f);
            engine.pressure(0.0f);
            engine.sustain(false);
            break;
        default: break;
        }
    } else if (m.isAllNotesOff() || m.isAllSoundOff()) {
        engine.allOff();
        lowSplit.forget(false);
    }
}

void ReevesProcessor::processBlock(juce::AudioBuffer<float> &buffer, juce::MidiBuffer &midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    float *L = buffer.getWritePointer(0), *R = buffer.getWritePointer(buffer.getNumChannels() > 1 ? 1 : 0);

    /* a whole program (a program change, a project): its sequences and chords, then the parameters below */
    if (pendingSet.exchange(false)) {
        Program p;
        {
            juce::SpinLock::ScopedLockType lock(progLock);
            p = pending;
        }
        engine.setProgram(p);
        for (int l = 0; l < 2; l++)
            for (int i = 0; i < NLAYER; i++)
                sentL[l][i] = p.layer[l][i];
        for (int i = 0; i < NPROGRAM; i++)
            sentP[i] = p.prog[i];
        started = true;
    }
    /* the parameters, when they change */
    for (int l = 0; l < 2; l++)
        for (int i = 0; i < NLAYER; i++) {
            int v = (int)std::lround(lparam[l][i]->convertFrom0to1(lparam[l][i]->getValue()));
            if (v != sentL[l][i]) {
                if (i == L_SEQ_ON && LAYER[i].kind == K_ONOFF && engine.program().layer[l][L_SEQ_TYPE] == ST_POLY)
                    engine.seqPlay(l, v != 0);
                engine.setParam(l, i, v);
                sentL[l][i] = (int16_t)v;
            }
        }
    for (int i = 0; i < NPROGRAM; i++) {
        int v = progValue(i);
        if (v != sentP[i]) {
            engine.setProgramParam(i, v);
            sentP[i] = (int16_t)v;
        }
    }
    int voices = apvts.getRawParameterValue("voices")->load() > 0.5f ? 16 : 8;
    if (voices != sentVoices) {
        engine.setVoices(voices);
        sentVoices = voices;
    }
    transpose = (int)std::lround(apvts.getRawParameterValue("transpose")->load());
    lowSplit.set(engine, (int)std::lround(apvts.getRawParameterValue("lsplit")->load()),
                 (int)std::lround(apvts.getRawParameterValue("lsplit_point")->load()),
                 apvts.getRawParameterValue("low_hold")->load() > 0.5f);
    engine.setQuality(apvts.getRawParameterValue("quality")->load() > 0.5f ? 1 : 0);

    /* the panel's commands */
    {
        int s1, n1, s2, n2;
        fifo.prepareToRead(fifo.getNumReady(), s1, n1, s2, n2);
        auto run = [&](const Command &c) {
            switch (c.c) {
            case C_PLAY: engine.seqPlay(c.layer, c.v != 0); break;
            case C_RECORD: engine.seqRecord(c.layer, c.v != 0); break;
            case C_REST: engine.seqRest(c.layer); break;
            case C_TIE: engine.seqTie(c.layer); break;
            case C_BACK: engine.seqBack(c.layer); break;
            case C_HOLD: engine.hold(c.v != 0); break;
            case C_CHORD: engine.unisonChord(c.layer); break;
            case C_GSTEP: engine.setGatedStep(c.layer, c.a, c.b, c.v); break;
            case C_PANIC:
                engine.panic();
                lowSplit.forget(true);
                break;
            default: break;
            }
        };
        for (int i = 0; i < n1; i++)
            run(cmds[(size_t)(s1 + i)]);
        for (int i = 0; i < n2; i++)
            run(cmds[(size_t)(s2 + i)]);
        fifo.finishedRead(n1 + n2);
    }

    /* the host's tempo and position */
    bool sync = apvts.getRawParameterValue("host_sync")->load() > 0.5f;
    double bpm = 120.0, ppq = 0.0;
    bool playing = false;
    if (auto *ph = getPlayHead())
        if (auto pos = ph->getPosition()) {
            if (auto b = pos->getBpm())
                bpm = *b;
            playing = pos->getIsPlaying();
            if (auto q = pos->getPpqPosition())
                ppq = *q;
        }
    engine.host(playing, ppq, bpm, sync);

    uiMidi.removeNextBlockOfMessages(midi, n);
    keyboardState.processNextMidiBuffer(midi, 0, n, true);

    int done = 0;
    for (const auto meta : midi) {
        int at = juce::jlimit(0, n, meta.samplePosition);
        if (at > done) {
            engine.render(L + done, R + done, at - done);
            done = at;
        }
        handleMidi(meta.getMessage());
    }
    if (n > done)
        engine.render(L + done, R + done, n - done);

    /* for the editor */
    for (int v = 0; v < MAXV; v++) {
        voiceLevel[v].store(engine.voiceLevel(v), std::memory_order_relaxed);
        voiceLayer[v].store(engine.voiceLayer(v), std::memory_order_relaxed);
    }
    for (int l = 0; l < 2; l++) {
        seqStepNow[l].store(engine.seqStep(l), std::memory_order_relaxed);
        recStepNow[l].store(engine.seqRecStep(l), std::memory_order_relaxed);
        recording[l].store(engine.seqRecording(l), std::memory_order_relaxed);
        beatLed[l].store(engine.clockBeat(l), std::memory_order_relaxed);
        for (int k = 0; k < 4; k++)
            lfoNow[l][k].store(engine.lfoValue(l, k), std::memory_order_relaxed);
    }
    bpmNow.store(engine.bpmNow(0), std::memory_order_relaxed);
    holdNow.store(engine.holding(), std::memory_order_relaxed);
    /* what the engine changed itself: the sequences, the chords */
    {
        const Program &ep = engine.program();
        juce::SpinLock::ScopedTryLockType lock(progLock);
        if (lock.isLocked() && !pendingSet.load()) {
            std::memcpy(live.seq, ep.seq, sizeof live.seq);
            std::memcpy(live.chord, ep.chord, sizeof live.chord);
        }
    }

    float vol = apvts.getRawParameterValue("volume")->load();
    float gain = vol * vol * 2.0f;
    if (lastGain < 0.0f)
        lastGain = gain;
    buffer.applyGainRamp(0, 0, n, lastGain, gain);
    if (buffer.getNumChannels() > 1)
        buffer.applyGainRamp(1, 0, n, lastGain, gain);
    lastGain = gain;
    for (int c = 2; c < buffer.getNumChannels(); c++)
        buffer.clear(c, 0, n);
}

/* ------------------------------------------------------------------------------ state --- */
void ReevesProcessor::getStateInformation(juce::MemoryBlock &destData)
{
    auto state = apvts.copyState();
    state.setProperty("program", program, nullptr);
    state.setProperty("uiScale", (double)uiScale.load(), nullptr);
    state.setProperty("uiScaleSet", uiScaleSet.load(), nullptr);
    state.setProperty("editLayer", editLayer.load(), nullptr);
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    auto *pg = xml->createNewChildElement("REEVESPROGRAM");
    programToXml(currentProgram(), *pg);
    copyXmlToBinary(*xml, destData);
}

void ReevesProcessor::setStateInformation(const void *data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (!xml || !xml->hasTagName(apvts.state.getType()))
        return;
    Program p;
    bool hasProg = false;
    if (auto *pg = xml->getChildByName("REEVESPROGRAM")) {
        hasProg = programFromXml(*pg, p);
        xml->removeChildElement(pg, true);
    }
    auto state = juce::ValueTree::fromXml(*xml);
    program = (int)state.getProperty("program", 0);
    uiScale = (float)(double)state.getProperty("uiScale", 0.9);
    uiScaleSet = (bool)state.getProperty("uiScaleSet", false);
    editLayer = (int)state.getProperty("editLayer", 0);
    apvts.replaceState(state);
    if (hasProg) {
        /* the parameters came with the state; the sequences, chords and name with the program */
        for (int l = 0; l < 2; l++)
            for (int i = 0; i < NLAYER; i++)
                p.layer[l][i] = (int16_t)value(l, i);
        for (int i = 0; i < NPROGRAM; i++)
            p.prog[i] = (int16_t)progValue(i);
        applyProgram(p, false);
    }
}

juce::AudioProcessorEditor *ReevesProcessor::createEditor() { return new ReevesEditor(*this); }

juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter() { return new ReevesProcessor(); }
