/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (REEVES) */
/* A layer's EFFECT: the Rev2's thirteen types, each with MIX and two parameters (the manual's table):
 *   DELAY MONO, DDL STEREO (ping-pong), BBD DELAY (dark, saturating, a little wobble): time, feedback; synced to
 *     the clock: the note values (halved while over a second, as the Rev2 does)
 *   CHORUS, PHASER HIGH / LOW / MST (six stages; MST in stereo), FLANGER 1 (through zero, high feedback) and
 *     FLANGER 2: rate, depth
 *   REVERB: time, colour
 *   RING MOD (after Oberheim's): tuning; tracking (the lowest note sets the carrier, as a ratio)
 *   DISTORTION: gain, tone
 *   HP FILTER: cutoff, resonance
 * Everything glides (no zipper, no click), a new type crossfades in. */
#pragma once
#include "Dsp.h"
#include "Params.h"
#include <vector>

namespace rv
{
class Fx
{
public:
    void prepare(float sampleRate)
    {
        sr = sampleRate;
        dlN = 1;
        while (dlN < (int)(sr * 1.05f) + 8)
            dlN <<= 1;
        dl[0].assign((size_t)dlN, 0.0f);
        dl[1].assign((size_t)dlN, 0.0f);
        mdN = 1;
        while (mdN < (int)(sr * 0.04f))
            mdN <<= 1;
        md[0].assign((size_t)mdN, 0.0f);
        md[1].assign((size_t)mdN, 0.0f);
        for (int i = 0; i < 8; i++) {
            int len = (int)(RV_LEN[i] * sr / 44100.0f * 1.6f) + 4;
            rvLine[i].assign((size_t)len, 0.0f);
        }
        for (int i = 0; i < 4; i++)
            rvAp[i].assign((size_t)((int)(RV_AP[i] * sr / 44100.0f) + 2), 0.0f);
        clear();
    }
    void clear()
    {
        for (auto &b : dl)
            std::fill(b.begin(), b.end(), 0.0f);
        for (auto &b : md)
            std::fill(b.begin(), b.end(), 0.0f);
        for (auto &b : rvLine)
            std::fill(b.begin(), b.end(), 0.0f);
        for (auto &b : rvAp)
            std::fill(b.begin(), b.end(), 0.0f);
        std::memset(rvW, 0, sizeof rvW);
        std::memset(rvApW, 0, sizeof rvApW);
        std::memset(rvLp, 0, sizeof rvLp);
        std::memset(ph6, 0, sizeof ph6);
        std::memset(bbdLp, 0, sizeof bbdLp);
        std::memset(fbS, 0, sizeof fbS);
        dlW = mdW = 0;
        hpS1[0] = hpS1[1] = hpS2[0] = hpS2[1] = 0.0f;
        dsLp[0] = dsLp[1] = 0.0f;
        tail = 0.0f;
    }
    /* the parameters (each block): on, type, mix 0..127, p1 0..255, p2 0..127, synced (delays), the tempo, the
     * lowest note's pitch (Hz: RING MOD's tracking; 0 none) */
    void set(bool on_, int type_, float mix_, float p1_, float p2_, bool synced_, float bpm_, float trackHz_)
    {
        wantOn = on_;
        wantType = type_;
        tMix = clampf(mix_, 0.0f, 127.0f) / 127.0f;
        tP1 = clampf(p1_, 0.0f, 255.0f);
        tP2 = clampf(p2_, 0.0f, 127.0f);
        synced = synced_;
        bpm = bpm_ < 20.0f ? 120.0f : bpm_;
        trackHz = trackHz_;
        if (!init) {
            type = wantType;
            on = wantOn;
            sMix = tMix;
            sP1 = tP1;
            sP2 = tP2;
            xf = on ? 1.0f : 0.0f;
            init = true;
        }
    }
    float tailLevel() const { return tail; }

    void process(float *L, float *R, int n)
    {
        const float kp = 1.0f - std::exp(-1.0f / (0.02f * sr));     /* the parameters' glide: 20 ms */
        const float kx = 1.0f / (0.012f * sr);                      /* a crossfade: 12 ms */
        for (int i = 0; i < n; i++) {
            /* a new type or off: out, then the new one in */
            bool wantRun = wantOn;
            if ((wantType != type || !wantRun) && xf > 0.0f) {
                xf -= kx;
                if (xf <= 0.0f) {
                    xf = 0.0f;
                    if (wantType != type) {
                        type = wantType;
                        clearState();
                    }
                }
            } else if (wantType == type && wantRun && xf < 1.0f) {
                xf = minf(1.0f, xf + kx);
            }
            on = xf > 0.0f;
            sMix += (tMix - sMix) * kp;
            sP1 += (tP1 - sP1) * kp;
            sP2 += (tP2 - sP2) * kp;
            float x0 = L[i], x1 = R[i];
            if (!on) {                       /* off: the dry as it is */
                tail *= 0.9999f;
                continue;
            }
            float wl = x0, wr = x1;
            run(x0, x1, wl, wr);
            float m = sMix * xf, s = 1.0f - m;
            L[i] = x0 * s + wl * m;
            R[i] = x1 * s + wr * m;
            float a = std::fabs(wl) + std::fabs(wr);
            tail = a > tail ? a : tail * 0.99995f;
        }
        for (int c = 0; c < 2; c++) {
            bbdLp[c] = flushDn(bbdLp[c]);
            hpS1[c] = flushDn(hpS1[c]);
            hpS2[c] = flushDn(hpS2[c]);
            for (int k = 0; k < 6; k++)
                ph6[c][k] = flushDn(ph6[c][k]);
        }
        for (float &v : rvLp)
            v = flushDn(v);
    }

private:
    static constexpr float RV_LEN[8] = {1557, 1617, 1491, 1422, 1277, 1356, 1188, 1116};
    static constexpr float RV_AP[4] = {142, 107, 379, 277};
    static constexpr float DLY_BEATS[11] = {4.0f, 3.0f, 2.0f, 1.5f, 1.0f, 2.0f / 3.0f, 0.75f, 0.5f, 1.0f / 3.0f, 0.375f, 0.25f};

    float sr = 44100.0f;
    bool init = false, wantOn = false, on = false, synced = false;
    int wantType = FX_DDL_STEREO, type = FX_DDL_STEREO;
    float tMix = 0, tP1 = 0, tP2 = 0, sMix = 0, sP1 = 0, sP2 = 0, xf = 0, bpm = 120, trackHz = 0, tail = 0;
    float dlTime[2] = {0.25f, 0.25f};     /* the delay's time, gliding (seconds) */

    std::vector<float> dl[2], md[2], rvLine[8], rvAp[4];
    int dlN = 1, dlW = 0, mdN = 1, mdW = 0;
    int rvW[8] = {}, rvApW[4] = {};
    float rvLp[8] = {}, rvMph[8] = {};
    float lfoPh = 0.0f, ph6[2][6] = {}, fbS[2] = {}, bbdLp[2] = {}, bbdWob = 0.0f;
    float hpS1[2] = {}, hpS2[2] = {}, dsLp[2] = {}, ringPh = 0.0f;
    uint32_t seed = 0x13579BDFu;

    void clearState()
    {
        float keep = tail;
        clear();
        tail = keep;
    }
    inline float rd(const std::vector<float> &b, int N, int w, float d) const   /* d samples back, interpolated */
    {
        float p = (float)w - d;
        while (p < 0.0f)
            p += (float)N;
        int i0 = (int)p;
        float f = p - (float)i0;
        float a = b[(size_t)(i0 & (N - 1))], c = b[(size_t)((i0 + 1) & (N - 1))];
        return a + (c - a) * f;
    }
    float delaySeconds()
    {
        if (synced) {
            int k = (int)(sP1 * 11.0f / 256.0f);
            float t = DLY_BEATS[k < 0 ? 0 : k > 10 ? 10 : k] * 60.0f / bpm;
            while (t > 1.0f)
                t *= 0.5f;
            return t;
        }
        return 0.002f + sP1 * (0.998f / 255.0f);
    }

    void run(float x0, float x1, float &yl, float &yr)
    {
        const float isr = 1.0f / sr;
        switch (type) {
        case FX_DELAY_MONO:
        case FX_DDL_STEREO:
        case FX_BBD: {
            float t = delaySeconds();
            float kt = type == FX_BBD ? 0.00012f : 0.00035f;       /* the time glides (a tape's bend, no click) */
            kt = 1.0f - std::pow(1.0f - kt, 44100.0f * isr);
            dlTime[0] += (t - dlTime[0]) * kt;
            float fb = minf(sP2 / 127.0f * 1.02f, 0.98f);
            float d = dlTime[0] * sr;
            if (type == FX_BBD) {                                  /* a wobble, as a BBD's clock drifts */
                bbdWob += 2.0f * PI * 0.6f * isr;
                if (bbdWob > 2.0f * PI)
                    bbdWob -= 2.0f * PI;
                d *= 1.0f + 0.0025f * std::sin(bbdWob);
            }
            d = clampf(d, 2.0f, (float)dlN - 4.0f);
            if (type == FX_DELAY_MONO) {
                float in = 0.5f * (x0 + x1), y = rd(dl[0], dlN, dlW, d);
                dl[0][(size_t)dlW] = in + softT(y * fb);
                yl = yr = y;
            } else if (type == FX_DDL_STEREO) {                     /* ping-pong */
                float a = rd(dl[0], dlN, dlW, d), b = rd(dl[1], dlN, dlW, d);
                dl[0][(size_t)dlW] = 0.5f * (x0 + x1) + b * fb;
                dl[1][(size_t)dlW] = a * fb;
                yl = a;
                yr = b;
            } else {                                               /* BBD: dark and darker with time, saturating */
                float fc = clampf(9000.0f - dlTime[0] * 6500.0f, 1800.0f, 9000.0f);
                float k = 1.0f - std::exp(-2.0f * PI * fc * isr);
                for (int c = 0; c < 2; c++) {
                    float y = rd(dl[c], dlN, dlW, d * (c ? 1.012f : 1.0f));
                    bbdLp[c] += (y - bbdLp[c]) * k;
                    float in = c ? x1 : x0;
                    dl[c][(size_t)dlW] = softT((in + bbdLp[c] * fb) * 0.9f) * 1.05f + 1e-9f * rndF(seed);
                    (c ? yr : yl) = bbdLp[c];
                }
            }
            dlW = (dlW + 1) & (dlN - 1);
            break;
        }
        case FX_CHORUS:
        case FX_FLANGER1:
        case FX_FLANGER2: {
            float rate = type == FX_CHORUS ? 0.05f + std::pow(sP1 / 255.0f, 2.0f) * 8.0f
                                           : 0.02f + std::pow(sP1 / 255.0f, 2.0f) * 6.0f;
            float depth = sP2 / 127.0f;
            lfoPh += rate * isr;
            if (lfoPh >= 1.0f)
                lfoPh -= 1.0f;
            float ml = 0.5f + 0.5f * std::sin(2.0f * PI * lfoPh), mr = 0.5f + 0.5f * std::sin(2.0f * PI * (lfoPh + 0.25f));
            md[0][(size_t)mdW] = x0 + (type != FX_CHORUS ? fbS[0] : 0.0f);
            md[1][(size_t)mdW] = x1 + (type != FX_CHORUS ? fbS[1] : 0.0f);
            if (type == FX_CHORUS) {
                float base = 0.007f * sr, sw = (0.0005f + 0.006f * depth) * sr;
                yl = rd(md[0], mdN, mdW, base + sw * ml);
                yr = rd(md[1], mdN, mdW, base + sw * mr);
            } else {
                bool tz = type == FX_FLANGER1;
                float fb = tz ? 0.86f : 0.6f;
                float base = (tz ? 0.0002f : 0.0006f) * sr, sw = (0.0002f + 0.0065f * depth) * sr;
                float a = rd(md[0], mdN, mdW, base + sw * ml), b = rd(md[1], mdN, mdW, base + sw * mr);
                fbS[0] = softT(a * fb);
                fbS[1] = softT(b * fb);
                if (tz) {                                       /* through zero: against the dry, slightly late */
                    a = 0.6f * (a - rd(md[0], mdN, mdW, base + sw * 0.5f));
                    b = 0.6f * (b - rd(md[1], mdN, mdW, base + sw * 0.5f));
                } else {
                    a *= 0.8f;
                    b *= 0.8f;
                }
                yl = a;
                yr = b;
            }
            mdW = (mdW + 1) & (mdN - 1);
            break;
        }
        case FX_PHASER_HI:
        case FX_PHASER_LO:
        case FX_PHASER_MST: {
            float rate = 0.03f + std::pow(sP1 / 255.0f, 2.0f) * 7.0f, depth = sP2 / 127.0f;
            float fb = type == FX_PHASER_HI ? 0.78f : type == FX_PHASER_LO ? 0.35f : 0.6f;
            lfoPh += rate * isr;
            if (lfoPh >= 1.0f)
                lfoPh -= 1.0f;
            for (int c = 0; c < 2; c++) {
                float ph = lfoPh + (type == FX_PHASER_MST && c ? 0.25f : 0.0f);
                float m = 0.5f + 0.5f * std::sin(2.0f * PI * ph);
                float fc = 180.0f * std::exp2(depth * 4.5f * m + (1.0f - depth) * 2.0f);
                float t = tanW(clampf(PI * fc * isr, 0.0f, 1.2f)), a = (t - 1.0f) / (t + 1.0f);
                float x = (c ? x1 : x0) + fbS[c] * fb;
                for (int k = 0; k < 6; k++) {                    /* first-order allpasses */
                    float y = a * x + ph6[c][k];
                    ph6[c][k] = x - a * y;
                    x = y;
                }
                fbS[c] = softT(x);
                (c ? yr : yl) = x;
            }
            /* (wet alone is the phaser's sweep against nothing: the notches come with the dry, so half and half) */
            yl = 0.5f * (yl + x0);
            yr = 0.5f * (yr + x1);
            break;
        }
        case FX_REVERB: {
            /* eight lines with a Householder mix, each damped (COLOUR) and gently moving; TIME: the decay */
            float t60 = 0.3f + std::pow(sP1 / 255.0f, 1.6f) * 14.0f;
            float damp = 1500.0f * std::exp2(sP2 / 127.0f * 3.6f);
            float kd = 1.0f - std::exp(-2.0f * PI * damp * isr);
            float in = 0.35f * (x0 + x1);
            for (int i = 0; i < 4; i++) {                        /* input diffusion */
                auto &b = rvAp[i];
                int N = (int)b.size(), w = rvApW[i];
                float z = b[(size_t)w], y = -0.6f * in + z;
                b[(size_t)w] = in + 0.6f * y;
                rvApW[i] = (w + 1) % N;
                in = y;
            }
            float o[8], sum = 0.0f;
            for (int i = 0; i < 8; i++) {
                auto &b = rvLine[i];
                int N = (int)b.size();
                rvMph[i] += (0.3f + 0.07f * (float)i) * isr;
                if (rvMph[i] >= 1.0f)
                    rvMph[i] -= 1.0f;
                float len = RV_LEN[i] * sr / 44100.0f * (1.0f + 0.4f * minf(sP1 / 255.0f * 1.5f, 1.0f));
                float d = minf(len + 6.0f * std::sin(2.0f * PI * rvMph[i]), (float)N - 3.0f);
                float p = (float)rvW[i] - d;
                while (p < 0.0f)
                    p += (float)N;
                int i0 = (int)p;
                float f = p - (float)i0, a = b[(size_t)(i0 % N)], c = b[(size_t)((i0 + 1) % N)];
                float y = a + (c - a) * f;
                float g = std::pow(10.0f, -3.0f * (d / sr) / t60);
                rvLp[i] += (y - rvLp[i]) * kd;
                o[i] = rvLp[i] * g;
                sum += o[i];
            }
            sum *= 0.25f;
            for (int i = 0; i < 8; i++) {
                auto &b = rvLine[i];
                int N = (int)b.size();
                float v = o[i] - sum + ((i & 1) ? in : in * 0.9f);
                b[(size_t)rvW[i]] = v > 3.0f ? 3.0f : v < -3.0f ? -3.0f : v;
                rvW[i] = (rvW[i] + 1) % N;
            }
            yl = 0.6f * (o[0] + o[2] + o[4] + o[6]);
            yr = 0.6f * (o[1] + o[3] + o[5] + o[7]);
            break;
        }
        case FX_RINGMOD: {
            float f;
            if (sP2 >= 64.0f && trackHz > 0.0f)                  /* tracking: a ratio to the lowest note */
                f = trackHz * std::exp2((sP1 - 128.0f) / 32.0f);
            else
                f = 20.0f * std::exp2(sP1 / 255.0f * 8.0f);
            ringPh += f * isr;
            if (ringPh >= 1.0f)
                ringPh -= ringPh >= 2.0f ? 2.0f : 1.0f;
            float c = std::sin(2.0f * PI * ringPh);
            yl = x0 * c;
            yr = x1 * c;
            break;
        }
        case FX_DISTORTION: {
            float g = 1.0f + std::pow(sP1 / 255.0f, 1.5f) * 60.0f;
            float fc = 800.0f * std::exp2(sP2 / 127.0f * 4.3f);
            float k = 1.0f - std::exp(-2.0f * PI * fc * isr);
            float lvl = 1.0f / (1.0f + 0.35f * std::sqrt(g));
            for (int c = 0; c < 2; c++) {
                float x = softT((c ? x1 : x0) * g);
                dsLp[c] += (x - dsLp[c]) * k;
                (c ? yr : yl) = dsLp[c] * lvl * 1.4f;
            }
            break;
        }
        case FX_HPF: {
            float fc = 20.0f * std::exp2(sP1 / 255.0f * 9.3f);
            float q = 0.5f + sP2 / 127.0f * 9.0f;
            float g = tanW(clampf(PI * fc * isr, 0.0f, 1.2f)), k = 1.0f / q;
            float a1 = 1.0f / (1.0f + g * (g + k)), a2 = g * a1;
            for (int c = 0; c < 2; c++) {
                float x = c ? x1 : x0;
                float v3 = x - hpS2[c], v1 = a1 * hpS1[c] + a2 * v3, v2 = hpS2[c] + g * v1;
                hpS1[c] = 2.0f * v1 - hpS1[c];
                hpS2[c] = 2.0f * v2 - hpS2[c];
                (c ? yr : yl) = softT((x - k * v1 - v2) * 0.8f) * 1.25f;
            }
            break;
        }
        default: yl = x0; yr = x1;
        }
    }
};
} // namespace rv
