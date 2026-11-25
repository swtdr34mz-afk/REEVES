/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (REEVES) */
/* The editor: a Prophet Rev2's front panel, drawn at 1860 x 570 and scaled to the window (drag the corner). Its two
 * rows of sections as on the Rev2 (MODULATION, CLOCK, ARP, SEQUENCER, the display with PARAMETER and VALUE,
 * PROGRAM and BANK, EFFECT, AUXILIARY ENVELOPE; LOW FREQUENCY OSCILLATORS, OSCILLATORS, LOW-PASS FILTER, AMPLIFIER),
 * the wheels, GLIDE, UNISON, HOLD, TRANSPOSE, the layer buttons, and REEVES' own VOICES (8 or 16) and HOST SYNC,
 * over a five-octave keyboard.
 *
 * The OLED works as the Rev2's: it shows what was touched last; MISC PARAMS, the modulation's SOURCE and
 * DESTINATION, the ARP's MODE, the sequencer's MODE, DESTINATION and RECORD and GLOBAL open their pages, PARAMETER
 * picks a line and VALUE sets it. With the mouse: the wheel over the display is VALUE, a click picks from a list, and
 * the gated sequencer's steps are drawn with the mouse. */
#pragma once
#include "PluginProcessor.h"
#include "ui/Look.h"

class ReevesPanel : public juce::Component, private juce::Timer
{
public:
    static constexpr int W = 1860, H = 570;
    explicit ReevesPanel(ReevesProcessor &);
    ~ReevesPanel() override;
    void paint(juce::Graphics &) override;

    ReevesProcessor &proc;
    int layer() const { return proc.editLayer.load(); }

    /* what the display shows */
    enum Page { P_HOME, P_MISC, P_MOD, P_ARP, P_GSEQ, P_STEPS, P_GLOBAL, P_POLYREC };
    struct Entry {
        juce::String label;
        std::function<int()> get;
        std::function<void(int)> set;
        int lo = 0, hi = 127;
        std::function<juce::String(int)> text;
    };
    Page page = P_HOME;
    int item = 0;
    std::vector<Entry> entries() const;
    void openPage(Page p, int at = 0);
    void parameterTurn(int d);
    void valueTurn(int d);
    void touch(int layerIdx, int p);                 /* a layer's parameter in hand */
    void touchGlobal(juce::RangedAudioParameter *);
    void entryMenu(juce::Rectangle<int> screen);    /* the current line's choices, as a menu */
    void programMenu(juce::Rectangle<int> screen);
    void writeDialog();
    juce::String valueText(int l, int p, int v) const;

    int lfoSel = 0, modSlot = 0, track = 0, step = 0;
    int shownLayer = 0, shownParam = -1;
    juce::RangedAudioParameter *shownGlobal = nullptr;
    juce::Component *oledView = nullptr;
    int shownVoices = 16;

    /* the hand-drawn parts poll what they show, and repaint when it changed */
    struct Live {
        virtual ~Live() = default;
        virtual void poll() = 0;
    };

private:
    void timerCallback() override;
    void build();
    void paintBackground(juce::Graphics &);

    rvui::Look look;
    std::vector<std::unique_ptr<juce::Component>> parts;
    std::vector<Live *> live;
    juce::Image background;
    float backgroundScale = 0.0f;
    std::vector<double> taps;
    template <class T, class... A> T *add(A &&...a);
    friend class Oled;
};

class ReevesEditor : public juce::AudioProcessorEditor
{
public:
    explicit ReevesEditor(ReevesProcessor &);
    void resized() override;

private:
    ReevesProcessor &proc;
    juce::SharedResourcePointer<rvui::UiResources> resources;   // (before the panel: freed after it)
    ReevesPanel panel;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ReevesEditor)
};
