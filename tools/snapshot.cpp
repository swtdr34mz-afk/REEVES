/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (REEVES) */
/* A picture of the editor (a PNG), without a window: for checking the panel's drawing.
 *   ReevesShot out.png [scale] [program] [layer B: 0/1] */
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
    std::unique_ptr<juce::AudioProcessorEditor> ed(proc.createEditor());
    ed->setSize(ReevesPanel::W, ReevesPanel::H);
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
