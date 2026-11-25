/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (REEVES) */
#include "Presets.h"
#include <cstdio>
#include <initializer_list>
#include <utility>

namespace rv
{
namespace
{
struct B {
    Program p;
    explicit B(const char *name)
    {
        programInit(p);
        std::snprintf(p.name, NAME_LEN, "%s", name);
    }
    /* set a layer's parameters (both layers: l = -1) */
    B &set(std::initializer_list<std::pair<int, int>> kv, int l = -1)
    {
        for (int k = 0; k < 2; k++)
            if (l < 0 || l == k)
                for (auto &e : kv)
                    p.layer[k][e.first] = (int16_t)e.second;
        return *this;
    }
    B &a(std::initializer_list<std::pair<int, int>> kv) { return set(kv, 0); }
    B &b(std::initializer_list<std::pair<int, int>> kv) { return set(kv, 1); }
    B &mode(int m, int split = 60)
    {
        p.prog[G_LAYER_MODE] = (int16_t)m;
        p.prog[G_SPLIT_POINT] = (int16_t)split;
        return *this;
    }
    B &lfo(int k, int freq, int shape, int amt, int dest, int clk = 0, int key = 0, int l = -1)
    {
        int b0 = L_LFO1 + k * LFO_N;
        return set({{b0 + LFO_FREQ, freq}, {b0 + LFO_SHAPE, shape}, {b0 + LFO_AMT, amt}, {b0 + LFO_DEST, dest},
                    {b0 + LFO_CLK, clk}, {b0 + LFO_KEY, key}},
                   l);
    }
    B &mod(int k, int src, int dst, int amt, int l = -1)
    {
        int b0 = L_MOD1 + k * MOD_N;
        return set({{b0 + MOD_SRC, src}, {b0 + MOD_DST, dst}, {b0 + MOD_AMT, amt}}, l);
    }
    /* a poly sequence: chords (up to six notes, 0 ends a chord; -1 a rest, -2 a tie) */
    B &poly(int l, std::initializer_list<std::initializer_list<int>> steps)
    {
        SeqData &s = p.seq[l];
        int i = 0;
        for (auto &st : steps) {
            int k = 0;
            for (int n : st) {
                if (k >= PSEQ_NOTES)
                    break;
                if (n == -1) {
                    s.note[i][k++] = PN_REST;
                } else if (n == -2) {
                    s.note[i][k++] = PN_TIE;
                } else {
                    s.note[i][k] = (uint8_t)(n + 2);
                    s.vel[i][k++] = 100;
                }
            }
            i++;
        }
        s.length = (uint8_t)i;
        p.layer[l][L_SEQ_TYPE] = ST_POLY;
        return *this;
    }
    B &gated(int l, int track, std::initializer_list<int> vals)
    {
        int i = 0;
        for (int v : vals)
            p.seq[l].gated[track][i++] = (uint8_t)v;
        if (i < GSEQ_STEPS)
            p.seq[l].gated[track][i] = GSEQ_RESET;
        return *this;
    }
    B &chord(int l, std::initializer_list<int> notes)
    {
        int i = 0;
        for (int n : notes)
            p.chord[l][i++] = (int8_t)n;
        return *this;
    }
    operator Program() const { return p; }
};

/* the shared starts */
B pad(const char *n)
{
    B b(n);
    b.set({{L_O1_SHAPE, SH_SAW}, {L_O2_SHAPE, SH_SAW}, {L_O2_FINE, 7}, {L_O1_FINE, -4}, {L_SLOP, 28}, {L_CUTOFF, 78},
           {L_RESO, 22}, {L_FENV_AMT, 38}, {L_FATT, 88}, {L_FDEC, 96}, {L_FSUS, 70}, {L_FREL, 96}, {L_AATT, 86},
           {L_ADEC, 90}, {L_ASUS, 120}, {L_AREL, 98}, {L_PAN_SPREAD, 90}, {L_KEYAMT, 32}, {L_FX_ON, 1},
           {L_FX_TYPE, FX_CHORUS}, {L_FX_MIX, 64}, {L_FX_P1, 40}, {L_FX_P2, 80}});
    b.lfo(0, 62, LS_TRI, 3, MD_OSC_FREQ);
    b.mod(0, MS_MODWHEEL, MD_LFO1_AMT, 30);
    return b;
}
B pluck(const char *n)
{
    B b(n);
    b.set({{L_O1_SHAPE, SH_SAWTRI}, {L_O2_SHAPE, SH_PULSE}, {L_O2_SHMOD, 30}, {L_O2_FREQ, 48}, {L_OSC_MIX, 40},
           {L_CUTOFF, 62}, {L_RESO, 30}, {L_FENV_AMT, 72}, {L_FDEC, 62}, {L_FSUS, 0}, {L_FREL, 58}, {L_FVEL, 50},
           {L_ADEC, 84}, {L_ASUS, 0}, {L_AREL, 70}, {L_AVEL, 60}, {L_KEYAMT, 50}, {L_PAN_SPREAD, 70}, {L_SLOP, 12}});
    return b;
}

Program make(int i)
{
    switch (i) {
    case 0: {
        B b = pad("Rev Pad");
        return b;
    }
    case 1: {
        B b("Brass Section");
        b.set({{L_O1_SHAPE, SH_SAW}, {L_O2_SHAPE, SH_SAW}, {L_O2_FINE, 9}, {L_SLOP, 24}, {L_CUTOFF, 58}, {L_RESO, 12},
               {L_FENV_AMT, 70}, {L_FATT, 52}, {L_FDEC, 78}, {L_FSUS, 56}, {L_FREL, 70}, {L_FVEL, 40}, {L_AATT, 40},
               {L_ADEC, 70}, {L_ASUS, 118}, {L_AREL, 72}, {L_KEYAMT, 40}, {L_PAN_SPREAD, 60}});
        b.lfo(0, 64, LS_TRI, 0, MD_OSC_FREQ).mod(0, MS_MODWHEEL, MD_LFO1_AMT, 12);
        b.set({{L_FX_ON, 1}, {L_FX_TYPE, FX_REVERB}, {L_FX_MIX, 28}, {L_FX_P1, 110}, {L_FX_P2, 70}});
        return b;
    }
    case 2: {
        B b("Prophet Strings");
        b.set({{L_O1_SHAPE, SH_SAW}, {L_O1_SHMOD, 30}, {L_O2_SHAPE, SH_PULSE}, {L_O2_SHMOD, 40}, {L_O2_FINE, 6},
               {L_OSC_MIX, 70}, {L_SLOP, 30}, {L_CUTOFF, 96}, {L_RESO, 10}, {L_FENV_AMT, 22}, {L_FATT, 80},
               {L_FDEC, 90}, {L_FSUS, 90}, {L_FREL, 90}, {L_AATT, 78}, {L_ASUS, 127}, {L_AREL, 92}, {L_KEYAMT, 48},
               {L_PAN_SPREAD, 110}, {L_FX_ON, 1}, {L_FX_TYPE, FX_CHORUS}, {L_FX_MIX, 80}, {L_FX_P1, 30}, {L_FX_P2, 100}});
        b.lfo(2, 40, LS_TRI, 22, MD_OSC_SHAPE).lfo(0, 64, LS_TRI, 2, MD_OSC_FREQ);
        return b;
    }
    case 3: {
        B b("Sync Lead");
        b.set({{L_O1_SHAPE, SH_SAW}, {L_O1_FREQ, 48}, {L_O2_SHAPE, SH_PULSE}, {L_O2_SHMOD, 50}, {L_SYNC, 1},
               {L_OSC_MIX, 30}, {L_CUTOFF, 120}, {L_RESO, 20}, {L_FENV_AMT, 20}, {L_FDEC, 70}, {L_FSUS, 60},
               {L_AREL, 40}, {L_E3_DEST, MD_OSC1_FREQ}, {L_E3_AMT, 70}, {L_E3DEC, 74}, {L_E3SUS, 30}, {L_UNISON, 1},
               {L_UNI_MODE, 3}, {L_UNI_DETUNE, 3}, {L_GLIDE, 1}, {L_O1_GLIDE, 50}, {L_O2_GLIDE, 50}, {L_GLIDE_MODE, GM_RATE_A},
               {L_KEY_MODE, KM_LAST}, {L_FX_ON, 1}, {L_FX_TYPE, FX_DDL_STEREO}, {L_FX_MIX, 36}, {L_FX_P1, 4},
               {L_FX_P2, 50}, {L_FX_CLK, 1}});
        b.lfo(0, 66, LS_TRI, 0, MD_OSC_FREQ).mod(0, MS_MODWHEEL, MD_LFO1_AMT, 14);
        return b;
    }
    case 4: {
        B b("Fat Unison Bass");
        b.set({{L_O1_SHAPE, SH_SAW}, {L_O2_SHAPE, SH_PULSE}, {L_O1_FREQ, 24}, {L_O2_FREQ, 24}, {L_SUB, 90},
               {L_CUTOFF, 40}, {L_RESO, 34}, {L_FENV_AMT, 66}, {L_FDEC, 66}, {L_FSUS, 10}, {L_FREL, 50}, {L_FVEL, 40},
               {L_AREL, 30}, {L_UNISON, 1}, {L_UNI_MODE, 6}, {L_UNI_DETUNE, 4}, {L_KEY_MODE, KM_LOW}, {L_SLOP, 18},
               {L_KEYAMT, 40}});
        return b;
    }
    case 5: {
        B b = pluck("Pluck Keys");
        b.set({{L_FX_ON, 1}, {L_FX_TYPE, FX_DDL_STEREO}, {L_FX_MIX, 40}, {L_FX_P1, 172}, {L_FX_P2, 56}, {L_FX_CLK, 1}});
        return b;
    }
    case 6: {
        B b("Glass Bell FM");                    /* the manual's recipe: audio mod into a ringing 4-pole */
        b.set({{L_O1_SHAPE, SH_TRI}, {L_O1_FREQ, 55}, {L_O2_SHAPE, SH_OFF}, {L_OSC_MIX, 127}, {L_AUDIOMOD, 127},
               {L_RESO, 127}, {L_CUTOFF, 96}, {L_KEYAMT, 64}, {L_FENV_AMT, 16}, {L_FDEC, 80}, {L_FSUS, 0},
               {L_ADEC, 104}, {L_ASUS, 0}, {L_AREL, 96}, {L_AVEL, 70}, {L_PAN_SPREAD, 90}, {L_FX_ON, 1},
               {L_FX_TYPE, FX_REVERB}, {L_FX_MIX, 46}, {L_FX_P1, 170}, {L_FX_P2, 100}, {L_PGM_VOL, 70}});
        return b;
    }
    case 7: {
        B b = pluck("Arp Machine");
        b.set({{L_ARP_ON, 1}, {L_ARP_MODE, AM_UP}, {L_ARP_RANGE, 2}, {L_DIVIDE, DV_16}, {L_FX_ON, 1},
               {L_FX_TYPE, FX_BBD}, {L_FX_MIX, 44}, {L_FX_P1, 172}, {L_FX_P2, 64}, {L_FX_CLK, 1}, {L_ADEC, 70}});
        b.lfo(1, 30, LS_TRI, 18, MD_CUTOFF);
        return b;
    }
    case 8: {
        B b = pluck("Poly Sequence");
        b.set({{L_DIVIDE, DV_8}, {L_FX_ON, 1}, {L_FX_TYPE, FX_DDL_STEREO}, {L_FX_MIX, 34},
               {L_FX_P1, 145}, {L_FX_P2, 50}, {L_FX_CLK, 1}});
        b.poly(0, {{57, 60, 64}, {-2}, {57, 60, 64}, {-1}, {55, 59, 62}, {-2}, {55, 59, 62}, {64},
                   {53, 57, 60}, {-2}, {53, 57, 60}, {-1}, {52, 55, 59}, {-2}, {52, 56, 59}, {71}});
        return b;
    }
    case 9: {
        B b("Gated Bassline");
        b.set({{L_O1_SHAPE, SH_SAW}, {L_O2_SHAPE, SH_PULSE}, {L_O2_FREQ, 24}, {L_SUB, 60}, {L_CUTOFF, 50},
               {L_RESO, 50}, {L_FENV_AMT, 60}, {L_FDEC, 56}, {L_FSUS, 0}, {L_ADEC, 64}, {L_ASUS, 40}, {L_AREL, 30},
               {L_SEQ_ON, 1}, {L_SEQ_TYPE, ST_GATED}, {L_SEQ_D1, MD_OSC_FREQ}, {L_SEQ_D3, MD_CUTOFF}, {L_DIVIDE, DV_16}});
        b.gated(0, 0, {0, 0, 12, 0, 7, 0, 10, 12, 0, 0, 15, 0, 7, 12, 3, 5});
        b.gated(0, 2, {10, 30, 5, 50, 20, 10, 60, 0, 30, 10, 40, 5, 70, 10, 30, 20});
        return b;
    }
    case 10: {
        B b("Brass over Pad");
        b.mode(LM_STACK);
        Program br = make(1), pd = make(0);
        for (int k = 0; k < NLAYER; k++) {
            b.p.layer[0][k] = br.layer[0][k];
            b.p.layer[1][k] = pd.layer[0][k];
        }
        b.b({{L_PGM_VOL, 80}});
        return b;
    }
    case 11: {
        B b("Bass | Lead Split");
        b.mode(LM_SPLIT, 60);
        Program bs = make(4), ld = make(3);
        for (int k = 0; k < NLAYER; k++) {
            b.p.layer[0][k] = bs.layer[0][k];
            b.p.layer[1][k] = ld.layer[0][k];
        }
        b.a({{L_UNI_MODE, 3}});
        return b;
    }
    case 12: {
        B b("Hollow 2-Pole");
        b.set({{L_O1_SHAPE, SH_PULSE}, {L_O1_SHMOD, 14}, {L_O2_SHAPE, SH_PULSE}, {L_O2_SHMOD, 86}, {L_O2_FINE, 5},
               {L_POLE4, 0}, {L_CUTOFF, 92}, {L_RESO, 96}, {L_FENV_AMT, 30}, {L_FDEC, 80}, {L_FSUS, 40}, {L_AATT, 20},
               {L_AREL, 70}, {L_PAN_SPREAD, 80}, {L_SLOP, 20}});
        b.lfo(1, 34, LS_TRI, 14, MD_CUTOFF);
        return b;
    }
    case 13: {
        B b = pad("Shimmer Space");
        b.set({{L_O1_SHAPE, SH_TRI}, {L_O2_FREQ, 48}, {L_O2_SHAPE, SH_SAWTRI}, {L_CUTOFF, 100}, {L_AATT, 96},
               {L_AREL, 106}, {L_FX_TYPE, FX_REVERB}, {L_FX_MIX, 70}, {L_FX_P1, 230}, {L_FX_P2, 110}});
        b.lfo(1, 20, LS_TRI, 20, MD_CUTOFF).lfo(3, 26, LS_TRI, 40, MD_PAN);
        return b;
    }
    case 14: {
        B b = pluck("Phase Keys");
        b.set({{L_O1_SHAPE, SH_TRI}, {L_O2_SHAPE, SH_PULSE}, {L_O2_FREQ, 36}, {L_ADEC, 96}, {L_AREL, 76},
               {L_FX_ON, 1}, {L_FX_TYPE, FX_PHASER_MST}, {L_FX_MIX, 90}, {L_FX_P1, 60}, {L_FX_P2, 90}});
        return b;
    }
    case 15: {
        B b("Ring Bells");
        b.set({{L_O1_SHAPE, SH_TRI}, {L_O2_SHAPE, SH_TRI}, {L_O2_FREQ, 55}, {L_CUTOFF, 130}, {L_ADEC, 100},
               {L_ASUS, 0}, {L_AREL, 96}, {L_FX_ON, 1}, {L_FX_TYPE, FX_RINGMOD}, {L_FX_MIX, 80}, {L_FX_P1, 166},
               {L_FX_P2, 127}, {L_PAN_SPREAD, 80}});
        return b;
    }
    case 16: {
        B b("Driven Lead");
        b.set({{L_O1_SHAPE, SH_SAW}, {L_O2_SHAPE, SH_SAW}, {L_O2_FINE, 12}, {L_CUTOFF, 84}, {L_RESO, 40},
               {L_FENV_AMT, 40}, {L_FDEC, 70}, {L_FSUS, 50}, {L_AREL, 46}, {L_UNISON, 1}, {L_UNI_MODE, 2},
               {L_UNI_DETUNE, 5}, {L_GLIDE, 1}, {L_O1_GLIDE, 46}, {L_O2_GLIDE, 46}, {L_FX_ON, 1},
               {L_FX_TYPE, FX_DISTORTION}, {L_FX_MIX, 100}, {L_FX_P1, 120}, {L_FX_P2, 70}, {L_PGM_VOL, 60}});
        b.lfo(0, 68, LS_TRI, 0, MD_OSC_FREQ).mod(0, MS_MODWHEEL, MD_LFO1_AMT, 16);
        return b;
    }
    case 17: {
        B b = pad("Flanged Sweep");
        b.set({{L_FX_TYPE, FX_FLANGER1}, {L_FX_MIX, 110}, {L_FX_P1, 30}, {L_FX_P2, 100}, {L_CUTOFF, 92}, {L_PGM_VOL, 80}});
        return b;
    }
    case 18: {
        B b("Chord Memory Stab");
        b.set({{L_O1_SHAPE, SH_SAW}, {L_O2_SHAPE, SH_SAW}, {L_O2_FINE, 10}, {L_CUTOFF, 70}, {L_RESO, 24},
               {L_FENV_AMT, 74}, {L_FDEC, 64}, {L_FSUS, 10}, {L_ADEC, 76}, {L_ASUS, 30}, {L_AREL, 50}, {L_UNISON, 1},
               {L_UNI_MODE, 17}, {L_PAN_SPREAD, 100}, {L_FX_ON, 1}, {L_FX_TYPE, FX_REVERB}, {L_FX_MIX, 30},
               {L_FX_P1, 120}, {L_FX_P2, 80}});
        b.chord(0, {0, 7, 12, 15, 19});
        b.chord(1, {0, 7, 12, 15, 19});
        return b;
    }
    case 19: {
        B b("Sub Drone");
        b.set({{L_O1_SHAPE, SH_SAW}, {L_O2_SHAPE, SH_SAW}, {L_O1_FREQ, 24}, {L_O2_FREQ, 31}, {L_SUB, 70},
               {L_VCA_LEVEL, 127}, {L_AENV_AMT, 0}, {L_CUTOFF, 56}, {L_RESO, 70}, {L_E3_DEST, MD_CUTOFF},
               {L_E3_AMT, 50}, {L_E3ATT, 96}, {L_E3DEC, 98}, {L_E3SUS, 0}, {L_E3_REPEAT, 1}, {L_SLOP, 60},
               {L_PGM_VOL, 60}, {L_FX_ON, 1}, {L_FX_TYPE, FX_REVERB}, {L_FX_MIX, 50}, {L_FX_P1, 200}, {L_FX_P2, 60}});
        b.lfo(3, 18, LS_TRI, 60, MD_PAN);
        return b;
    }
    case 20: {
        B b("Wobble");
        b.set({{L_O1_SHAPE, SH_SAW}, {L_O2_SHAPE, SH_PULSE}, {L_O1_FREQ, 24}, {L_O2_FREQ, 24}, {L_O2_FINE, 8},
               {L_SUB, 80}, {L_CUTOFF, 44}, {L_RESO, 60}, {L_AREL, 30}, {L_UNISON, 1}, {L_UNI_MODE, 4},
               {L_UNI_DETUNE, 3}, {L_FX_ON, 1}, {L_FX_TYPE, FX_DISTORTION}, {L_FX_MIX, 50}, {L_FX_P1, 70}, {L_FX_P2, 60},
               {L_PGM_VOL, 75}});
        b.lfo(1, 80, LS_TRI, 70, MD_CUTOFF, 1, 1);
        return b;
    }
    case 21: {
        B b("Noise Sweep");
        b.set({{L_O1_SHAPE, SH_OFF}, {L_O2_SHAPE, SH_OFF}, {L_NOISE, 127}, {L_CUTOFF, 40}, {L_RESO, 100},
               {L_FENV_AMT, 100}, {L_FATT, 96}, {L_FDEC, 100}, {L_FSUS, 0}, {L_AATT, 70}, {L_ADEC, 104}, {L_ASUS, 0},
               {L_AREL, 100}, {L_KEYAMT, 64}, {L_FX_ON, 1}, {L_FX_TYPE, FX_REVERB}, {L_FX_MIX, 60}, {L_FX_P1, 200},
               {L_FX_P2, 90}});
        return b;
    }
    case 22: {
        B b = pad("Soft Brass Pad");
        b.set({{L_CUTOFF, 66}, {L_FENV_AMT, 56}, {L_FATT, 74}, {L_AATT, 64}, {L_FX_TYPE, FX_REVERB}, {L_FX_MIX, 40},
               {L_FX_P1, 150}, {L_FX_P2, 80}});
        return b;
    }
    case 23: {
        B b("Clavi 2-Pole");
        b.set({{L_O1_SHAPE, SH_PULSE}, {L_O1_SHMOD, 20}, {L_O2_SHAPE, SH_PULSE}, {L_O2_SHMOD, 70}, {L_O2_FREQ, 48},
               {L_POLE4, 0}, {L_CUTOFF, 70}, {L_RESO, 60}, {L_FENV_AMT, 80}, {L_FDEC, 50}, {L_FSUS, 0}, {L_FVEL, 60},
               {L_ADEC, 66}, {L_ASUS, 0}, {L_AREL, 34}, {L_AVEL, 80}, {L_KEYAMT, 64}, {L_FX_ON, 1},
               {L_FX_TYPE, FX_PHASER_LO}, {L_FX_MIX, 60}, {L_FX_P1, 80}, {L_FX_P2, 60}});
        return b;
    }
    case 24: {
        B b("Pulse Lead");
        b.set({{L_O1_SHAPE, SH_PULSE}, {L_O1_SHMOD, 30}, {L_O2_SHAPE, SH_SAW}, {L_O2_FREQ, 48}, {L_O2_FINE, 4},
               {L_OSC_MIX, 50}, {L_CUTOFF, 96}, {L_RESO, 30}, {L_FENV_AMT, 30}, {L_FDEC, 70}, {L_FSUS, 60},
               {L_AREL, 44}, {L_GLIDE, 1}, {L_O1_GLIDE, 40}, {L_O2_GLIDE, 40}, {L_GLIDE_MODE, GM_RATE_A},
               {L_UNISON, 1}, {L_UNI_MODE, 1}, {L_KEY_MODE, KM_LAST}, {L_FX_ON, 1}, {L_FX_TYPE, FX_DELAY_MONO},
               {L_FX_MIX, 30}, {L_FX_P1, 120}, {L_FX_P2, 60}});
        b.lfo(2, 50, LS_TRI, 18, MD_OSC1_SHAPE).lfo(0, 68, LS_TRI, 0, MD_OSC_FREQ).mod(0, MS_MODWHEEL, MD_LFO1_AMT, 14);
        b.lfo(0, 68, LS_TRI, 0, MD_OSC_FREQ, 0, 0).set({{L_LFO1 + LFO_N * 0 + LFO_KEY, 0}});
        return b;
    }
    case 25: {
        B b = pad("Warm Strings");
        b.set({{L_O1_SHAPE, SH_SAW}, {L_O2_SHAPE, SH_SAW}, {L_O2_FREQ, 48}, {L_OSC_MIX, 50}, {L_CUTOFF, 92},
               {L_FENV_AMT, 16}, {L_AATT, 74}, {L_AREL, 90}, {L_FX_TYPE, FX_CHORUS}, {L_FX_MIX, 90}, {L_FX_P1, 50},
               {L_FX_P2, 110}});
        return b;
    }
    case 26: {
        B b("Repeat Texture");
        b.set({{L_O1_SHAPE, SH_SAWTRI}, {L_O2_SHAPE, SH_PULSE}, {L_O2_FREQ, 43}, {L_CUTOFF, 60}, {L_RESO, 70},
               {L_E3_DEST, MD_CUTOFF}, {L_E3_AMT, 80}, {L_E3ATT, 30}, {L_E3DEC, 64}, {L_E3SUS, 0}, {L_E3_REPEAT, 1},
               {L_AATT, 60}, {L_AREL, 90}, {L_PAN_SPREAD, 100}, {L_FX_ON, 1}, {L_FX_TYPE, FX_DDL_STEREO},
               {L_FX_MIX, 40}, {L_FX_P1, 150}, {L_FX_P2, 70}, {L_FX_CLK, 1}});
        b.mod(1, MS_LFO2, MD_ENV3_DEC, 30).lfo(1, 10, LS_TRI, 0, MD_OFF);
        return b;
    }
    case 27: {
        B b("Organ Gate");                       /* the manual's 'gated VCA': a square LFO on the amp */
        b.set({{L_O1_SHAPE, SH_PULSE}, {L_O1_SHMOD, 50}, {L_O2_SHAPE, SH_TRI}, {L_O2_FREQ, 48}, {L_SUB, 60},
               {L_CUTOFF, 110}, {L_AENV_AMT, 0}, {L_AREL, 20}, {L_FX_ON, 1}, {L_FX_TYPE, FX_CHORUS}, {L_FX_MIX, 60},
               {L_FX_P1, 90}, {L_FX_P2, 60}});
        b.lfo(0, 96, LS_SQUARE, 127, MD_VCA, 1, 1);
        b.set({{L_LFO1 + LFO_FREQ, 90}});
        return b;
    }
    case 28: {
        B b("Mono Bass");
        b.set({{L_O1_SHAPE, SH_SAW}, {L_O2_SHAPE, SH_PULSE}, {L_O1_FREQ, 24}, {L_O2_FREQ, 12}, {L_O2_SHMOD, 40},
               {L_CUTOFF, 50}, {L_RESO, 40}, {L_FENV_AMT, 62}, {L_FDEC, 60}, {L_FSUS, 20}, {L_FVEL, 40}, {L_ADEC, 70},
               {L_ASUS, 90}, {L_AREL, 24}, {L_UNISON, 1}, {L_UNI_MODE, 1}, {L_KEY_MODE, KM_LAST}, {L_GLIDE, 1},
               {L_O1_GLIDE, 30}, {L_O2_GLIDE, 30}, {L_GLIDE_MODE, GM_RATE_A}});
        return b;
    }
    case 29: {
        B b("Cinematic Swell");
        b.mode(LM_STACK);
        Program a = make(13), c = make(4);
        for (int k = 0; k < NLAYER; k++) {
            b.p.layer[0][k] = a.layer[0][k];
            b.p.layer[1][k] = c.layer[0][k];
        }
        b.b({{L_UNISON, 0}, {L_AATT, 90}, {L_AREL, 100}, {L_PGM_VOL, 70}, {L_O1_FREQ, 24}});
        return b;
    }
    case 30: {
        B b = pluck("Seq Dance");
        b.set({{L_O1_SHAPE, SH_SAW}, {L_O1_FREQ, 24}, {L_O2_FREQ, 24}, {L_O2_SHAPE, SH_SAW}, {L_O2_FINE, 10},
               {L_DIVIDE, DV_16}, {L_RESO, 56}, {L_FX_ON, 1}, {L_FX_TYPE, FX_DDL_STEREO},
               {L_FX_MIX, 24}, {L_FX_P1, 196}, {L_FX_P2, 40}, {L_FX_CLK, 1}});
        b.lfo(1, 6, LS_TRI, 30, MD_CUTOFF);
        b.poly(0, {{45}, {57}, {45}, {60}, {45}, {57}, {64}, {45}, {43}, {55}, {43}, {59}, {43}, {55}, {62}, {50}});
        return b;
    }
    default: return B("Basic Program");
    }
}
} // namespace

const Program FACTORY[NFACTORY] = {
    make(0),  make(1),  make(2),  make(3),  make(4),  make(5),  make(6),  make(7),  make(8),  make(9),  make(10),
    make(11), make(12), make(13), make(14), make(15), make(16), make(17), make(18), make(19), make(20), make(21),
    make(22), make(23), make(24), make(25), make(26), make(27), make(28), make(29), make(30), make(31)};
} // namespace rv
