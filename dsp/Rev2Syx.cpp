/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (REEVES) */
#include "Rev2Syx.h"
#include <algorithm>
#include <cstring>

namespace rv
{
namespace
{
constexpr int RAW = 2046;                /* a program's bytes: layer A at 0, layer B at 1024 */
constexpr int LAYER_B = 1024;

/* "packed MS bit": each 8 bytes carry 7, the first holding their top bits (bit k: the k-th byte's) */
std::vector<uint8_t> unpack(const uint8_t *p, size_t n)
{
    std::vector<uint8_t> out;
    out.reserve(n * 7 / 8 + 7);
    for (size_t i = 0; i < n; i += 8) {
        uint8_t ms = p[i];
        for (size_t j = 1; j < 8 && i + j < n; j++)
            out.push_back((uint8_t)(p[i + j] | (((ms >> (j - 1)) & 1) << 7)));
    }
    return out;
}

int clampTo(int p, int v) { return std::clamp(v, LAYER[p].lo, LAYER[p].hi); }

/* a layer: where the Rev2 keeps each parameter in its 1024 bytes, and how its value becomes REEVES' */
void layerFrom(const uint8_t *b, int16_t *L, SeqData &s)
{
    auto set = [&](int p, int v) { L[p] = (int16_t)clampTo(p, v); };
    auto bip = [&](int p, int v) { L[p] = (int16_t)clampTo(p, v - 127); };   /* 0..254 around 127 */
    set(L_O1_FREQ, b[0]);
    set(L_O2_FREQ, b[1]);
    set(L_O1_FINE, b[2] - 50);
    set(L_O2_FINE, b[3] - 50);
    set(L_O1_SHAPE, b[4]);
    set(L_O2_SHAPE, b[5]);
    set(L_O1_SHMOD, b[6]);
    set(L_O2_SHMOD, b[7]);
    set(L_O1_GLIDE, b[8]);
    set(L_O2_GLIDE, b[9]);
    set(L_O1_KEY, b[10]);
    set(L_O2_KEY, b[11]);
    set(L_O1_RESET, b[12]);
    set(L_O2_RESET, b[13]);
    set(L_OSC_MIX, b[14]);
    set(L_SUB, b[15]);
    set(L_NOISE, b[16]);
    set(L_SYNC, b[17]);
    set(L_GLIDE_MODE, b[18]);
    set(L_GLIDE, b[19]);
    set(L_PB_RANGE, b[20]);
    set(L_SLOP, b[21]);
    set(L_CUTOFF, b[22]);
    set(L_RESO, b[23]);
    set(L_KEYAMT, b[24]);
    set(L_AUDIOMOD, b[25]);
    set(L_POLE4, b[26]);
    set(L_VCA_LEVEL, b[27]);
    set(L_PGM_VOL, b[28]);
    set(L_PAN_SPREAD, b[29]);
    set(L_E3_DEST, b[30]);
    set(L_E3_REPEAT, b[31]);
    bip(L_FENV_AMT, b[32]);
    set(L_AENV_AMT, b[33]);
    bip(L_E3_AMT, b[34]);
    /* the three envelopes side by side: LPF, VCA, ENV 3 */
    const int env[3][6] = {{L_FVEL, L_FDEL, L_FATT, L_FDEC, L_FSUS, L_FREL},
                           {L_AVEL, L_ADEL, L_AATT, L_ADEC, L_ASUS, L_AREL},
                           {L_E3_VEL, L_E3DEL, L_E3ATT, L_E3DEC, L_E3SUS, L_E3REL}};
    for (int e = 0; e < 3; e++)
        for (int k = 0; k < 6; k++)
            set(env[e][k], b[35 + 3 * k + e]);
    /* the four LFOs side by side */
    for (int k = 0; k < 4; k++) {
        int q = L_LFO1 + k * LFO_N;
        set(q + LFO_FREQ, b[53 + k]);
        set(q + LFO_SHAPE, b[57 + k]);
        set(q + LFO_AMT, b[61 + k]);
        set(q + LFO_DEST, b[65 + k]);
        set(q + LFO_CLK, b[69 + k]);
        set(q + LFO_KEY, b[73 + k]);
    }
    for (int k = 0; k < 8; k++) {
        int q = L_MOD1 + k * MOD_N;
        set(q + MOD_SRC, b[77 + k]);
        bip(q + MOD_AMT, b[85 + k]);
        set(q + MOD_DST, b[93 + k]);
    }
    const int ctl[5][2] = {{L_MW_AMT, L_MW_DST}, {L_AT_AMT, L_AT_DST}, {L_BR_AMT, L_BR_DST}, {L_VEL_AMT, L_VEL_DST},
                           {L_FT_AMT, L_FT_DST}};
    for (int k = 0; k < 5; k++) {
        bip(ctl[k][0], b[101 + 2 * k]);
        set(ctl[k][1], b[102 + 2 * k]);
    }
    set(L_SEQ_D1, b[111]);
    set(L_SEQ_D2, b[112]);
    set(L_SEQ_D3, b[113]);
    set(L_SEQ_D4, b[114]);
    /* the Rev2's effect 0 is OFF; its 1..13 are REEVES' 13 */
    if (b[115] == 0) {
        set(L_FX_ON, 0);
    } else {
        set(L_FX_ON, b[116]);
        set(L_FX_TYPE, b[115] - 1);
    }
    set(L_FX_MIX, b[117]);
    set(L_FX_P1, b[118]);
    set(L_FX_P2, b[119]);
    set(L_FX_CLK, b[120]);
    set(L_UNI_DETUNE, b[121]);
    set(L_KEY_MODE, b[122]);
    set(L_UNISON, b[123]);
    set(L_UNI_MODE, b[124] + 1);         /* 1..16 voices, then CHORD */
    set(L_BPM, b[130]);
    set(L_DIVIDE, b[131]);
    set(L_ARP_MODE, b[132]);
    set(L_ARP_RANGE, b[133] + 1);
    set(L_ARP_REPEAT, b[134] + 1);
    set(L_ARP_RELATCH, b[135]);
    set(L_ARP_ON, b[136]);
    set(L_SEQ_ON, 0);                     /* (a program never starts its sequence by itself) */
    set(L_SEQ_MODE, b[138]);
    set(L_SEQ_TYPE, b[139] ? ST_GATED : ST_POLY);

    /* the gated sequence: four tracks of 16 (126: the track's end; 127, a rest, has no place here: 0) */
    for (int t = 0; t < GSEQ_TRACKS; t++)
        for (int i = 0; i < GSEQ_STEPS; i++) {
            int v = b[140 + 16 * t + i];
            s.gated[t][i] = (uint8_t)(v == 127 ? 0 : std::min(v, (int)GSEQ_RESET));
        }
    /* the poly sequence: six note rows of 64 steps, each followed by its velocities (128 + velocity: a note;
     * 129: a tie; 128 or 0: nothing) */
    int last = -1;
    for (int i = 0; i < PSEQ_STEPS; i++)
        for (int k = 0; k < PSEQ_NOTES; k++) {
            int n = b[256 + 128 * k + i], v = b[320 + 128 * k + i];
            if (v > 129 && n < 128) {
                s.note[i][k] = (uint8_t)(n + 2);
                s.vel[i][k] = (uint8_t)(v - 128);
                last = std::max(last, i);
            } else if (v == 129) {
                s.note[i][k] = PN_TIE;
                s.vel[i][k] = 0;
                last = std::max(last, i);
            } else {
                s.note[i][k] = PN_REST;
                s.vel[i][k] = 0;
                if (v == 128 && k == 0)
                    last = std::max(last, i);  /* (a rest step still counts in the sequence's length) */
            }
        }
    s.length = (uint8_t)(last + 1);
}
} // namespace

void rev2ToProgram(const uint8_t *raw, Program &p)
{
    programInit(p);
    for (int l = 0; l < 2; l++)
        layerFrom(raw + (l ? LAYER_B : 0), p.layer[l], p.seq[l]);
    p.prog[G_LAYER_MODE] = (int16_t)std::clamp((int)raw[231], PROGRAM[G_LAYER_MODE].lo, PROGRAM[G_LAYER_MODE].hi);
    p.prog[G_SPLIT_POINT] = (int16_t)std::clamp((int)raw[232], PROGRAM[G_SPLIT_POINT].lo, PROGRAM[G_SPLIT_POINT].hi);
    char name[NAME_LEN] = {};
    int n = 0;
    for (int i = 0; i < NAME_LEN - 1; i++) {
        uint8_t c = raw[235 + i];
        name[n++] = (char)(c >= 32 && c < 127 ? c : ' ');
    }
    while (n > 0 && name[n - 1] == ' ')
        name[--n] = 0;
    std::memcpy(p.name, n ? name : "Rev2 Program", NAME_LEN);
    p.name[NAME_LEN - 1] = 0;
}

std::vector<Rev2Import> rev2ReadSyx(const uint8_t *d, size_t size)
{
    std::vector<Rev2Import> out;
    size_t i = 0;
    while (i < size) {
        if (d[i] != 0xF0) {
            i++;
            continue;
        }
        size_t e = i + 1;
        while (e < size && d[e] != 0xF7)
            e++;
        if (e >= size)
            break;
        /* F0 01 2F 02 bank prog <packed> F7, or F0 01 2F 03 <packed> F7 */
        const uint8_t *m = d + i;
        size_t len = e - i + 1;
        if (len > 6 && m[1] == 0x01 && m[2] == 0x2F && (m[3] == 0x02 || m[3] == 0x03)) {
            bool dump = m[3] == 0x02;
            size_t at = dump ? 6 : 4;
            if (len > at + 1) {
                auto raw = unpack(m + at, len - at - 1);
                if (raw.size() >= (size_t)RAW) {
                    Rev2Import r;
                    rev2ToProgram(raw.data(), r.program);
                    if (dump) {
                        r.bank = m[4];
                        r.number = m[5];
                    }
                    out.push_back(r);
                }
            }
        }
        i = e + 1;
    }
    return out;
}
} // namespace rv
