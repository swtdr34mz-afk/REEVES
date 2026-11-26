/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (REEVES) */
/* LOW SPLIT on the MIDI notes (PROPHET 67's, KeySplit.h): below POINT the left hand plays SPLIT octaves lower; LOW HOLD
 * keeps the left hand's notes until a new one is played there (a bass line held while the right hand plays on). It
 * works before the engine, so both layers, the arp and the A|B split all see the notes it gives. */
#pragma once
#include "Engine.h"

namespace rv
{
class LowSplit
{
public:
    int split = 0, point = 60;           /* split: 0 off, 1..3 octaves down */
    bool lowHold = false;

    LowSplit() { forget(true); }

    /* the notes let go (ALL NOTES OFF and the like); alsoHeld: the keys' state too */
    void forget(bool alsoHeld)
    {
        for (int n = 0; n < 128; n++) {
            mapped[n] = -1;
            latched[n] = false;
            if (alsoHeld)
                held[n] = false;
        }
    }

    /* the settings for this block; LOW HOLD off lets go of what only it held */
    void set(Engine &e, int s, int p, bool hold)
    {
        bool was = lowHold;
        split = s;
        point = p;
        lowHold = hold;
        if (was && !lowHold)
            for (int n = 0; n < 128; n++)
                if (latched[n] && !held[n])
                    stop(e, n);
    }

    bool isLeft(int note) const { return split > 0 && note < point; }

    /* transpose: the plugin's own (octaves), after the split */
    void noteDown(Engine &e, int note, float vel, int transpose)
    {
        if (lowHold && isLeft(note)) {   /* a new note on the left, none held there: the held ones go */
            bool anyHeld = false, anyLatched = false;
            for (int n = 0; n < 128; n++) {
                anyHeld |= held[n] && isLeft(n);
                anyLatched |= latched[n];
            }
            if (!anyHeld && anyLatched)
                for (int n = 0; n < 128; n++)
                    if (latched[n])
                        stop(e, n);
        }
        if (mapped[note] >= 0)
            stop(e, note);
        held[note] = true;
        int m = note + 12 * transpose - (isLeft(note) ? 12 * split : 0);
        if (m < 0 || m > 127)
            return;
        mapped[note] = m;
        e.noteOn(m, vel);
        if (lowHold && isLeft(note))
            latched[note] = true;
    }

    void noteUp(Engine &e, int note)
    {
        held[note] = false;
        if (latched[note] && lowHold)
            return;                      /* LOW HOLD: it stays */
        stop(e, note);
    }

private:
    int mapped[128];                     /* the note each key plays (-1: none) */
    bool held[128] = {}, latched[128] = {};

    void stop(Engine &e, int note)
    {
        if (mapped[note] >= 0)
            e.noteOff(mapped[note]);
        mapped[note] = -1;
        latched[note] = false;
    }
};
} // namespace rv
