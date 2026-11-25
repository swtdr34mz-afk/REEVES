/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (REEVES) */
/* A picture of the editor (a PNG), without a window: for checking the panel's drawing.
 *   ReevesShot out.png [scale] [program] [layer B: 0/1] [display page] [line] */
#include "PluginProcessor.h"
#include "PluginEditor.h"

int main(int argc, char **argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    juce::String out = argc > 1 ? argv[1] : "shot.png";
    float scale = argc > 2 ? (float)std::atof(argv[2]) : 1.0f;
    int prog = argc > 3 ? std::atoi(argv[3]) : 0;
    ReevesProcessor proc;
    proc.setCurrentProgram(prog);
    if (argc > 4)
        proc.editLayer = std::atoi(argv[4]);
    proc.uiScale = 1.0f;
    {
        /* the processor as a host drives it: prepare, blocks with notes, its state out and back in */
        proc.setPlayConfigDetails(0, 2, 48000.0, 512);
        proc.prepareToPlay(48000.0, 512);
        juce::AudioBuffer<float> buf(2, 512);
        float peak = 0.0f;
        for (int b = 0; b < 200; b++) {
            juce::MidiBuffer m;
            if (b == 2)
                m.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8)100), 10);
            if (b == 150)
                m.addEvent(juce::MidiMessage::noteOff(1, 60), 0);
            buf.clear();
            proc.processBlock(buf, m);
            peak = juce::jmax(peak, buf.getMagnitude(0, 512));
        }
        juce::MemoryBlock st;
        proc.getStateInformation(st);
        proc.setStateInformation(st.getData(), (int)st.getSize());
        std::printf("processed: peak %.3f, state %d bytes\n", peak, (int)st.getSize());
    }
    std::unique_ptr<juce::AudioProcessorEditor> ed(proc.createEditor());
    ed->setSize(ReevesPanel::W, ReevesPanel::H);
    if (argc > 5)
        if (auto *panel = dynamic_cast<ReevesPanel *>(ed->getChildComponent(0)))
            panel->openPage((ReevesPanel::Page)std::atoi(argv[5]), argc > 6 ? std::atoi(argv[6]) : 0);
    for (auto *c : ed->getChildren())             /* (the live parts: their state, as the timer would) */
        for (auto *k : c->getChildren())
            if (auto *lv = dynamic_cast<ReevesPanel::Live *>(k))
                lv->poll();
    auto img = ed->createComponentSnapshot(ed->getLocalBounds(), true, scale);
    juce::File f = juce::File::getCurrentWorkingDirectory().getChildFile(out);
    f.deleteFile();
    juce::FileOutputStream os(f);
    juce::PNGImageFormat().writeImageToStream(img, os);
    std::printf("wrote %s (%d x %d)\n", f.getFullPathName().toRawUTF8(), img.getWidth(), img.getHeight());
    ed.reset();
    return 0;
}
