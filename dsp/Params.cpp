/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (REEVES) */
#include "Params.h"
#include <cstdio>
#include <cstring>

namespace rv
{
const char *const OSCSHAPE_NAME[NSHAPE] = {"Off", "Sawtooth", "Saw+Tri", "Triangle", "Pulse"};
const char *const LFOSHAPE_NAME[NLFOSHAPE] = {"Triangle", "Sawtooth", "Rev Saw", "Square", "Random"};
static const char *const ONOFF[] = {"Off", "On"};
static const char *const GLIDEMODE[] = {"Fixed Rate", "Fixed Rate A", "Fixed Time", "Fixed Time A"};
static const char *const KEYMODE[] = {"Low", "High", "Last", "Low Retrig", "High Retrig", "Last Retrig"};
static const char *const ARPMODE[] = {"Up", "Down", "Up+Down", "Random", "Assign"};
static const char *const SEQTYPE[] = {"Gated", "Poly"};
static const char *const GATEDMODE[] = {"Normal", "No Reset", "No Gate", "No Gate/Reset", "Key Step"};
static const char *const LAYERMODE[] = {"Single", "Stack A+B", "Split A|B"};
static const char *const PANMODE[] = {"Alternate", "Fixed"};
static const char *const EDITB[] = {"Layer A", "Layer B"};
const char *const FXTYPE_NAME[NFXTYPE] = {"Delay Mono", "DDL Stereo", "BBD Delay", "Chorus",   "Phaser High",
                                          "Phaser Low", "Phaser Mst", "Flanger 1", "Flanger 2", "Reverb",
                                          "Ring Mod",   "Distortion", "HP Filter"};
const char *const DIVIDE_NAME[NDIVIDE] = {"Half",   "Quarter",  "8th",      "8 Half", "8 Swing",  "8 Trip", "16th",
                                          "16 Half", "16 Swing", "16 Trip", "32nd",   "32 Trip", "64 Trip"};
/* a synced LFO's cycle (a step is a 16th) */
const char *const LFOSYNC_NAME[17] = {"32 Steps", "16 Steps", "8 Steps", "6 Steps", "4 Steps", "3 Steps",
                                      "2 Steps",  "1.5 Steps", "1 Step", "2/3 Step", "1/2 Step", "1/3 Step",
                                      "1/4 Step", "1/6 Step", "1/8 Step", "1/16 Step", ""};
const char *const DLYSYNC_NAME[11] = {"Whole",    "Half dot", "Half",       "Quarter dot", "Quarter", "Quarter trip",
                                      "8th dot",  "8th",      "8th trip",   "16th dot",    "16th"};
const char *const MODSRC_NAME[NMODSRC] = {"Off",       "Seq 1",     "Seq 2",      "Seq 3",      "Seq 4",
                                          "LFO 1",     "LFO 2",     "LFO 3",      "LFO 4",      "Env LPF",
                                          "Env VCA",   "Env 3",     "Pitchbend",  "Mod Wheel",  "Pressure",
                                          "Breath",    "Foot Pedal", "Expression", "Velocity",   "Note Number",
                                          "Noise",     "DC",        "Audio Out"};
const char *const MODDST_NAME[NMODDST + 1] = {
    "Off",          "Osc 1 Freq",   "Osc 2 Freq",   "Osc All Freq", "Osc 1 Level",  "Osc Mix",      "Noise Level",
    "Sub Osc Level", "Osc 1 Shape", "Osc 2 Shape",  "Osc All Shape", "Cutoff",      "Resonance",    "Audio Mod",
    "VCA",          "Pan",          "LFO 1 Freq",   "LFO 2 Freq",   "LFO 3 Freq",   "LFO 4 Freq",   "LFO All Freq",
    "LFO 1 Amount", "LFO 2 Amount", "LFO 3 Amount", "LFO 4 Amount", "LFO All Amt",  "LPF Env Amt",  "Amp Env Amt",
    "Env 3 Amount", "Env All Amt",  "LPF Attack",   "VCA Attack",   "Env 3 Attack", "Env All Att",  "LPF Decay",
    "VCA Decay",    "Env 3 Decay",  "Env All Decay", "LPF Release", "VCA Release",  "Env 3 Release", "Env All Rel",
    "Mod 1 Amount", "Mod 2 Amount", "Mod 3 Amount", "Mod 4 Amount", "Mod 5 Amount", "Mod 6 Amount", "Mod 7 Amount",
    "Mod 8 Amount", "Osc Slop",     "FX Mix",       "FX Param 1",   "FX Param 2",   "Slew"};

#define N(id, name, lo, hi, def) {id, name, lo, hi, def, K_NUM, nullptr}
#define S(id, name, lo, hi, def) {id, name, lo, hi, def, K_SIGNED, nullptr}
#define C(id, name, n, def, list) {id, name, 0, (n) - 1, def, K_CHOICE, list}
#define B(id, name, def) {id, name, 0, 1, def, K_ONOFF, ONOFF}
#define LFO(n, dest)                                                                                                    \
    {"lfo" #n "_freq", "LFO " #n " Frequency", 0, 150, 70, K_LFOFREQ, nullptr},                                         \
        C("lfo" #n "_shape", "LFO " #n " Shape", NLFOSHAPE, 0, LFOSHAPE_NAME),                                          \
        N("lfo" #n "_amt", "LFO " #n " Amount", 0, 127, 0), C("lfo" #n "_dest", "LFO " #n " Destination", NMODDST, dest, MODDST_NAME), \
        B("lfo" #n "_clk", "LFO " #n " Clock Sync", 0), B("lfo" #n "_key", "LFO " #n " Key Sync", 0)
#define MOD(n)                                                                                                          \
    C("mod" #n "_src", "Mod " #n " Source", NMODSRC, 0, MODSRC_NAME),                                                   \
        C("mod" #n "_dst", "Mod " #n " Destination", NMODDST, 0, MODDST_NAME),                                          \
        S("mod" #n "_amt", "Mod " #n " Amount", -127, 127, 0)

const ParamDef LAYER[NLAYER] = {
    {"o1_freq", "Osc 1 Freq", 0, 120, 36, K_FREQ, nullptr},
    S("o1_fine", "Osc 1 Fine Tune", -50, 50, 0),
    C("o1_shape", "Osc 1 Shape", NSHAPE, SH_SAW, OSCSHAPE_NAME),
    N("o1_shmod", "Osc 1 Shape Mod", 0, 99, 50),
    {"o2_freq", "Osc 2 Freq", 0, 120, 36, K_FREQ, nullptr},
    S("o2_fine", "Osc 2 Fine Tune", -50, 50, 0),
    C("o2_shape", "Osc 2 Shape", NSHAPE, SH_SAW, OSCSHAPE_NAME),
    N("o2_shmod", "Osc 2 Shape Mod", 0, 99, 50),
    B("sync", "Osc Sync", 0),
    N("sub", "Sub Octave", 0, 127, 0),
    N("osc_mix", "Osc Mix", 0, 127, 64),
    N("noise", "Noise", 0, 127, 0),
    N("slop", "Osc Slop", 0, 127, 0),
    B("o1_key", "Osc 1 Key On/Off", 1),
    B("o2_key", "Osc 2 Key On/Off", 1),
    B("o1_reset", "Osc 1 Note Reset", 0),
    B("o2_reset", "Osc 2 Note Reset", 0),
    B("glide", "Glide", 0),
    N("o1_glide", "Osc 1 Glide", 0, 127, 0),
    N("o2_glide", "Osc 2 Glide", 0, 127, 0),
    C("glide_mode", "Glide Mode", NGLIDEMODE, 0, GLIDEMODE),
    N("cutoff", "Cutoff", 0, 164, 164),
    N("reso", "Resonance", 0, 127, 0),
    S("fenv_amt", "LPF Env Amount", -127, 127, 0),
    N("fvel", "LPF Velocity", 0, 127, 0),
    N("keyamt", "LPF Key Amount", 0, 127, 0),
    N("audiomod", "Audio Mod", 0, 127, 0),
    B("pole4", "4 Pole", 1),
    N("fdel", "LPF Env Delay", 0, 127, 0),
    N("fatt", "LPF Env Attack", 0, 127, 0),
    N("fdec", "LPF Env Decay", 0, 127, 0),
    N("fsus", "LPF Env Sustain", 0, 127, 0),
    N("frel", "LPF Env Release", 0, 127, 0),
    N("pan_spread", "Pan Spread", 0, 127, 0),
    N("aenv_amt", "Amp Env Amount", 0, 127, 127),
    N("avel", "Amp Velocity", 0, 127, 0),
    N("adel", "Amp Env Delay", 0, 127, 0),
    N("aatt", "Amp Env Attack", 0, 127, 0),
    N("adec", "Amp Env Decay", 0, 127, 0),
    N("asus", "Amp Env Sustain", 0, 127, 127),
    N("arel", "Amp Env Release", 0, 127, 20),
    N("vca_level", "VCA Level", 0, 127, 0),
    C("e3_dest", "Env 3 Destination", NMODDST, 0, MODDST_NAME),
    S("e3_amt", "Env 3 Amount", -127, 127, 0),
    N("e3_vel", "Env 3 Velocity", 0, 127, 0),
    N("e3del", "Env 3 Delay", 0, 127, 0),
    N("e3att", "Env 3 Attack", 0, 127, 0),
    N("e3dec", "Env 3 Decay", 0, 127, 0),
    N("e3sus", "Env 3 Sustain", 0, 127, 0),
    N("e3rel", "Env 3 Release", 0, 127, 0),
    B("e3_repeat", "Env 3 Repeat", 0),
    LFO(1, MD_OSC_FREQ),
    LFO(2, MD_CUTOFF),
    LFO(3, MD_OSC_SHAPE),
    LFO(4, MD_PAN),
    MOD(1), MOD(2), MOD(3), MOD(4), MOD(5), MOD(6), MOD(7), MOD(8),
    S("mw_amt", "Mod Wheel Amount", -127, 127, 0),
    C("mw_dst", "Mod Wheel Destination", NMODDST, MD_LFO1_AMT, MODDST_NAME),
    S("at_amt", "Pressure Amount", -127, 127, 0),
    C("at_dst", "Pressure Destination", NMODDST, MD_CUTOFF, MODDST_NAME),
    S("br_amt", "Breath Amount", -127, 127, 0),
    C("br_dst", "Breath Destination", NMODDST, MD_CUTOFF, MODDST_NAME),
    S("vel_amt", "Velocity Amount", -127, 127, 0),
    C("vel_dst", "Velocity Destination", NMODDST, MD_OFF, MODDST_NAME),
    S("ft_amt", "Foot Amount", -127, 127, 0),
    C("ft_dst", "Foot Destination", NMODDST, MD_CUTOFF, MODDST_NAME),
    B("fx_on", "FX On/Off", 0),
    C("fx_type", "FX Select", NFXTYPE, FX_DDL_STEREO, FXTYPE_NAME),
    N("fx_mix", "FX Mix", 0, 127, 40),
    {"fx_p1", "FX Param 1", 0, 255, 100, K_FXP1, nullptr},
    N("fx_p2", "FX Param 2", 0, 127, 40),
    B("fx_clk", "FX Clock Sync", 0),
    N("bpm", "BPM", 30, 250, 120),
    C("divide", "Clock Divide", NDIVIDE, DV_16, DIVIDE_NAME),
    B("arp_on", "Arp On/Off", 0),
    C("arp_mode", "Arp Mode", NARPMODE, 0, ARPMODE),
    N("arp_range", "Arp Range", 1, 3, 1),
    N("arp_repeat", "Arp Repeats", 1, 3, 1),
    B("arp_relatch", "Arp Relatch", 0),
    B("seq_on", "Seq Play", 0),
    C("seq_type", "Seq Type", 2, ST_GATED, SEQTYPE),
    C("seq_mode", "Seq Mode", NGATEDMODE, 0, GATEDMODE),
    C("seq_d1", "Seq 1 Destination", NMODDST, MD_OFF, MODDST_NAME),
    C("seq_d2", "Seq 2 Destination", NMODDST + 1, MD_OFF, MODDST_NAME),
    C("seq_d3", "Seq 3 Destination", NMODDST, MD_OFF, MODDST_NAME),
    C("seq_d4", "Seq 4 Destination", NMODDST + 1, MD_OFF, MODDST_NAME),
    N("pgm_vol", "Program Volume", 0, 127, 100),
    B("unison", "Unison", 0),
    N("uni_mode", "Unison Mode", 1, 17, 4),
    N("uni_detune", "Unison Detune", 0, 16, 4),
    C("key_mode", "Key Mode", NKEYMODE, KM_LAST, KEYMODE),
    N("pb_range", "Pitchbend Range", 0, 12, 2),
    C("pan_mode", "Pan Mod Mode", 2, 0, PANMODE),
};

const ParamDef PROGRAM[NPROGRAM] = {
    C("layer_mode", "Layer Mode", 3, LM_SINGLE, LAYERMODE),
    {"split_point", "Split Point", 0, 127, 60, K_NOTE, nullptr},
    C("edit_b", "Edit Layer", 2, 0, EDITB),
};

static const char *const NOTE12[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

void paramText(int p, int v, char *out, int n, bool synced, int fxType)
{
    const ParamDef &d = LAYER[p];
    switch (d.kind) {
    case K_SIGNED: std::snprintf(out, (size_t)n, v > 0 ? "+%d" : "%d", v); break;
    case K_CHOICE:
    case K_ONOFF: std::snprintf(out, (size_t)n, "%s", d.choice[v < d.lo ? d.lo : v > d.hi ? d.hi : v]); break;
    case K_NOTE: std::snprintf(out, (size_t)n, "%s%d", NOTE12[v % 12], v / 12 - 2); break;
    case K_FREQ: std::snprintf(out, (size_t)n, "%s%d", NOTE12[v % 12], v / 12); break;
    case K_LFOFREQ:
        if (synced)
            std::snprintf(out, (size_t)n, "%s", LFOSYNC_NAME[v * 16 / 151]);
        else
            std::snprintf(out, (size_t)n, "%d", v);
        break;
    case K_FXP1:
        if (synced && fxType <= FX_BBD)
            std::snprintf(out, (size_t)n, "%s", DLYSYNC_NAME[v * 11 / 256]);
        else
            std::snprintf(out, (size_t)n, "%d", v);
        break;
    default:
        if (p == L_UNI_MODE && v == 17)
            std::snprintf(out, (size_t)n, "Chord");
        else
            std::snprintf(out, (size_t)n, "%d", v);
    }
}

void seqClear(SeqData &s)
{
    std::memset(&s, 0, sizeof s);
    for (auto &t : s.gated)
        for (auto &st : t)
            st = 0;
}

void programInit(Program &p)
{
    std::memset(&p, 0, sizeof p);
    std::snprintf(p.name, NAME_LEN, "Basic Program");
    for (int l = 0; l < 2; l++) {
        for (int i = 0; i < NLAYER; i++)
            p.layer[l][i] = (int16_t)LAYER[i].def;
        seqClear(p.seq[l]);
        for (auto &c : p.chord[l])
            c = -1;
    }
    for (int i = 0; i < NPROGRAM; i++)
        p.prog[i] = (int16_t)PROGRAM[i].def;
}
} // namespace rv
