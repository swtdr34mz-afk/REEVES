/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (REEVES) */
/* REEVES' parameters, laid out as a Prophet Rev2 program: two layers (A and B) of the same parameters, and the
 * program's own (STACK / SPLIT, the split point). Every value is an integer in the Rev2's own range (CUTOFF 0..164,
 * FINE TUNE -50..50, ENV AMOUNT -127..127, ...), so what the editor's display shows is what the Rev2 shows.
 *
 * The modulation sources and destinations are the Rev2's (its manual, appendices A and B): 22 sources, 53
 * destinations, the same order. */
#pragma once
#include <cstdint>

namespace rv
{
/* ------------------------------------------------------------------ choices --- */
enum OscShape { SH_OFF, SH_SAW, SH_SAWTRI, SH_TRI, SH_PULSE, NSHAPE };
enum LfoShape { LS_TRI, LS_SAW, LS_REVSAW, LS_SQUARE, LS_RANDOM, NLFOSHAPE };
enum GlideMode { GM_RATE, GM_RATE_A, GM_TIME, GM_TIME_A, NGLIDEMODE };
enum KeyMode { KM_LOW, KM_HIGH, KM_LAST, KM_LOW_R, KM_HIGH_R, KM_LAST_R, NKEYMODE };
enum ArpMode { AM_UP, AM_DOWN, AM_UPDOWN, AM_RANDOM, AM_ASSIGN, NARPMODE };
enum SeqType { ST_GATED, ST_POLY };
enum GatedMode { GS_NORMAL, GS_NORESET, GS_NOGATE, GS_NOGATE_NORESET, GS_KEYSTEP, NGATEDMODE };
enum LayerMode { LM_SINGLE, LM_STACK, LM_SPLIT };
enum FxType {
    FX_DELAY_MONO, FX_DDL_STEREO, FX_BBD, FX_CHORUS, FX_PHASER_HI, FX_PHASER_LO, FX_PHASER_MST, FX_FLANGER1,
    FX_FLANGER2, FX_REVERB, FX_RINGMOD, FX_DISTORTION, FX_HPF, NFXTYPE
};
enum Divide {
    DV_HALF, DV_QUARTER, DV_8, DV_8HALF, DV_8SWING, DV_8TRIP, DV_16, DV_16HALF, DV_16SWING, DV_16TRIP, DV_32, DV_32TRIP,
    DV_64TRIP, NDIVIDE
};

/* the modulation sources (appendix A) */
enum ModSrc {
    MS_OFF, MS_SEQ1, MS_SEQ2, MS_SEQ3, MS_SEQ4, MS_LFO1, MS_LFO2, MS_LFO3, MS_LFO4, MS_ENV_LPF, MS_ENV_VCA, MS_ENV3,
    MS_PITCHBEND, MS_MODWHEEL, MS_PRESSURE, MS_BREATH, MS_FOOT, MS_EXPRESSION, MS_VELOCITY, MS_NOTE, MS_NOISE, MS_DC,
    MS_AUDIO, NMODSRC
};
/* the destinations (appendix B); the gated sequencer's tracks 2 and 4 add SLEW (the track before) */
enum ModDst {
    MD_OFF, MD_OSC1_FREQ, MD_OSC2_FREQ, MD_OSC_FREQ, MD_OSC1_LEVEL, MD_OSC_MIX, MD_NOISE, MD_SUB, MD_OSC1_SHAPE,
    MD_OSC2_SHAPE, MD_OSC_SHAPE, MD_CUTOFF, MD_RESO, MD_AUDIOMOD, MD_VCA, MD_PAN, MD_LFO1_FREQ, MD_LFO2_FREQ,
    MD_LFO3_FREQ, MD_LFO4_FREQ, MD_LFO_FREQ, MD_LFO1_AMT, MD_LFO2_AMT, MD_LFO3_AMT, MD_LFO4_AMT, MD_LFO_AMT,
    MD_FENV_AMT, MD_AENV_AMT, MD_ENV3_AMT, MD_ENV_AMT, MD_FENV_ATT, MD_AENV_ATT, MD_ENV3_ATT, MD_ENV_ATT, MD_FENV_DEC,
    MD_AENV_DEC, MD_ENV3_DEC, MD_ENV_DEC, MD_FENV_REL, MD_AENV_REL, MD_ENV3_REL, MD_ENV_REL, MD_MOD1_AMT, MD_MOD2_AMT,
    MD_MOD3_AMT, MD_MOD4_AMT, MD_MOD5_AMT, MD_MOD6_AMT, MD_MOD7_AMT, MD_MOD8_AMT, MD_SLOP, MD_FX_MIX, MD_FX_P1,
    MD_FX_P2, NMODDST,
    MD_SLEW = NMODDST                    /* (the gated sequencer's tracks 2 and 4 only) */
};

/* -------------------------------------------------------- a layer's parameters --- */
enum LayerParam {
    /* OSCILLATORS */
    L_O1_FREQ, L_O1_FINE, L_O1_SHAPE, L_O1_SHMOD, L_O2_FREQ, L_O2_FINE, L_O2_SHAPE, L_O2_SHMOD, L_SYNC, L_SUB, L_OSC_MIX,
    L_NOISE, L_SLOP, L_O1_KEY, L_O2_KEY, L_O1_RESET, L_O2_RESET,
    /* GLIDE */
    L_GLIDE, L_O1_GLIDE, L_O2_GLIDE, L_GLIDE_MODE,
    /* LOW-PASS FILTER */
    L_CUTOFF, L_RESO, L_FENV_AMT, L_FVEL, L_KEYAMT, L_AUDIOMOD, L_POLE4, L_FDEL, L_FATT, L_FDEC, L_FSUS, L_FREL,
    /* AMPLIFIER */
    L_PAN_SPREAD, L_AENV_AMT, L_AVEL, L_ADEL, L_AATT, L_ADEC, L_ASUS, L_AREL, L_VCA_LEVEL,
    /* AUXILIARY ENVELOPE */
    L_E3_DEST, L_E3_AMT, L_E3_VEL, L_E3DEL, L_E3ATT, L_E3DEC, L_E3SUS, L_E3REL, L_E3_REPEAT,
    /* LOW FREQUENCY OSCILLATORS: four of LFO_N each */
    L_LFO1, LFO_FREQ = 0, LFO_SHAPE, LFO_AMT, LFO_DEST, LFO_CLK, LFO_KEY, LFO_N,
    L_LFO_END = L_LFO1 + 4 * LFO_N,
    /* MODULATION: eight slots of three */
    L_MOD1 = L_LFO_END, MOD_SRC = 0, MOD_DST, MOD_AMT, MOD_N,
    L_MOD_END = L_MOD1 + 8 * MOD_N,
    /* the dedicated sources: mod wheel, pressure, breath, velocity, foot (amount, destination each) */
    L_MW_AMT = L_MOD_END, L_MW_DST, L_AT_AMT, L_AT_DST, L_BR_AMT, L_BR_DST, L_VEL_AMT, L_VEL_DST, L_FT_AMT, L_FT_DST,
    /* EFFECT */
    L_FX_ON, L_FX_TYPE, L_FX_MIX, L_FX_P1, L_FX_P2, L_FX_CLK,
    /* CLOCK, ARP */
    L_BPM, L_DIVIDE, L_ARP_ON, L_ARP_MODE, L_ARP_RANGE, L_ARP_REPEAT, L_ARP_RELATCH,
    /* SEQUENCER */
    L_SEQ_ON, L_SEQ_TYPE, L_SEQ_MODE, L_SEQ_D1, L_SEQ_D2, L_SEQ_D3, L_SEQ_D4,
    /* MISC PARAMS, and the panel's UNISON */
    L_PGM_VOL, L_UNISON, L_UNI_MODE, L_UNI_DETUNE, L_KEY_MODE, L_PB_RANGE, L_PAN_MODE,
    NLAYER
};

/* the program's own and the plugin's */
enum ProgramParam { G_LAYER_MODE, G_SPLIT_POINT, G_EDIT_B, NPROGRAM };

enum Kind : uint8_t {
    K_NUM,      /* a number as it is */
    K_SIGNED,   /* with its sign (+12, -3) */
    K_CHOICE,   /* a name from the list */
    K_ONOFF,
    K_NOTE,     /* a MIDI note's name (C3, F#4) */
    K_FREQ,     /* an oscillator's pitch (C0 .. C10, the Rev2's) */
    K_LFOFREQ,  /* 0..150, a clock division when synced */
    K_FXP1      /* the effect's first parameter (delay time synced: a note) */
};

struct ParamDef {
    const char *id;           /* the host's id (stable: saved projects) */
    const char *name;         /* the host's and the display's name */
    int lo, hi, def;
    Kind kind;
    const char *const *choice;
};

extern const ParamDef LAYER[NLAYER];
extern const ParamDef PROGRAM[NPROGRAM];
extern const char *const MODSRC_NAME[NMODSRC];
extern const char *const MODDST_NAME[NMODDST + 1];
extern const char *const FXTYPE_NAME[NFXTYPE];
extern const char *const DIVIDE_NAME[NDIVIDE];
extern const char *const LFOSYNC_NAME[17];
extern const char *const DLYSYNC_NAME[11];
extern const char *const OSCSHAPE_NAME[NSHAPE];
extern const char *const LFOSHAPE_NAME[NLFOSHAPE];

/* the value as the display shows it (layer p, value v, the layer's LFO / delay sync for the synced texts) */
void paramText(int p, int v, char *out, int n, bool synced = false, int fxType = 0);

/* the gated sequencer's and the poly sequencer's data: in the program, not host parameters */
constexpr int GSEQ_STEPS = 16, GSEQ_TRACKS = 4, GSEQ_RESET = 126;
constexpr int PSEQ_STEPS = 64, PSEQ_NOTES = 6;
constexpr uint8_t PN_REST = 0, PN_TIE = 1;          /* a poly step's note slot: 0 rest, 1 tie, else note + 2 */
struct SeqData {
    uint8_t gated[GSEQ_TRACKS][GSEQ_STEPS];          /* 0..125, GSEQ_RESET marks the end */
    uint8_t note[PSEQ_STEPS][PSEQ_NOTES];            /* PN_REST, PN_TIE, or MIDI note + 2 */
    uint8_t vel[PSEQ_STEPS][PSEQ_NOTES];
    uint8_t length;                                  /* the poly sequence's steps (0: none recorded) */
};
void seqClear(SeqData &s);

/* a whole program: both layers, the program's parameters, a sequence each layer, a chord memory each layer */
constexpr int NAME_LEN = 20;
struct Program {
    char name[NAME_LEN];
    int16_t layer[2][NLAYER];
    int16_t prog[NPROGRAM];
    SeqData seq[2];
    int8_t chord[2][16];                             /* UNISON CHORD's notes, relative to the lowest (-1: none) */
};
void programInit(Program &p);                         /* the BASIC PROGRAM (the Rev2's) */

} // namespace rv
