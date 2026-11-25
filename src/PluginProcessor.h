/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (REEVES) */
/* REEVES as a plugin (VST3, AU on the Mac) and a standalone app: the engine (dsp/Engine.h), both layers' parameters
 * for the host (automation, saved with the project), the factory programs and the user's own as the host's programs,
 * MIDI in (notes, velocity, pressure, pitch bend, mod wheel, breath, foot, expression, sustain), the host's tempo and
 * position for the clocks. The panel's buttons that act on the engine (PLAY, RECORD, HOLD, ...) come through a small
 * queue; what the engine changes itself (a recorded sequence, a chord memory) comes back for the editor and the
 * project. */
#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <atomic>
#include "Engine.h"
#include "Presets.h"

class ReevesProcessor : public juce::AudioProcessor
{
public:
    ReevesProcessor();
    ~ReevesProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout &layouts) const override;
    void processBlock(juce::AudioBuffer<float> &, juce::MidiBuffer &) override;

    juce::AudioProcessorEditor *createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "REEVES"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 10.0; }

    int getNumPrograms() override { return rv::NFACTORY + (int)userPrograms.size(); }
    int getCurrentProgram() override { return program; }
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int, const juce::String &) override {}
    void getStateInformation(juce::MemoryBlock &destData) override;
    void setStateInformation(const void *data, int sizeInBytes) override;

    /* the parameters: a layer's ("a_cutoff", "b_cutoff"), the program's, the plugin's */
    static juce::String layerId(int layer, int p) { return juce::String(layer ? "b_" : "a_") + rv::LAYER[p].id; }
    juce::AudioProcessorValueTreeState apvts;
    juce::RangedAudioParameter *lp(int layer, int p) const { return lparam[layer][p]; }
    int value(int layer, int p) const;          /* a layer's parameter now (its integer) */
    int progValue(int p) const;
    void setValue(int layer, int p, int v);     /* (from the editor: a gesture, the host told) */
    void setProgValue(int p, int v);

    /* the panel's commands to the engine (any thread; done at the next block) */
    enum Cmd { C_PLAY, C_RECORD, C_REST, C_TIE, C_BACK, C_HOLD, C_CHORD, C_GSTEP, C_PANIC };
    void command(Cmd c, int layer = 0, int a = 0, int b = 0, int v = 0);

    /* what the engine shows (the editor's LEDs and display) */
    std::atomic<float> voiceLevel[rv::MAXV] = {};
    std::atomic<int> voiceLayer[rv::MAXV] = {};
    std::atomic<int> seqStepNow[2] = {};
    std::atomic<int> recStepNow[2] = {};
    std::atomic<bool> recording[2] = {};
    std::atomic<bool> beatLed[2] = {};
    std::atomic<float> bpmNow{120.0f};
    std::atomic<float> lfoNow[2][4] = {};
    std::atomic<bool> holdNow{false};
    juce::MidiKeyboardState keyboardState;
    juce::MidiMessageCollector uiMidi;          /* the editor's wheels */
    std::atomic<float> uiScale{0.9f};
    std::atomic<bool> uiScaleSet{false};         /* (a size chosen: else the window opens as large as the screen allows) */
    std::atomic<int> editLayer{0};              /* EDIT LAYER B: the panel shows layer B */

    /* the program's name and its sequences and chord memories (the engine's live copy) */
    juce::String programName() const;
    void setProgramName(const juce::String &);
    rv::SeqData seqCopy(int layer) const;
    std::array<int8_t, 16> chordCopy(int layer) const;

    /* the user's programs (WRITE): files in Documents/REEVES/Programs, after the factory ones; those in a folder
     * there (an imported bank) keep its name as their group */
    std::vector<rv::Program> userPrograms;
    std::vector<juce::String> userGroup;
    static juce::File programFolder();
    void loadUserPrograms();
    int writeProgram(const juce::String &name);
    /* a Rev2's SysEx file (.syx): each program in it kept as one of the user's, in a folder named after the file;
     * the number of programs (0: none in it) */
    int importRev2(const juce::File &syx);
    rv::Program currentProgram() const;         /* the sound as it is now, whole */
    bool compare = false;                       /* COMPARE: the stored program heard, the edit kept aside */
    void setCompare(bool on);

private:
    rv::Engine engine;
    juce::RangedAudioParameter *lparam[2][rv::NLAYER] = {};
    juce::RangedAudioParameter *pparam[rv::NPROGRAM] = {};
    int16_t sentL[2][rv::NLAYER] = {}, sentP[rv::NPROGRAM] = {};
    int sentVoices = -1;
    bool started = false;
    int program = 0;
    float lastGain = -1.0f;
    int transpose = 0;
    std::atomic<bool> panicPending{false};

    /* a whole program for the engine (a program change, a project opened): taken at the next block */
    mutable juce::SpinLock progLock;
    rv::Program pending, live;                  /* (live: the engine's sequences and chords, for the editor and saves) */
    std::atomic<bool> pendingSet{false};
    rv::Program editKept;                       /* COMPARE's: the edit, while the stored one plays */

    struct Command {
        int c, layer, a, b, v;
    };
    juce::AbstractFifo fifo{256};
    std::array<Command, 256> cmds;

    void handleMidi(const juce::MidiMessage &m);
    void applyProgram(const rv::Program &p, bool toHost);
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    static void programToXml(const rv::Program &p, juce::XmlElement &x);
    static bool programFromXml(const juce::XmlElement &x, rv::Program &p);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ReevesProcessor)
};
