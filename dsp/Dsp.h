/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (REEVES) */
/* REEVES' building blocks, from PROPHET 67's engine (the same oscillators, ladder and half-bands, so the two sound
 * alike): band-limited oscillators (a windowed sinc's step and ramp over 16 samples, each corner placed to 1/1024 of
 * a sample, -80 dB of aliasing), the low-pass as four one-poles in a loop solved without a delay with a saturating
 * input, a polyphase half-band for its 2x, and small helpers. */
#pragma once
#include <cmath>
#include <cstdint>
#include <cstring>

namespace rv
{
constexpr float PI = 3.14159265358979f;

inline float clampf(float x, float lo, float hi) { return x < lo ? lo : x > hi ? hi : x; }
inline float minf(float a, float b) { return a < b ? a : b; }
inline float maxf(float a, float b) { return a > b ? a : b; }
inline float ex2(float x) { return std::exp2(x); }
/* 2^x, fast (a polynomial, as PROPHET 67's: 2e-7 off), for what runs per sample */
inline float fexp2(float x)
{
    if (x < -126.0f)
        return 0.0f;
    if (x > 127.0f)
        x = 127.0f;
    float fl = std::floor(x), f = x - fl;
    float p = 1.534580891e-04f;
    p = p * f + 1.339993158e-03f;
    p = p * f + 9.618488972e-03f;
    p = p * f + 5.550328776e-02f;
    p = p * f + 2.402264689e-01f;
    p = p * f + 6.931472057e-01f;
    p = p * f + 1.0f;
    return std::ldexp(p, (int)fl);
}
/* a transconductor's curve: L tanh(x / L), L the level it saturates at (Pade, flat at +-3) */
inline float ota(float x)
{
    const float L = 1.25f;
    float t = x * (1.0f / L);
    if (t > 3.0f)
        return L;
    if (t < -3.0f)
        return -L;
    float t2 = t * t;
    return L * t * (27.0f + t2) / (27.0f + 9.0f * t2);
}
inline float hzOf(float midi) { return 440.0f * std::exp2((midi - 69.0f) * (1.0f / 12.0f)); }
inline float flushDn(float x) { return std::fabs(x) < 1e-15f ? 0.0f : x; }
inline float tanW(float w)                          /* tan(w) for the prewarp, w < ~1.2 */
{
    float w2 = w * w;
    return w * (1.0f + w2 * (0.33333f + w2 * (0.13333f + w2 * 0.05397f)));
}
inline uint32_t rndU(uint32_t &s)
{
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    return s;
}
inline float rndF(uint32_t &s) { return (float)(int32_t)rndU(s) * (1.0f / 2147483648.0f); }   /* -1..1 */

/* the filter's input stage: a soft clip without a division (flat at +-1.5, where it reaches 1) */
inline float sat(float x)
{
    x = clampf(x, -1.5f, 1.5f);
    return x - 0.148148f * x * x * x;
}
/* a gentle tanh (Padé, flat at +-3) */
inline float softT(float x)
{
    if (x > 3.0f)
        return 1.0f;
    if (x < -3.0f)
        return -1.0f;
    float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

/* ----------------------------------------------------------- the BLEP tables --- */
constexpr int BL_H = 8, BL_OS = 1024, BL_N = 2 * BL_H * BL_OS + 2;
constexpr uint32_t ORING = 32u;
const float *blepTables();                           /* the step's (0..BL_N) and the ramp's residual (BL_N..) */

/* A band-limited output line: corrections land in a ring around where they happen, the output is BL_H samples late */
struct BlRing {
    float ring[ORING];
    uint32_t rp;
    void clear() { std::memset(this, 0, sizeof *this); }
    /* a step of height J (ramp 0) or a change of slope J per sample (ramp 1), d samples before this one */
    inline void add(const float *tab, float J, float d, int ramp)
    {
        const float *tb = tab + (ramp ? BL_N : 0);
        float di = d * (float)BL_OS;
        int i0 = (int)di;
        if (i0 >= BL_OS)
            i0 = BL_OS - 1;
        if (i0 < 0)
            i0 = 0;
        float fr = di - (float)i0;
        for (int j = -BL_H; j < BL_H; j++) {
            const float *q = tb + (j + BL_H) * BL_OS + i0;
            float b = q[0] + fr * (q[1] - q[0]);
            if (!ramp && j >= 0)
                b -= 1.0f;
            ring[(rp + (uint32_t)j) & (ORING - 1u)] += J * b;
        }
    }
    /* this sample's naive value in, the corrected one (BL_H late) out */
    inline float out(float naive)
    {
        ring[rp & (ORING - 1u)] += naive;
        uint32_t r = (rp - (uint32_t)BL_H) & (ORING - 1u);
        float o = ring[r];
        ring[r] = 0.0f;
        rp++;
        return o;
    }
};

/* ----------------------------------------------------------- the waveforms --- */
/* The Rev2's shapes, each with SHAPE MOD (0..1, 0.5 the plain wave):
 *   SAWTOOTH   a saw with a second, smaller drop: SHAPE MOD moves where it falls (at 0.5 the plain saw; toward
 *              either end it becomes a stepped double saw, an octave's brightness): the DSI saw's 'width'
 *   SAW+TRI    the saw and the triangle mixed, SHAPE MOD tilting the triangle's peak
 *   TRIANGLE   a triangle whose peak moves (SHAPE MOD): from a ramp down through the triangle to a ramp up
 *   PULSE      the pulse's width (0.5 a square)
 * As piecewise-linear pieces: the value at a phase, and its corners (a jump in value, a change of slope) */
struct Wave {
    /* The Rev2's DCO shapes, as measured from a Rev2 (recordings of every shape at SHAPE MOD 0 / 25 / 50 / 75 / 99, notes
     * E0 to G6): SHAPE MOD is a pulse width for every shape. At 0 the plain wave; turned up, a pulse of that width is
     * added (the saw gets a second drop that moves from the end to the start, the triangle and saw+tri a step that moves
     * from the start to the end), and PULSE is that pulse alone (0: a needle, 50: about a square at 64 %, 99: a needle
     * the other way). Measured fits: within 10 to 20 % of the recordings' shape (the old model: 45 to 65 %). */
    int shape = 1;
    float pk = 0.5f;                  /* the triangle's peak */
    float sw = 0.0f, spos = 0.5f;     /* the added pulse: its step (down) and where */
    float pw = 0.5f;
    float wSaw = 0, wTri = 0, wPul = 0, dc = 0;
    void set(int shp, float m)
    {
        shape = shp;
        m = clampf(m, 0.0f, 1.0f);
        wSaw = wTri = wPul = dc = sw = 0.0f;
        /* the pulse's width (measured: 25 -> 0.27, 50 -> 0.60, 75 -> 0.93, 99 -> 0.965) */
        float w = clampf(-0.06f + 1.31f * m, 0.0f, 0.965f);
        switch (shp) {
        case 1:                               /* SAW: + 0.34 x the pulse, its drop at 1 - w */
            wSaw = 1.0f;
            if (w > 0.003f) {
                sw = 0.68f;
                spos = 1.0f - w;
            }
            break;
        case 2:                               /* SAW + TRI: rises to 0.675, falls to the wrap; + 0.6 x the pulse at w */
            wSaw = 0.45f;
            wTri = 0.697f;
            pk = 0.675f;
            if (w > 0.003f) {
                sw = 1.2f;
                spos = w;
            }
            break;
        case 3:                               /* TRIANGLE: + 0.55 x the pulse at w */
            wTri = 1.0f;
            pk = 0.5f;
            if (w > 0.003f) {
                sw = 1.1f;
                spos = w;
            }
            break;
        case 4:                               /* PULSE: 0.01 to 0.985 */
            wPul = 1.0f;
            pw = clampf(0.01f + 1.26f * m, 0.01f, 0.985f);
            dc = 2.0f * pw - 1.0f;
            break;
        default: break;
        }
    }
    inline float at(float ph) const          /* the value at a phase */
    {
        float y = 0.0f;
        if (wSaw > 0.0f)
            y += wSaw * (2.0f * ph - 1.0f);
        if (sw > 0.0f)
            y += sw * ((ph < spos ? 0.5f : -0.5f) - (spos - 0.5f));
        if (wTri > 0.0f)
            y += wTri * (ph < pk ? -1.0f + 2.0f * ph / pk : 1.0f - 2.0f * (ph - pk) / (1.0f - pk));
        if (wPul > 0.0f)
            y += (ph < pw ? 1.0f : -1.0f) - dc;
        return y;
    }
    /* the triangle's slope (per unit of phase) at ph (the saw's is the same everywhere) */
    inline float triSlope(float ph) const { return wTri * (ph < pk ? 2.0f / pk : -2.0f / (1.0f - pk)); }
};

/* An oscillator: its phase, its ring. tick() steps it a sample, a correction for every corner it passed */
struct Osc {
    float ph = 0.0f;
    BlRing r;
    void clear()
    {
        ph = 0.0f;
        r.clear();
    }
    /* the corners in (a, b] of phase, b reached 'ago' samples before this sample's end */
    inline void corners(const float *tab, const Wave &w, float a, float b, float inc, float ago)
    {
        auto cross = [&](float c, float J, float S) {
            if (a < c && b >= c) {
                float d = minf((b - c) / inc + ago, 0.999f);
                if (J != 0.0f)
                    r.add(tab, J, d, 0);
                if (S != 0.0f)
                    r.add(tab, S * inc, d, 1);
            }
        };
        if (w.wPul > 0.0f)
            cross(w.pw, -2.0f, 0.0f);
        if (w.sw > 0.0f)
            cross(w.spos, -w.sw, 0.0f);
        if (w.wTri > 0.0f)
            cross(w.pk, 0.0f, w.triSlope(w.pk) - w.triSlope(w.pk * 0.999f));
    }
    inline void edge(const float *tab, const Wave &w, float from, float inc, float d)   /* from 'from' to 0 */
    {
        r.add(tab, w.at(0.0f) - w.at(from), d, 0);
        if (w.wTri > 0.0f)
            r.add(tab, (w.triSlope(0.0f) - w.triSlope(from)) * inc, d, 1);
    }
    /* reset >= 0: hard sync, that many samples ago. wrapped (and wd, how long ago): the phase came round */
    inline float tick(const float *tab, const Wave &w, float inc, float reset, int &wrapped, float &wd)
    {
        float old = ph, p = old + inc;
        bool on = w.shape != 0;
        wrapped = 0;
        if (reset >= 0.0f) {                  /* synced: from where it got to straight back to the start */
            float pr = old + inc * (1.0f - reset);
            if (pr >= 1.0f)
                pr -= 1.0f;
            else if (on)
                corners(tab, w, old, pr, inc, reset);
            if (on)
                edge(tab, w, pr, inc, minf(reset, 0.999f));
            p = inc * reset;
            wrapped = 1;
            wd = reset;
        } else if (p >= 1.0f) {
            p -= 1.0f;
            wrapped = 1;
            wd = minf(p / inc, 0.999f);
            if (on) {
                corners(tab, w, old, 1.0f, inc, wd);
                edge(tab, w, 0.999999f, inc, wd);
                corners(tab, w, -1.0f, p, inc, 0.0f);
            }
        } else if (on) {
            corners(tab, w, old, p, inc, 0.0f);
        }
        ph = p;
        return r.out(on ? w.at(p) : 0.0f);
    }
};

/* ----------------------------------------------------------- the half-band --- */
/* a polyphase IIR half-band (two paths of four first-order allpasses; de Soras' elliptic design: -91 dB from 24.7 kHz
 * at 88.2, flat to 19.4 kHz) */
static const float HB_A0[4] = {0.0472898531f, 0.3385179585f, 0.6552730688f, 0.8745126132f};
static const float HB_A1[4] = {0.1727806110f, 0.5069776792f, 0.7764899192f, 0.9589457184f};
inline float hbAp4(float x, float *t, const float *a)
{
    float y0 = a[0] * (x - t[1]) + t[0], y1 = a[1] * (y0 - t[2]) + t[1], y2 = a[2] * (y1 - t[3]) + t[2],
          y3 = a[3] * (y2 - t[4]) + t[3];
    t[0] = x;
    t[1] = y0;
    t[2] = y1;
    t[3] = y2;
    t[4] = y3;
    return y3;
}
/* a 2x stage: up (one sample in, two out) and down (two in, one out), each through the half-band */
struct HalfBand {
    float u0[5] = {}, u1[5] = {}, d0[5] = {}, d1[5] = {}, dp = 0.0f;
    inline void up(float x, float &a, float &b)
    {
        a = hbAp4(x, u0, HB_A0);
        b = hbAp4(x, u1, HB_A1);
    }
    inline float down(float a, float b)
    {
        float y = 0.5f * (hbAp4(a, d0, HB_A0) + hbAp4(dp, d1, HB_A1));
        dp = b;
        return y;
    }
    void flush()
    {
        for (int i = 0; i < 5; i++) {
            u0[i] = flushDn(u0[i]);
            u1[i] = flushDn(u1[i]);
            d0[i] = flushDn(d0[i]);
            d1[i] = flushDn(d1[i]);
        }
        dp = flushDn(dp);
    }
};
} // namespace rv
