// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2026 swtdr34mz (REEVES)
//
// REEVES for the MPC: the same engine as the VST3 and AU (dsp/: Engine, Params, Presets), hosted by the MPC Plugin
// SDK's framework (mpc_plugin.h). mpc/build.py copies the engine's files beside this one before the SDK builds it.
//
// The sound: 8 or 16 voices over two layers (single, stack, split), two oscillators with shape mod, sub, noise, sync,
// slop and glide, the Curtis-style 4-pole or 2-pole low-pass (4x HIGH or 2x ECO), three DADSR envelopes, four LFOs,
// eight mod slots, an effect, clock, arp, poly sequencer and gated sequencer per layer; on the MPC's tempo, locked to
// its bar while it plays.
//
// Parameters (their ids never change; new ones only at the end):
//   0           VOLUME (the host nudges parameter 0 on every prepare: harmless here, it ramps)
//   1..136      layer A's 136 parameters, in dsp/Params.h's order ("A Cutoff", ...)
//   137..272    layer B's
//   273, 274    LAYER MODE (single, stack, split), SPLIT POINT
//   275         VOICES (8 or 16)
//   276         QUALITY (ECO: the filters at 2x; HIGH: at 4x)
//   277         HOLD
// The programs' sequences and chord memories ride in the state (saveExtraState); a program change brings its own.
//
// MIDI in: notes and velocity, pitch bend, channel and poly pressure, CC 1 mod wheel, CC 2 breath, CC 4 foot,
// CC 11 expression, CC 64 sustain, CC 120 all sound off, CC 121 reset controllers, CC 123 all notes off.

#include "mpc_plugin.h"
#include "Engine.h"
#include "Presets.h"
#include "reeves_names.h"

#include <cmath>
#include <cstring>

namespace {
using namespace rv;

enum {
    kVolume = 0,
    kA = 1,
    kB = kA + NLAYER,
    kLayerMode = kB + NLAYER,
    kSplitPoint,
    kVoices,
    kQuality,
    kHold,
    kNumParams
};
static_assert(kNumParams == 278, "the parameters' order is permanent");
static_assert(sizeof(SHORT) / sizeof(SHORT[0]) == NLAYER, "a short name for each of a layer's parameters");

const char *const NOTE12[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

class ReevesMpc : public mpc::Plugin
{
public:
    ReevesMpc() : mpc::Plugin(makeInfo())
    {
        addParam(mpc::floatParam("vol", "Volume", 0.0f, 1.0f, 0.7f, "", 2));
        for (int l = 0; l < 2; l++)
            for (int p = 0; p < NLAYER; p++) {
                const ParamDef &d = LAYER[p];
                std::string id = std::string(l ? "b_" : "a_") + d.id, name = std::string(l ? "B " : "A ") + SHORT[p];
                if (d.kind == K_CHOICE || d.kind == K_ONOFF) {
                    std::vector<std::string> ch;
                    for (int k = d.lo; k <= d.hi; k++)
                        ch.push_back(d.choice[k]);
                    addParam(mpc::choiceParam(id.c_str(), name.c_str(), ch, d.def - d.lo));
                } else {
                    addParam(mpc::intParam(id.c_str(), name.c_str(), d.lo, d.hi, d.def));
                }
            }
        addParam(mpc::choiceParam("layer_mode", "Layer Mode", {"Single", "Stack A+B", "Split A|B"}, 0));
        addParam(mpc::intParam("split_point", "Split Point", 0, 127, 60));
#if defined(__arm__)                     /* (the 32-bit MPCs: 8 voices and ECO to start with) */
        addParam(mpc::choiceParam("voices", "Voices", {"8 Voice", "16 Voice"}, 0));
#else
        addParam(mpc::choiceParam("voices", "Voices", {"8 Voice", "16 Voice"}, 1));
#endif
        addParam(mpc::choiceParam("quality", "Quality", {"ECO", "HIGH"}, 0));
        addParam(mpc::boolParam("hold", "Hold", false));

        /* the factory programs: VOLUME and the sound; VOICES, QUALITY and HOLD they leave as they are */
        for (int k = 0; k < NFACTORY; k++) {
            std::vector<float> v;
            v.push_back(0.7f);
            for (int l = 0; l < 2; l++)
                for (int p = 0; p < NLAYER; p++) {
                    int val = FACTORY[k].layer[l][p];
                    if (p == L_SEQ_ON && FACTORY[k].layer[l][L_SEQ_TYPE] == ST_POLY)
                        val = 0;                 /* (the poly sequencer waits for PLAY) */
                    v.push_back((float)val);
                }
            v.push_back((float)FACTORY[k].prog[G_LAYER_MODE]);
            v.push_back((float)FACTORY[k].prog[G_SPLIT_POINT]);
            addProgram(FACTORY[k].name, v);
        }
        engine.prepare(44100.0);         // (the MPC's rate: made here, on the UI thread, never in process)
        rate = 44100.0;
        cur = FACTORY[0];
        pendingProgram = 0;
    }

    void prepare(double sampleRate, int) override
    {
        if (sampleRate != rate) {        // (only if the rate really changes: the MPC's is fixed at 44.1 kHz)
            engine.prepare(sampleRate);
            rate = sampleRate;
        }
        forceSend();
    }

    /* resume, or a program / state loaded (on the audio thread): the voices let go; the program's sequences and
     * chords (a state's own, else the factory program's when the program changed) */
    void reset() override
    {
        engine.panic();
        if (stagedReady.exchange(false)) {
            std::memcpy(cur.seq, staged.seq, sizeof cur.seq);
            std::memcpy(cur.chord, staged.chord, sizeof cur.chord);
            std::memcpy(cur.name, staged.name, sizeof cur.name);
            lastProgram = currentProgram();
        } else if (currentProgram() != lastProgram) {
            int k = currentProgram();
            if (k >= 0 && k < NFACTORY) {
                std::memcpy(cur.seq, FACTORY[k].seq, sizeof cur.seq);
                std::memcpy(cur.chord, FACTORY[k].chord, sizeof cur.chord);
                std::memcpy(cur.name, FACTORY[k].name, sizeof cur.name);
            }
            lastProgram = k;
        }
        forceSend();
    }

    void process(mpc::ProcessContext &ctx) override
    {
        const int n = ctx.numFrames;
        float *L = ctx.outputs[0], *R = ctx.numOutputs > 1 ? ctx.outputs[1] : ctx.outputs[0];
        sync();
        /* the MPC's tempo, and its position while it plays */
        double bpm = ctx.transport.valid && ctx.transport.tempoValid ? ctx.transport.bpm : 120.0;
        bool playing = ctx.transport.valid && ctx.transport.playing && ctx.transport.ppqValid;
        engine.host(playing, playing ? ctx.transport.ppqPos : 0.0, bpm, true);
        /* the block rendered up to each MIDI event (the engine lands it on its 16-sample grid) */
        int done = 0;
        for (int i = 0; i < ctx.numMidiEvents; i++) {
            const mpc::MidiEvent &e = ctx.midi[i];
            int at = mpc::clamp(e.sampleOffset, 0, n);
            if (at > done) {
                engine.render(L + done, R + done, at - done);
                done = at;
            }
            midi(e);
        }
        if (n > done)
            engine.render(L + done, R + done, n - done);
        /* VOLUME, a ramp over the block (0.7: about 0 dB) */
        float v = (float)get(kVolume), g = v * v * 2.0f;
        if (lastGain < 0.0f)
            lastGain = g;
        float step = (g - lastGain) / (float)(n > 0 ? n : 1), gg = lastGain;
        for (int s = 0; s < n; s++) {
            gg += step;
            L[s] *= gg;
            if (R != L)
                R[s] *= gg;
        }
        lastGain = g;
    }

    bool formatValue(int index, float plain, std::string &out) override
    {
        int v = (int)std::lround(plain);
        char t[40];
        if (index == kVolume) {
            if (plain <= 0.0001f)
                out = "-inf dB";
            else {
                std::snprintf(t, sizeof t, "%.1f dB", 20.0 * std::log10(plain * plain * 2.0));
                out = t;
            }
            return true;
        }
        if (index >= kA && index < kLayerMode) {
            int l = index >= kB, p = index - (l ? kB : kA);
            const ParamDef &d = LAYER[p];
            if (d.kind == K_CHOICE || d.kind == K_ONOFF)
                v += d.lo;               /* (a choice's plain value is its index from 0) */
            v = mpc::clamp(v, d.lo, d.hi);
            bool synced = false;
            int base = l ? kB : kA;
            if (p >= L_LFO1 && p < L_LFO_END && (p - L_LFO1) % LFO_N == LFO_FREQ)
                synced = getInt(base + p - LFO_FREQ + LFO_CLK) != 0;
            if (p == L_FX_P1)
                synced = getInt(base + L_FX_CLK) != 0;
            paramText(p, v, t, sizeof t, synced, getInt(base + L_FX_TYPE));
            out = t;
            if (out.size() > 12)
                out.resize(12);
            return true;
        }
        if (index == kSplitPoint) {
            v = mpc::clamp(v, 0, 127);
            out = std::string(NOTE12[v % 12]) + std::to_string(v / 12 - 2);
            return true;
        }
        return false;
    }

    /* the sequences, the chords and the name, in the state */
    void saveExtraState(std::vector<uint8_t> &out) override
    {
        const uint8_t magic[4] = {'R', 'V', 'S', '1'};
        out.insert(out.end(), magic, magic + 4);
        auto put = [&](const void *p, size_t n) { out.insert(out.end(), (const uint8_t *)p, (const uint8_t *)p + n); };
        put(cur.seq, sizeof cur.seq);
        put(cur.chord, sizeof cur.chord);
        put(cur.name, sizeof cur.name);
    }
    void loadExtraState(const uint8_t *data, size_t size) override
    {
        if (size != 4 + sizeof cur.seq + sizeof cur.chord + sizeof cur.name || std::memcmp(data, "RVS1", 4) != 0)
            return;
        const uint8_t *p = data + 4;
        std::memcpy(staged.seq, p, sizeof staged.seq);
        p += sizeof staged.seq;
        std::memcpy(staged.chord, p, sizeof staged.chord);
        p += sizeof staged.chord;
        std::memcpy(staged.name, p, sizeof staged.name);
        staged.name[NAME_LEN - 1] = 0;
        for (auto &s : staged.seq)
            if (s.length > PSEQ_STEPS)
                s.length = PSEQ_STEPS;
        stagedReady.store(true);         /* (taken in on the audio thread: reset()) */
    }

private:
    static mpc::PluginInfo makeInfo()
    {
        mpc::PluginInfo i;
        i.name = "REEVES";
        i.vendor = "swtdr34mz";
        i.uniqueId = MPC_FOURCC('R', 'v', 's', '2');
        i.version = 1000;
        i.isInstrument = true;
        i.numInputs = 0;
        i.numOutputs = 2;
        i.tailSamples = 44100 * 10;
        return i;
    }

    void forceSend()
    {
        programSent = false;
        lastGain = -1.0f;
    }

    int layerValue(int l, int p) const
    {
        const ParamDef &d = LAYER[p];
        int v = getInt((l ? kB : kA) + p);
        if (d.kind == K_CHOICE || d.kind == K_ONOFF)
            v += d.lo;
        return mpc::clamp(v, d.lo, d.hi);
    }

    /* the parameters to the engine, when they change (atomics: safe here) */
    void sync()
    {
        if (!programSent) {              /* (a whole program: the sequences and chords with the parameters) */
            for (int l = 0; l < 2; l++)
                for (int p = 0; p < NLAYER; p++)
                    cur.layer[l][p] = (int16_t)layerValue(l, p);
            cur.prog[G_LAYER_MODE] = (int16_t)mpc::clamp(getInt(kLayerMode), 0, 2);
            cur.prog[G_SPLIT_POINT] = (int16_t)mpc::clamp(getInt(kSplitPoint), 0, 127);
            engine.setProgram(cur);
            for (int l = 0; l < 2; l++)
                for (int p = 0; p < NLAYER; p++)
                    sent[l][p] = engine.program().layer[l][p];
            sentMode = cur.prog[G_LAYER_MODE];
            sentSplit = cur.prog[G_SPLIT_POINT];
            programSent = true;
        }
        for (int l = 0; l < 2; l++)
            for (int p = 0; p < NLAYER; p++) {
                int v = layerValue(l, p);
                if (v != sent[l][p]) {
                    if (p == L_SEQ_ON && engine.program().layer[l][L_SEQ_TYPE] == ST_POLY)
                        engine.seqPlay(l, v != 0);
                    engine.setParam(l, p, v);
                    sent[l][p] = (int16_t)v;
                    cur.layer[l][p] = (int16_t)v;
                }
            }
        int mode = mpc::clamp(getInt(kLayerMode), 0, 2), split = mpc::clamp(getInt(kSplitPoint), 0, 127);
        if (mode != sentMode) {
            engine.setProgramParam(G_LAYER_MODE, mode);
            sentMode = mode;
        }
        if (split != sentSplit) {
            engine.setProgramParam(G_SPLIT_POINT, split);
            sentSplit = split;
        }
        engine.setVoices(getInt(kVoices) ? 16 : 8);
        engine.setQuality(getInt(kQuality));
        bool hold = getBool(kHold);
        if (hold != holdSent) {
            engine.hold(hold);
            holdSent = hold;
        }
    }

    void midi(const mpc::MidiEvent &e)
    {
        const uint8_t st = e.status();
        if (e.isNoteOn())
            engine.noteOn(e.note(), (float)e.velocity() / 127.0f);
        else if (e.isNoteOff())
            engine.noteOff(e.note());
        else if (e.isPitchBend())
            engine.bend((float)e.pitchBend() / 8192.0f);
        else if (st == 0xA0)             /* poly pressure: as the channel's */
            engine.pressure((float)e.data[2] / 127.0f);
        else if (e.isAftertouch())
            engine.pressure((float)e.data[1] / 127.0f);
        else if (e.isController()) {
            int cc = e.controller();
            float v = (float)e.controllerValue() / 127.0f;
            switch (cc) {
            case 1: engine.modWheel(v); break;
            case 2: engine.breath(v); break;
            case 4: engine.foot(v); break;
            case 11: engine.expression(v); break;
            case 64: engine.sustain(e.controllerValue() >= 64); break;
            case 120: engine.panic(); break;
            case 123: engine.allOff(); break;
            case 121:
                engine.bend(0.0f);
                engine.modWheel(0.0f);
                engine.pressure(0.0f);
                engine.sustain(false);
                break;
            default: break;
            }
        }
    }

    Engine engine;
    double rate = 0.0;
    Program cur, staged;
    std::atomic<bool> stagedReady{false};
    int lastProgram = 0, pendingProgram = 0;
    int16_t sent[2][NLAYER] = {};
    int sentMode = -1, sentSplit = -1;
    bool programSent = false, holdSent = false;
    float lastGain = -1.0f;
};

} // namespace

MPC_PLUGIN_ENTRY(ReevesMpc)
