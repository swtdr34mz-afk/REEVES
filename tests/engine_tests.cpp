/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (REEVES) */
/* REEVES' engine tests, without JUCE: every factory program at 44.1, 48 and 96 kHz (no NaN, no runaway, sound
 * where there should be, silence after), every effect type, both filters, unison, the arp, both sequencers, any
 * buffer size the same, 8 and 16 voices, the CPU. A failure exits non-zero. */
#include "Engine.h"
#include "Presets.h"
#include <chrono>
#include <cmath>
#include <cstdio>
#include <vector>

using namespace rv;
static int fails = 0;
#define CHECK(c, ...)                                                                                                   \
    do {                                                                                                                \
        if (!(c)) {                                                                                                     \
            std::printf("FAIL: " __VA_ARGS__);                                                                          \
            std::printf("\n");                                                                                          \
            fails++;                                                                                                    \
        }                                                                                                               \
    } while (0)

struct Stats {
    float peak = 0, rms = 0;
    bool nan = false;
};
static Stats run(Engine &e, int frames, int block = 256)
{
    std::vector<float> L(block), R(block);
    Stats s;
    double sum = 0;
    int done = 0;
    while (done < frames) {
        int n = std::min(block, frames - done);
        e.render(L.data(), R.data(), n);
        for (int i = 0; i < n; i++) {
            if (!std::isfinite(L[i]) || !std::isfinite(R[i]))
                s.nan = true;
            s.peak = std::max(s.peak, std::max(std::fabs(L[i]), std::fabs(R[i])));
            sum += L[i] * L[i] + R[i] * R[i];
        }
        done += n;
    }
    s.rms = (float)std::sqrt(sum / (2.0 * frames));
    return s;
}

static void chord(Engine &e, bool on)
{
    const int notes[4] = {48, 55, 60, 64};
    for (int n : notes)
        on ? e.noteOn(n, 0.8f) : e.noteOff(n);
}

int main()
{
    /* every factory program, three rates */
    const double rates[3] = {44100.0, 48000.0, 96000.0};
    for (double sr : rates)
        for (int p = 0; p < NFACTORY; p++) {
            Engine e;
            e.prepare(sr);
            e.setProgram(FACTORY[p]);
            e.host(false, 0.0, 120.0, false);
            chord(e, true);
            Stats a = run(e, (int)(sr * 1.5));
            chord(e, false);
            Stats b = run(e, (int)(sr * 1.0));
            CHECK(!a.nan && !b.nan, "%s @%g: NaN", FACTORY[p].name, sr);
            CHECK(a.peak < 4.0f, "%s @%g: peak %.2f", FACTORY[p].name, sr, a.peak);
            CHECK(a.rms > 0.003f, "%s @%g: too quiet (rms %.4f)", FACTORY[p].name, sr, a.rms);
            if (sr == 44100.0)
                std::printf("  %-20s peak %.2f rms %.3f\n", FACTORY[p].name, a.peak, a.rms);
        }
    /* every effect, the filter both ways, resonance at the top */
    for (int fx = 0; fx < NFXTYPE; fx++)
        for (int pole = 0; pole < 2; pole++) {
            Engine e;
            e.prepare(48000.0);
            Program p;
            programInit(p);
            p.layer[0][L_FX_ON] = 1;
            p.layer[0][L_FX_TYPE] = (int16_t)fx;
            p.layer[0][L_FX_MIX] = 127;
            p.layer[0][L_FX_P1] = 200;
            p.layer[0][L_FX_P2] = 127;
            p.layer[0][L_POLE4] = (int16_t)pole;
            p.layer[0][L_RESO] = 127;
            p.layer[0][L_CUTOFF] = 80;
            e.setProgram(p);
            chord(e, true);
            Stats a = run(e, 48000 * 2);
            chord(e, false);
            Stats b = run(e, 48000 * 3);
            CHECK(!a.nan && !b.nan, "fx %s pole %d: NaN", FXTYPE_NAME[fx], pole);
            CHECK(a.peak < 6.0f && b.peak < 6.0f, "fx %s pole %d: peak %.2f / %.2f", FXTYPE_NAME[fx], pole, a.peak, b.peak);
        }
    /* the 4-pole sings at full resonance with nothing in */
    {
        Engine e;
        e.prepare(44100.0);
        Program p;
        programInit(p);
        p.layer[0][L_O1_SHAPE] = p.layer[0][L_O2_SHAPE] = SH_OFF;
        p.layer[0][L_RESO] = 127;
        p.layer[0][L_CUTOFF] = 90;
        p.layer[0][L_KEYAMT] = 64;
        p.layer[0][L_NOISE] = 2;
        e.setProgram(p);
        e.noteOn(60, 1.0f);
        Stats a = run(e, 44100 * 2);
        std::printf("  self-oscillation: peak %.2f rms %.3f\n", a.peak, a.rms);
        CHECK(a.rms > 0.02f, "the 4-pole does not self-oscillate (rms %.4f)", a.rms);
    }
    /* any buffer size: the same samples */
    {
        Engine a, b;
        a.prepare(44100.0);
        b.prepare(44100.0);
        a.setProgram(FACTORY[1]);
        b.setProgram(FACTORY[1]);
        chord(a, true);
        chord(b, true);
        std::vector<float> L1(44100), R1(44100), L2(44100), R2(44100);
        a.render(L1.data(), R1.data(), 44100);
        int done = 0, sizes[5] = {1, 7, 64, 333, 1000}, k = 0;
        while (done < 44100) {
            int n = std::min(sizes[k++ % 5], 44100 - done);
            b.render(L2.data() + done, R2.data() + done, n);
            done += n;
        }
        float diff = 0;
        for (int i = 0; i < 44100; i++)
            diff = std::max(diff, std::fabs(L1[i] - L2[i]));
        CHECK(diff < 1e-6f, "the buffer size changes the sound (%.3g)", diff);
    }
    /* unison 16, chord memory, the arp, both sequencers, stack and split, 8 voices */
    {
        Engine e;
        e.prepare(44100.0);
        Program p;
        programInit(p);
        p.layer[0][L_UNISON] = 1;
        p.layer[0][L_UNI_MODE] = 16;
        p.layer[0][L_UNI_DETUNE] = 8;
        e.setProgram(p);
        e.noteOn(40, 1.0f);
        Stats a = run(e, 44100);
        CHECK(e.voicesSounding() == 16, "unison 16: %d voices", e.voicesSounding());
        CHECK(!a.nan && a.peak < 6.0f, "unison: peak %.2f", a.peak);
        e.setVoices(8);
        run(e, 4410);
        CHECK(e.voicesSounding() <= 8, "8 voices: %d sound", e.voicesSounding());
        e.noteOff(40);
        run(e, 44100);
    }
    {
        Engine e;
        e.prepare(44100.0);
        Program p;
        programInit(p);
        p.layer[0][L_ARP_ON] = 1;
        p.layer[0][L_ARP_RANGE] = 2;
        p.layer[0][L_ARP_MODE] = AM_UPDOWN;
        p.layer[0][L_AREL] = 5;
        e.setProgram(p);
        e.host(false, 0.0, 120.0, false);
        chord(e, true);
        Stats a = run(e, 44100 * 2);
        CHECK(a.rms > 0.005f && !a.nan, "arp: rms %.4f", a.rms);
        chord(e, false);
        run(e, 44100);
        CHECK(e.voicesSounding() == 0, "arp: %d voices after", e.voicesSounding());
    }
    {
        Engine e;
        e.prepare(44100.0);
        Program p;
        programInit(p);
        p.layer[0][L_SEQ_TYPE] = ST_POLY;
        p.layer[0][L_AREL] = 5;
        p.seq[0].length = 4;
        for (int s = 0; s < 4; s++) {
            p.seq[0].note[s][0] = (uint8_t)(48 + s * 3 + 2);
            p.seq[0].vel[s][0] = 100;
        }
        p.seq[0].note[2][0] = PN_TIE;
        e.setProgram(p);
        e.host(false, 0.0, 120.0, false);
        e.seqPlay(0, true);
        Stats a = run(e, 44100 * 2);
        CHECK(a.rms > 0.005f, "poly seq: rms %.4f", a.rms);
        e.seqPlay(0, false);
        run(e, 44100);
        CHECK(e.voicesSounding() == 0, "poly seq: %d voices after stop", e.voicesSounding());
        /* recording */
        e.seqRecord(0, true);
        e.noteOn(60, 0.9f);
        e.noteOn(64, 0.9f);
        e.noteOff(60);
        e.noteOff(64);
        e.seqRest(0);
        e.noteOn(67, 0.9f);
        e.noteOff(67);
        e.seqRecord(0, false);
        const SeqData &s = e.seqData(0);
        CHECK(s.length == 3, "recorded length %d", s.length);
        CHECK(s.note[0][0] == 62 && s.note[0][1] == 66 && s.note[2][0] == 69, "recorded notes");
    }
    {
        Engine e;
        e.prepare(44100.0);
        Program p;
        programInit(p);
        p.layer[0][L_SEQ_ON] = 1;
        p.layer[0][L_SEQ_TYPE] = ST_GATED;
        p.layer[0][L_SEQ_D1] = MD_OSC_FREQ;
        p.layer[0][L_SEQ_D2] = MD_SLEW;
        for (int s = 0; s < 16; s++) {
            p.seq[0].gated[0][s] = (uint8_t)((s * 5) % 24);
            p.seq[0].gated[1][s] = (uint8_t)(s & 1 ? 40 : 0);
        }
        p.seq[0].gated[0][12] = GSEQ_RESET;
        e.setProgram(p);
        e.noteOn(48, 1.0f);
        Stats a = run(e, 44100 * 2);
        CHECK(a.rms > 0.005f && !a.nan, "gated seq: rms %.4f", a.rms);
        e.noteOff(48);
        run(e, 44100);
    }
    {
        Engine e;
        e.prepare(44100.0);
        Program p = FACTORY[0];
        p.prog[G_LAYER_MODE] = LM_STACK;
        e.setProgram(p);
        chord(e, true);
        run(e, 4410);
        CHECK(e.voicesSounding() == 8, "stack: %d voices for 4 notes", e.voicesSounding());
        chord(e, false);
        p.prog[G_LAYER_MODE] = LM_SPLIT;
        e.setProgram(p);
        run(e, 44100 * 10);
        e.noteOn(40, 1.0f);
        e.noteOn(80, 1.0f);
        run(e, 4410);
        CHECK(e.voicesSounding() == 2, "split: %d voices", e.voicesSounding());
    }
    /* the CPU: 16 voices, both layers, effects, 10 s of audio */
    {
        Engine e;
        e.prepare(48000.0);
        Program p = FACTORY[0];
        p.prog[G_LAYER_MODE] = LM_STACK;
        p.layer[0][L_FX_ON] = p.layer[1][L_FX_ON] = 1;
        p.layer[0][L_FX_TYPE] = FX_REVERB;
        e.setProgram(p);
        for (int n = 0; n < 8; n++)
            e.noteOn(40 + n * 5, 0.9f);
        auto t0 = std::chrono::steady_clock::now();
        run(e, 48000 * 10);
        double s = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        std::printf("  CPU: 16 voices + 2 effects: %.1f %% of one core\n", s * 10.0);
    }
    std::printf(fails ? "%d FAILED\n" : "all passed\n", fails);
    return fails ? 1 : 0;
}
