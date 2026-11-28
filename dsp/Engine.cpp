/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (REEVES) */
#include "Engine.h"
#include <algorithm>
#include <vector>

namespace rv
{
/* ------------------------------------------------------------------ tables --- */
const float *blepTables()
{
    /* PROPHET 67's: a Kaiser-windowed sinc (cutoff 0.9 of Nyquist, beta 8) over 16 samples, integrated once (the
     * band-limited step) and its residual once more (the ramp's), 1024 points a sample. Made once, shared */
    static const std::vector<float> t = [] {
        const int H = BL_H, OS = BL_OS, N = BL_N;
        const double fc = 0.9, beta = 8.0, pi = 3.14159265358979323846;
        auto i0 = [](double x) {
            double s = 1, term = 1;
            for (int k = 1; k < 60; k++) {
                term *= (x / (2 * k)) * (x / (2 * k));
                s += term;
            }
            return s;
        };
        std::vector<double> h(N), B(N), R(N);
        for (int i = 0; i < N; i++) {
            double x = -H + (double)i / OS, r = x / H;
            double w = std::fabs(r) <= 1 ? i0(beta * std::sqrt(1 - r * r)) / i0(beta) : 0;
            double sc = x == 0 ? 1.0 : std::sin(pi * fc * x) / (pi * fc * x);
            h[i] = fc * sc * w;
        }
        double acc = 0;
        B[0] = 0;
        for (int i = 1; i < N; i++)
            B[i] = (acc += 0.5 * (h[i - 1] + h[i]) / OS);
        double total = B[2 * H * OS];
        for (int i = 0; i < N; i++)
            B[i] /= total;
        acc = 0;
        R[0] = 0;
        for (int i = 1; i < N; i++) {
            double x0 = -H + (double)(i - 1) / OS, x1 = -H + (double)i / OS;
            double e0 = B[i - 1] - (x0 >= 0 ? 1.0 : 0.0), e1 = B[i] - (x1 >= 0 ? 1.0 : 0.0);
            if (x0 < 0 && x1 >= 0)
                e0 = B[i - 1];
            R[i] = (acc += 0.5 * (e0 + e1) / OS);
        }
        double drift = R[2 * H * OS];
        std::vector<float> out(2 * N);
        for (int i = 0; i < N; i++) {
            out[i] = (float)B[i];
            out[N + i] = (float)(R[i] - drift * std::min(1.0, (double)i / (2 * H * OS)));
        }
        out[N - 1] = 1.0f;
        out[2 * N - 1] = 0.0f;
        return out;
    }();
    return t.data();
}

namespace
{
/* each unison voice's place in the detune (cents per step of DETUNE) */
const float U_DET[MAXV] = {0.0f, -1.0f, 1.0f, -0.5f, 0.5f, -0.75f, 0.75f, -0.25f,
                           0.25f, -0.88f, 0.88f, -0.38f, 0.38f, -0.62f, 0.62f, -0.12f};
/* PAN SPREAD: each voice's side and how far (every other voice the other way) */
const float PAN_SLOT[MAXV] = {-1.0f, 1.0f, -0.6f, 0.6f, -0.85f, 0.85f, -0.4f, 0.4f,
                              -0.95f, 0.95f, -0.7f, 0.7f, -0.5f, 0.5f, -0.75f, 0.75f};
const float LFO_SYNC_STEPS[16] = {32, 16, 8, 6, 4, 3, 2, 1.5f, 1, 2.0f / 3.0f, 0.5f, 1.0f / 3.0f, 0.25f,
                                  1.0f / 6.0f, 0.125f, 0.0625f};
const float DIV_SPB[NDIVIDE] = {0.5f, 1, 2, 2, 2, 3, 4, 4, 4, 6, 8, 12, 16};   /* steps a beat */
const float DIV_SWING[NDIVIDE] = {0, 0, 0, 0.1667f, 0.3333f, 0, 0, 0.1667f, 0.3333f, 0, 0, 0, 0};

enum { K_ARP = 1000, K_UNI = 2000, K_SEQ = 3000 };
constexpr float VOICE_GAIN = 0.32f;           /* a voice's level: a four-note chord of the BASIC PROGRAM about -10 dB RMS */

inline float envSec(float v) { return v <= 0.0f ? 0.0005f : 0.0005f * std::exp2(v * (15.9f / 127.0f)); }   /* 0.5 ms .. 30 s */
inline float delaySec(float v) { return v <= 0.5f ? 0.0f : 0.001f * std::exp2(v * (13.3f / 127.0f)); }   /* .. 10 s */
inline float glideSec(float v) { return 0.0005f * std::exp2(v * (14.3f / 127.0f)); }                    /* .. 10 s */
inline float freqSemis(float a)                     /* a destination's amount (-127..127 a unit) in semitones */
{
    float x = clampf(a, -254.0f, 254.0f) * (1.0f / 127.0f);
    return 48.0f * x * std::fabs(x);
}
inline bool isFreq(int d) { return d == MD_OSC1_FREQ || d == MD_OSC2_FREQ || d == MD_OSC_FREQ; }
} // namespace

/* ------------------------------------------------------------------- setup --- */
Engine::Engine()
{
    programInit(prog);
    tab = blepTables();
    for (int l = 0; l < 2; l++)
        L_[l].P = prog.layer[l];
    for (int i = 0; i < MAXV; i++) {
        V[i].seed = 0x1234567u + 7919u * (uint32_t)i;
        V[i].panSlot = PAN_SLOT[i];
    }
}

void Engine::prepare(double sampleRate)
{
    sr = (float)sampleRate;
    for (auto &v : V) {
        uint32_t s = v.seed;
        float ps = v.panSlot;
        v = Voice();
        v.seed = s;
        v.panSlot = ps;
    }
    for (int l = 0; l < 2; l++) {
        L_[l].fx.prepare(sr);
        L_[l].P = prog.layer[l];
        L_[l].dcL = L_[l].dcR = L_[l].dcxL = L_[l].dcxR = 0.0f;
    }
    dcR = 1.0f - 2.0f * PI * 8.0f / sr;
    for (float &c : ctl)
        c = 0.0f;
    gpos = CH;
}

void Engine::setProgram(const Program &p)
{
    for (int l = 0; l < 2; l++)
        if (L_[l].playing)
            seqPlay(l, false);                  /* (a new program: the poly sequencer stops; PLAY starts it) */
    prog = p;
    for (int l = 0; l < 2; l++) {
        L_[l].P = prog.layer[l];
        if (prog.layer[l][L_SEQ_TYPE] == ST_POLY)
            prog.layer[l][L_SEQ_ON] = 0;
        L_[l].rec = false;
    }
}
void Engine::setParam(int l, int p, int v)
{
    if (l < 0 || l > 1 || p < 0 || p >= NLAYER)
        return;
    prog.layer[l][p] = (int16_t)clampf((float)v, (float)LAYER[p].lo, (float)LAYER[p].hi);
}
void Engine::setProgramParam(int p, int v)
{
    if (p < 0 || p >= NPROGRAM)
        return;
    int old = prog.prog[p];
    prog.prog[p] = (int16_t)v;
    if (p == G_LAYER_MODE && old != v)
        allOff();
}
void Engine::setVoices(int n)
{
    n = n <= 8 ? 8 : 16;
    if (n == nVoices)
        return;
    nVoices = n;
    for (int i = n; i < MAXV; i++)
        if (V[i].layer >= 0 && V[i].e[1].st != ES_IDLE)
            voiceKill(V[i]);
}

int Engine::cap(int l) const
{
    int m = prog.prog[G_LAYER_MODE];
    if (m == LM_SINGLE)
        return l == 0 ? nVoices : 0;
    return nVoices / 2;
}
bool Engine::layerActive(int l) const { return l == 0 || prog.prog[G_LAYER_MODE] != LM_SINGLE; }

/* ------------------------------------------------------------------- notes --- */
void Engine::noteOn(int note, float vel)
{
    int m = prog.prog[G_LAYER_MODE];
    if (m == LM_SPLIT)
        layerKey(note < prog.prog[G_SPLIT_POINT] ? 0 : 1, note, vel, true);
    else {
        layerKey(0, note, vel, true);
        if (m == LM_STACK)
            layerKey(1, note, vel, true);
    }
}
void Engine::noteOff(int note)
{
    for (int l = 0; l < 2; l++)                /* (both: a split point moved while a key was held) */
        layerKey(l, note, 0.0f, false);
}

void Engine::layerKey(int l, int note, float vel, bool down)
{
    Layer &ly = L_[l];
    const int16_t *P = ly.P;
    /* the keys held, in order */
    int at = -1;
    for (int i = 0; i < ly.nKeys; i++)
        if (ly.keys[i] == note)
            at = i;
    if (down) {
        if (at >= 0)
            return;
        bool wasHeld = ly.nKeys > 0;
        if (ly.nKeys < NKEYS) {
            ly.keys[ly.nKeys] = (int8_t)note;
            ly.keyVel[ly.nKeys++] = vel;
        }
        /* RECORD: into the poly sequence (and heard) */
        if (ly.rec) {
            SeqData &s = prog.seq[l];
            if (ly.recFresh) {
                for (int k = 0; k < PSEQ_NOTES; k++)
                    s.note[ly.recStep][k] = PN_REST;
                ly.recFresh = false;
            }
            for (int k = 0; k < PSEQ_NOTES; k++)
                if (s.note[ly.recStep][k] == PN_REST) {
                    s.note[ly.recStep][k] = (uint8_t)(note + 2);
                    s.vel[ly.recStep][k] = (uint8_t)clampf(vel * 127.0f, 1.0f, 127.0f);
                    break;
                }
            ly.recHeld++;
        }
        if (P[L_ARP_ON] && !(ly.playing)) {
            if (P[L_ARP_RELATCH] && holdOn && !wasHeld)
                ly.arpLatched = false;      /* a new chord: the old one goes */
            if (ly.arpLatched && !wasHeld && !holdOn)
                ly.arpLatched = false;
            arpRebuild(l);
            if (ly.arpI < 0 && !(hostSync && hostPlaying)) {   /* the first note: at once, the clock from here */
                ly.pos = 0.0;
                ly.stepIdx = 0;
                ly.stepHigh = true;
                arpStep(l);
            }
            return;
        }
        bool gated = P[L_SEQ_ON] && P[L_SEQ_TYPE] == ST_GATED;
        if (gated) {
            int mode = P[L_SEQ_MODE];
            if (mode == GS_KEYSTEP)
                gatedStep(l, true);
            else if (!wasHeld && (mode == GS_NORMAL || mode == GS_NOGATE)) {
                        ly.gStep = -1;
                ly.gRunning = false;
            }
        }
        ly.anyHeld = wasHeld;                   /* (glide's A modes: legato or not) */
        if (P[L_UNISON]) {
            unisonUpdate(l, !wasHeld);
        } else {
            playNote(l, note, note, vel);
        }
        if (gated && P[L_SEQ_MODE] != GS_KEYSTEP && !ly.gRunning) {
            ly.gRunning = true;
            if (!(hostSync && hostPlaying)) {
                ly.pos = 0.0;
                ly.stepIdx = 0;
                ly.stepHigh = true;
            }
            gatedStep(l, false);
        }
    } else {
        if (at < 0)
            return;
        for (int i = at; i < ly.nKeys - 1; i++) {
            ly.keys[i] = ly.keys[i + 1];
            ly.keyVel[i] = ly.keyVel[i + 1];
        }
        ly.nKeys--;
        if (ly.rec && ly.recHeld > 0 && --ly.recHeld == 0) {
            SeqData &s = prog.seq[l];
            ly.recStep = (ly.recStep + 1) % PSEQ_STEPS;
            s.length = (uint8_t)std::max<int>(s.length, ly.recStep == 0 ? PSEQ_STEPS : ly.recStep);
            ly.recFresh = true;
        }
        if (P[L_ARP_ON] && !ly.playing) {
            if (holdOn || susOn) {
                ly.arpLatched = true;
                return;
            }
            arpRebuild(l);
            return;
        }
        if (P[L_UNISON])
            unisonUpdate(l, false);
        else
            stopNote(l, note);
        if (ly.nKeys == 0)
            ly.gRunning = false;
    }
}

void Engine::playNote(int l, int key, int note, float vel)
{
    /* the arp's and the sequencer's notes with UNISON on: each a stack of its own */
    const int16_t *P = L_[l].P;
    int n = 1;
    float det = 0.0f;
    if (P[L_UNISON] && key >= K_ARP) {
        n = std::max(1, std::min<int>(P[L_UNI_MODE] >= 17 ? 1 : P[L_UNI_MODE], cap(l)));
        det = (float)P[L_UNI_DETUNE] * 3.0f;
        L_[l].uniN = n;
    }
    for (int j = 0; j < n; j++) {
        int i = findVoice(l);
        if (i < 0)
            return;
        startVoice(V[i], l, key, note, vel, n > 1 ? U_DET[j] * det : 0.0f, true);
    }
}

void Engine::stopNote(int l, int key)
{
    for (int i = 0; i < MAXV; i++) {
        Voice &v = V[i];
        if (v.layer != l)
            continue;
        if (v.pend && v.pkey == key) {
            v.pend = false;                      /* (the note it was taken for, let go before it began) */
            continue;
        }
        if (v.key == key && v.gate) {
            if (susOn || holdOn)
                v.sus = true;
            else
                releaseVoice(v);
            v.gate = false;
        }
    }
}

int Engine::findVoice(int l)
{
    /* within the layer's share: a free one (the longest silent), else the oldest let go, else the oldest */
    int c = cap(l), base = (prog.prog[G_LAYER_MODE] == LM_SINGLE) ? 0 : l * (nVoices / 2);
    int best = -1;
    uint32_t bestAge = 0xFFFFFFFFu;
    for (int i = base; i < base + c; i++) {
        Voice &v = V[i];
        if (v.e[1].st == ES_IDLE && !v.pend && !v.gate && v.age < bestAge) {
            best = i;
            bestAge = v.age;
        }
    }
    if (best >= 0)
        return best;
    for (int pass = 0; pass < 2 && best < 0; pass++) {
        bestAge = 0xFFFFFFFFu;
        for (int i = base; i < base + c; i++) {
            Voice &v = V[i];
            if (pass == 0 && (v.gate || v.sus))
                continue;
            if (v.age < bestAge) {
                best = i;
                bestAge = v.age;
            }
        }
    }
    return best;
}

void Engine::envGate(Env &e, const int16_t *P, int base, const Voice &v, int ei)
{
    float d = delaySec((float)P[base]);
    (void)v;
    (void)ei;
    e.dly = d;
    e.st = d > 0.0f ? ES_DELAY : ES_ATT;
}

void Engine::startVoice(Voice &v, int l, int key, int note, float vel, float det, bool retrig)
{
    const int16_t *P = L_[l].P;
    Layer &ly = L_[l];
    bool busy = v.e[1].st != ES_IDLE && v.layer >= 0;
    if (busy && (v.layer != l || v.key != key) && retrig && v.e[1].v > 0.001f) {
        /* taken while it sounds: out in 4.5 ms, then this note */
        v.pend = true;
        v.pnote = note;
        v.pkey = key;
        v.pvel = vel;
        v.pdet = det;
        if (v.fade >= 1.0f)
            v.fade = 0.9999f;
        v.layer = l;
        v.gate = false;
        v.age = ++ageN;
        return;
    }
    bool fresh = v.e[1].st == ES_IDLE;
    v.layer = l;
    v.key = key;
    v.note = note;
    v.vel = vel;
    v.uniDet = det;
    v.gate = true;
    v.sus = false;
    v.stepGate = true;
    v.age = ++ageN;
    v.fade = 1.0f;
    v.pend = false;
    /* glide: from the layer's last note (the Rev2's), or none */
    float from = ly.lastPitch;
    bool legato = ly.anyHeld;
    bool glideOn = P[L_GLIDE] && (P[L_GLIDE_MODE] == GM_RATE || P[L_GLIDE_MODE] == GM_TIME || legato);
    v.t1 = v.t2 = (float)note;
    if (glideOn && (P[L_O1_GLIDE] > 0 || P[L_O2_GLIDE] > 0)) {
        bool timeMode = P[L_GLIDE_MODE] >= GM_TIME;
        float T1 = glideSec(P[L_O1_GLIDE]), T2 = glideSec(P[L_O2_GLIDE]);
        float dist = std::fabs((float)note - from);
        if (!fresh || true) {
            v.p1 = P[L_O1_GLIDE] > 0 ? from : (float)note;
            v.p2 = P[L_O2_GLIDE] > 0 ? from : (float)note;
        }
        v.gr1 = P[L_O1_GLIDE] > 0 ? (timeMode ? maxf(dist, 0.01f) / T1 : 12.0f / T1) : 0.0f;
        v.gr2 = P[L_O2_GLIDE] > 0 ? (timeMode ? maxf(dist, 0.01f) / T2 : 12.0f / T2) : 0.0f;
    } else {
        v.p1 = v.p2 = (float)note;
        v.gr1 = v.gr2 = 0.0f;
    }
    ly.lastPitch = (float)note;
    ly.lastNote = note;
    if (retrig || fresh) {
        if (P[L_O1_RESET])
            v.o1.ph = 0.0f;
        if (P[L_O2_RESET])
            v.o2.ph = 0.0f;
        if (P[L_O1_RESET])
            v.flip = 1.0f;
        envGate(v.e[0], P, L_FDEL, v, 0);
        envGate(v.e[1], P, L_ADEL, v, 1);
        envGate(v.e[2], P, L_E3DEL, v, 2);
        /* the LFOs: KEY SYNC from the start (when no other key is held), else in step with the layer's */
        for (int k = 0; k < 4; k++) {
            const int16_t *Q = P + L_LFO1 + k * LFO_N;
            if (Q[LFO_KEY] && (fresh || !legato || P[L_UNISON] == 0)) {
                v.lph[k] = 0.0f;
                v.lsh[k] = rndF(ly.lfoSeed);
            } else if (fresh) {
                v.lph[k] = ly.lfoPh[k];
                v.lsh[k] = ly.lfoSh[k];
            }
        }
    }
    if (fresh) {
        for (float &d : v.dst)
            d = 0.0f;
        v.smOk = false;
        v.e[0].v = v.e[1].v = v.e[2].v = 0.0f;
    }
}

void Engine::releaseVoice(Voice &v)
{
    for (auto &e : v.e)
        if (e.st != ES_IDLE)
            e.st = ES_REL;
    v.sus = false;
}

void Engine::voiceKill(Voice &v)
{
    v.pend = false;
    v.gate = v.sus = false;
    if (v.fade >= 1.0f)
        v.fade = 0.9999f;
}

void Engine::unisonUpdate(int l, bool newKey)
{
    Layer &ly = L_[l];
    const int16_t *P = ly.P;
    if (ly.nKeys == 0) {
        stopNote(l, K_UNI);
        ly.anyHeld = false;
        return;
    }
    /* the note with priority */
    int km = P[L_KEY_MODE], pick = ly.nKeys - 1;
    for (int i = 0; i < ly.nKeys; i++) {
        if ((km == KM_LOW || km == KM_LOW_R) && ly.keys[i] < ly.keys[pick])
            pick = i;
        if ((km == KM_HIGH || km == KM_HIGH_R) && ly.keys[i] > ly.keys[pick])
            pick = i;
    }
    int note = ly.keys[pick];
    float vel = ly.keyVel[pick];
    bool retrig = newKey || km >= KM_LOW_R;
    /* the voices: N stacked (or the chord's notes) */
    int c = cap(l), base = prog.prog[G_LAYER_MODE] == LM_SINGLE ? 0 : l * (nVoices / 2);
    int offs[MAXV], n = 0;
    if (P[L_UNI_MODE] >= 17) {
        for (int i = 0; i < 16 && n < c; i++)
            if (prog.chord[l][i] >= 0)
                offs[n++] = prog.chord[l][i];
        if (n == 0)
            offs[n++] = 0;
    } else {
        n = std::min<int>(P[L_UNI_MODE], c);
        for (int i = 0; i < n; i++)
            offs[i] = 0;
    }
    float det = (float)P[L_UNI_DETUNE] * 3.0f;     /* cents a step */
    bool chord = P[L_UNI_MODE] >= 17;
    /* voices already on the unison: move them (legato) or strike again */
    int k = 0;
    for (int i = base; i < base + c && k < n; i++) {
        Voice &v = V[i];
        if (v.layer == l && v.key == K_UNI && (v.gate || v.sus) && !retrig) {
            float tgt = (float)(note + offs[k]);
            v.t1 = v.t2 = tgt;
            if (!(P[L_GLIDE] && (v.gr1 > 0 || v.gr2 > 0))) {
                const float d = tgt - v.p1;
                if (P[L_GLIDE] && (P[L_O1_GLIDE] || P[L_O2_GLIDE])) {
                    bool tm = P[L_GLIDE_MODE] >= GM_TIME;
                    v.gr1 = P[L_O1_GLIDE] ? (tm ? std::fabs(d) / glideSec(P[L_O1_GLIDE]) : 12.0f / glideSec(P[L_O1_GLIDE])) : 0.0f;
                    v.gr2 = P[L_O2_GLIDE] ? (tm ? std::fabs(d) / glideSec(P[L_O2_GLIDE]) : 12.0f / glideSec(P[L_O2_GLIDE])) : 0.0f;
                } else {
                    v.p1 = v.p2 = tgt;
                }
            }
            v.note = note + offs[k];
            v.gate = true;
            v.sus = false;
            k++;
        }
    }
    ly.uniN = n;
    if (k == 0) {
        /* strike: free the layer's unison voices and start n */
        for (int i = base; i < base + c; i++)
            if (V[i].layer == l && V[i].key == K_UNI && i - base >= n) {
                V[i].key = -1;
                if (V[i].gate || V[i].sus)
                    releaseVoice(V[i]);
                V[i].gate = V[i].sus = false;
            }
        for (int j = 0; j < n; j++) {
            int i = base + j;
            if (i >= MAXV)
                break;
            startVoice(V[i], l, K_UNI, note + offs[j], vel, chord ? 0.0f : U_DET[j] * det * (n > 1 ? 1.0f : 0.0f), true);
        }
    }
}

void Engine::sustain(bool on)
{
    susOn = on;
    if (!on && !holdOn)
        for (auto &v : V)
            if (v.sus) {
                releaseVoice(v);
                v.sus = false;
            }
    if (!on && !holdOn)
        for (int l = 0; l < 2; l++)
            if (L_[l].arpLatched) {
                L_[l].arpLatched = false;
                arpRebuild(l);
            }
}
void Engine::hold(bool on)
{
    holdOn = on;
    if (!on)
        sustain(susOn);
}
void Engine::bend(float v) { bendV = clampf(v, -1.0f, 1.0f); }
void Engine::modWheel(float v) { mwV = clampf(v, 0.0f, 1.0f); }
void Engine::pressure(float v) { prV = clampf(v, 0.0f, 1.0f); }
void Engine::breath(float v) { brV = clampf(v, 0.0f, 1.0f); }
void Engine::foot(float v) { ftV = clampf(v, 0.0f, 1.0f); }
void Engine::expression(float v) { exV = clampf(v, 0.0f, 1.0f); }

void Engine::allOff()
{
    for (int l = 0; l < 2; l++) {
        Layer &ly = L_[l];
        ly.nKeys = 0;
        ly.arpN = 0;
        ly.arpI = -1;
        ly.arpLatched = false;
        ly.gRunning = false;
        ly.anyHeld = false;
        ly.recHeld = 0;
        for (auto &s : ly.sounding)
            s = 0;
    }
    for (auto &v : V) {
        v.gate = v.sus = false;
        v.pend = false;
        releaseVoice(v);
    }
}
void Engine::panic()
{
    allOff();
    for (auto &v : V) {
        for (auto &e : v.e) {
            e.v = 0.0f;
            e.st = ES_IDLE;
        }
        v.layer = -1;
        v.key = -1;
        v.fade = 1.0f;
    }
    for (int l = 0; l < 2; l++) {
        L_[l].fx.clear();
        L_[l].playing = false;
    }
}

void Engine::host(bool playing, double ppq, double bpm, bool sync)
{
    hostSync = sync;
    if (bpm > 10.0)
        hostBpm = bpm;
    if (playing && !hostPlaying) {
        for (int l = 0; l < 2; l++)        /* the host starts: the poly sequencers with it (PLAY on) */
            L_[l].stepIdx = -1;
    }
    hostPlaying = playing;
    hostPpq = ppq;
    if (sync && playing)
        for (int l = 0; l < 2; l++)
            L_[l].pos = ppq;
}

/* ----------------------------------------------------------------- the sequencers --- */
void Engine::seqPlay(int l, bool on)
{
    Layer &ly = L_[l];
    if (on == ly.playing)
        return;
    ly.playing = on;
    prog.layer[l][L_SEQ_ON] = on ? 1 : 0;
    if (on) {
        ly.pStep = -1;
        if (!(hostSync && hostPlaying)) {
            ly.pos = 0.0;
            ly.stepIdx = 0;
            ly.stepHigh = true;
            polyStep(l);
        }
    } else {
        for (int k = 0; k < PSEQ_NOTES; k++)
            if (ly.sounding[k]) {
                stopNote(l, K_SEQ + k);
                ly.sounding[k] = 0;
            }
        ly.pStep = -1;
    }
}
void Engine::seqRecord(int l, bool on)
{
    Layer &ly = L_[l];
    ly.rec = on;
    if (on) {
        ly.recStep = 0;
        ly.recHeld = 0;
        ly.recFresh = true;
        prog.seq[l].length = 0;
        for (auto &st : prog.seq[l].note)
            for (auto &n : st)
                n = PN_REST;
    }
}
void Engine::seqRest(int l)
{
    Layer &ly = L_[l];
    if (!ly.rec)
        return;
    for (int k = 0; k < PSEQ_NOTES; k++)
        prog.seq[l].note[ly.recStep][k] = PN_REST;
    ly.recStep = (ly.recStep + 1) % PSEQ_STEPS;
    prog.seq[l].length = (uint8_t)std::max<int>(prog.seq[l].length, ly.recStep == 0 ? PSEQ_STEPS : ly.recStep);
    ly.recFresh = true;
}
void Engine::seqTie(int l)
{
    Layer &ly = L_[l];
    if (!ly.rec)
        return;
    for (int k = 0; k < PSEQ_NOTES; k++)
        prog.seq[l].note[ly.recStep][k] = k == 0 ? PN_TIE : PN_REST;
    ly.recStep = (ly.recStep + 1) % PSEQ_STEPS;
    prog.seq[l].length = (uint8_t)std::max<int>(prog.seq[l].length, ly.recStep == 0 ? PSEQ_STEPS : ly.recStep);
    ly.recFresh = true;
}
void Engine::seqBack(int l)
{
    Layer &ly = L_[l];
    if (ly.rec && ly.recStep > 0) {
        ly.recStep--;
        ly.recFresh = true;
    }
}
void Engine::setGatedStep(int l, int t, int s, int v)
{
    if (l < 0 || l > 1 || t < 0 || t > 3 || s < 0 || s >= GSEQ_STEPS)
        return;
    prog.seq[l].gated[t][s] = (uint8_t)clampf((float)v, 0.0f, (float)GSEQ_RESET);
}
void Engine::unisonChord(int l)
{
    Layer &ly = L_[l];
    if (ly.nKeys == 0)
        return;
    int lo = 127;
    for (int i = 0; i < ly.nKeys; i++)
        lo = std::min<int>(lo, ly.keys[i]);
    int8_t sorted[NKEYS];
    int n = ly.nKeys;
    for (int i = 0; i < n; i++)
        sorted[i] = ly.keys[i];
    std::sort(sorted, sorted + n);
    for (int i = 0; i < 16; i++)
        prog.chord[l][i] = i < n ? (int8_t)(sorted[i] - lo) : (int8_t)-1;
}

float Engine::stepsPerBeat(int l) const { return DIV_SPB[L_[l].P[L_DIVIDE]]; }

int Engine::seqStep(int l) const
{
    const Layer &ly = L_[l];
    if (ly.P[L_SEQ_TYPE] == ST_POLY)
        return ly.playing ? ly.pStep : (ly.rec ? ly.recStep : -1);
    return ly.gRunning || ly.P[L_SEQ_MODE] == GS_KEYSTEP ? ly.gStep : -1;
}
bool Engine::clockBeat(int l) const
{
    double f = L_[l].pos - std::floor(L_[l].pos);
    return f < 0.5;
}

void Engine::clockChunk(int l)
{
    Layer &ly = L_[l];
    const int16_t *P = ly.P;
    float dt = (float)CH / sr;
    ly.bpmNow = hostSync ? (float)hostBpm : (float)P[L_BPM];
    if (!(hostSync && hostPlaying))
        ly.pos += (double)(dt * ly.bpmNow / 60.0f);
    else
        ly.pos += (double)(dt * ly.bpmNow / 60.0f);   /* (from the block's start, set by host()) */
    int dv = P[L_DIVIDE];
    float spb = DIV_SPB[dv], sw = DIV_SWING[dv];
    double x = ly.pos * spb;
    long s = (long)std::floor(x);
    double f = x - (double)s;
    long eff = s;
    if ((s & 1) && f < sw)
        eff = s - 1;
    /* the step's start and length (with the swing) */
    double start = (double)eff + ((eff & 1) ? sw : 0.0), len = (eff & 1) ? 1.0 - sw : 1.0 + sw;
    if (eff < ly.stepIdx - 1 || eff > ly.stepIdx + 64)   /* (the host jumped: from here) */
        ly.stepIdx = eff - 1;
    bool arp = P[L_ARP_ON] && !ly.playing && ly.arpN > 0;
    bool poly = ly.playing;
    bool gated = P[L_SEQ_ON] && P[L_SEQ_TYPE] == ST_GATED && ly.gRunning && P[L_SEQ_MODE] != GS_KEYSTEP;
    if (eff != ly.stepIdx) {
        ly.stepIdx = eff;
        ly.stepHigh = true;
        if (arp)
            arpStep(l);
        if (poly)
            polyStep(l);
        if (gated)
            gatedStep(l, false);
    } else if (ly.stepHigh && x - start >= len * 0.5) {
        ly.stepHigh = false;
        /* the gate's second half: the arp's note let go, the poly step's (unless the next is a tie), the
         * gated sequencer's envelopes */
        if (ly.arpPlaying >= 0) {
            stopNote(l, K_ARP);
            ly.arpPlaying = -1;
        }
        if (poly && ly.pStep >= 0) {
            const SeqData &sd = prog.seq[l];
            int nx = sd.length ? (ly.pStep + 1) % sd.length : 0;
            if (sd.note[nx][0] != PN_TIE)
                for (int k = 0; k < PSEQ_NOTES; k++)
                    if (ly.sounding[k]) {
                        stopNote(l, K_SEQ + k);
                        ly.sounding[k] = 0;
                    }
        }
        if (gated && (P[L_SEQ_MODE] == GS_NORMAL || P[L_SEQ_MODE] == GS_NORESET))
            for (auto &v : V)
                if (v.layer == l && v.gate) {
                    v.stepGate = false;
                    for (auto &e : v.e)
                        if (e.st != ES_IDLE)
                            e.st = ES_REL;
                }
    }
    ly.stepPhase = (float)((x - start) / len);
}

void Engine::arpRebuild(int l)
{
    Layer &ly = L_[l];
    if (ly.arpLatched && ly.nKeys == 0)
        return;                                 /* (held: the notes stay) */
    if (ly.arpLatched && ly.nKeys > 0 && !ly.P[L_ARP_RELATCH]) {
        /* held, more keys: added */
        for (int i = 0; i < ly.nKeys; i++) {
            bool in = false;
            for (int j = 0; j < ly.arpN; j++)
                in |= ly.arpNotes[j] == ly.keys[i];
            if (!in && ly.arpN < NKEYS) {
                ly.arpNotes[ly.arpN] = ly.keys[i];
                ly.arpVel[ly.arpN++] = ly.keyVel[i];
            }
        }
        return;
    }
    ly.arpN = ly.nKeys;
    for (int i = 0; i < ly.nKeys; i++) {
        ly.arpNotes[i] = ly.keys[i];
        ly.arpVel[i] = ly.keyVel[i];
    }
    if (ly.arpN == 0) {
        ly.arpI = -1;
        if (ly.arpPlaying >= 0) {
            stopNote(l, K_ARP);
            ly.arpPlaying = -1;
        }
    }
}

void Engine::arpStep(int l)
{
    Layer &ly = L_[l];
    const int16_t *P = ly.P;
    if (ly.arpN == 0)
        return;
    if (ly.arpPlaying >= 0) {
        stopNote(l, K_ARP);
        ly.arpPlaying = -1;
    }
    /* the walk: the notes (sorted, or as played), each REPEATS times, over RANGE octaves */
    int8_t order[NKEYS];
    float ov[NKEYS];
    int n = ly.arpN;
    for (int i = 0; i < n; i++) {
        order[i] = ly.arpNotes[i];
        ov[i] = ly.arpVel[i];
    }
    int mode = P[L_ARP_MODE];
    if (mode != AM_ASSIGN)
        for (int i = 1; i < n; i++)
            for (int j = i; j > 0 && order[j] < order[j - 1]; j--) {
                std::swap(order[j], order[j - 1]);
                std::swap(ov[j], ov[j - 1]);
            }
    int oct = P[L_ARP_RANGE], rep = P[L_ARP_REPEAT];
    int total = n * oct;
    if (ly.arpRep + 1 < rep && ly.arpI >= 0) {
        ly.arpRep++;
    } else {
        ly.arpRep = 0;
        switch (mode) {
        case AM_DOWN: ly.arpI = ly.arpI <= 0 ? total - 1 : ly.arpI - 1; break;
        case AM_UPDOWN:
            if (total == 1)
                ly.arpI = 0;
            else {
                if (ly.arpI < 0) {
                    ly.arpI = 0;
                    ly.arpDir = 1;
                } else {
                    ly.arpI += ly.arpDir;
                    if (ly.arpI >= total) {
                        ly.arpI = total - 2;
                        ly.arpDir = -1;
                    } else if (ly.arpI < 0) {
                        ly.arpI = 1;
                        ly.arpDir = 1;
                    }
                }
            }
            break;
        case AM_RANDOM: ly.arpI = (int)(rndU(noiseSeed) % (uint32_t)total); break;
        default: ly.arpI = ly.arpI + 1 >= total ? 0 : ly.arpI + 1;
        }
    }
    if (ly.arpI >= total)
        ly.arpI = 0;
    int idx = mode == AM_DOWN ? ly.arpI : ly.arpI;
    int o = idx / n, k = idx % n;
    if (mode == AM_DOWN) {                       /* (down: from the top octave) */
        o = idx / n;
        k = idx % n;
    }
    int note = order[k] + 12 * o;
    if (note > 127)
        note -= 12 * ((note - 116) / 12 + 1);
    ly.arpPlaying = note;
    ly.anyHeld = false;
    playNote(l, K_ARP, note, ov[k]);
}

void Engine::polyStep(int l)
{
    Layer &ly = L_[l];
    const SeqData &s = prog.seq[l];
    if (s.length == 0)
        return;
    ly.pStep = (ly.pStep + 1) % s.length;
    const uint8_t *st = s.note[ly.pStep];
    if (st[0] == PN_TIE)
        return;                                 /* the notes before go on */
    for (int k = 0; k < PSEQ_NOTES; k++)
        if (ly.sounding[k]) {
            stopNote(l, K_SEQ + k);
            ly.sounding[k] = 0;
        }
    for (int k = 0; k < PSEQ_NOTES; k++)
        if (st[k] >= 2) {
            playNote(l, K_SEQ + k, st[k] - 2, s.vel[ly.pStep][k] / 127.0f);
            ly.sounding[k] = st[k];
        }
}

void Engine::gatedStep(int l, bool fromKey)
{
    Layer &ly = L_[l];
    const int16_t *P = ly.P;
    const SeqData &s = prog.seq[l];
    (void)fromKey;
    /* each track its own length: up to its RESET */
    ly.gStep = (ly.gStep + 1) % GSEQ_STEPS;
    int st = ly.gStep;
    for (int t = 0; t < 4; t++) {
        int len = GSEQ_STEPS;
        for (int i = 0; i < GSEQ_STEPS; i++)
            if (s.gated[t][i] >= GSEQ_RESET) {
                len = i;
                break;
            }
        if (len == 0)
            continue;
        int sp = st % len;
        float val = (float)s.gated[t][sp];
        ly.gTarget[t] = val;
    }
    /* SLEW: track 2 slews track 1, track 4 track 3 */
    ly.gSlew[0] = P[L_SEQ_D2] == MD_SLEW ? ly.gTarget[1] : 0.0f;
    ly.gSlew[2] = P[L_SEQ_D4] == MD_SLEW ? ly.gTarget[3] : 0.0f;
    ly.gSlew[1] = ly.gSlew[3] = 0.0f;
    for (int t = 0; t < 4; t++)
        if (ly.gSlew[t] <= 0.0f)
            ly.gVal[t] = ly.gTarget[t];
    /* NORMAL and NO RESET: each step strikes the envelopes of the keys held */
    int mode = P[L_SEQ_MODE];
    if (mode == GS_NORMAL || mode == GS_NORESET || mode == GS_KEYSTEP)
        for (auto &v : V)
            if (v.layer == l && v.gate) {
                v.stepGate = true;
                envGate(v.e[0], P, L_FDEL, v, 0);
                envGate(v.e[1], P, L_ADEL, v, 1);
                envGate(v.e[2], P, L_E3DEL, v, 2);
            }
}

/* --------------------------------------------------------------- modulation --- */
float Engine::lfoHz(const Layer &ly, int k, float extra) const
{
    const int16_t *Q = ly.P + L_LFO1 + k * LFO_N;
    if (Q[LFO_CLK]) {
        int i = (int)clampf((float)Q[LFO_FREQ] * 16.0f / 151.0f, 0.0f, 15.0f);
        return ly.bpmNow / 60.0f / (LFO_SYNC_STEPS[i] * 0.25f);
    }
    float v = clampf((float)Q[LFO_FREQ] + extra, 0.0f, 150.0f);
    return 0.022f * std::exp2(v * (14.47f / 150.0f));
}

static inline float lfoShape(int sh, float ph, float held)
{
    switch (sh) {
    case LS_TRI: return ph < 0.25f ? 4.0f * ph : ph < 0.75f ? 2.0f - 4.0f * ph : 4.0f * ph - 4.0f;
    case LS_SAW: return ph;
    case LS_REVSAW: return 1.0f - ph;
    case LS_SQUARE: return ph < 0.5f ? 1.0f : 0.0f;
    default: return held;
    }
}

void Engine::modulate(Voice &v, Layer &ly, float *src)
{
    const int16_t *P = ly.P;
    const float *old = v.dst;                    /* (the last chunk's: what modulates the modulation) */
    float d[NMODDST] = {};
    float semi[3] = {};                          /* the gated sequencer straight to pitch: semitones */
    /* the LFOs */
    for (int k = 0; k < 4; k++) {
        const int16_t *Q = P + L_LFO1 + k * LFO_N;
        float amt = clampf((float)Q[LFO_AMT] + old[MD_LFO1_AMT + k] + old[MD_LFO_AMT], 0.0f, 127.0f);
        d[Q[LFO_DEST]] += src[MS_LFO1 + k] * amt;
    }
    /* ENV 3 */
    {
        float vs = 1.0f - (float)P[L_E3_VEL] / 127.0f * (1.0f - v.vel);
        float amt = clampf((float)P[L_E3_AMT] + old[MD_ENV3_AMT] + old[MD_ENV_AMT], -127.0f, 127.0f);
        d[P[L_E3_DEST]] += src[MS_ENV3] * amt * vs;
    }
    /* the eight slots */
    for (int k = 0; k < 8; k++) {
        const int16_t *Q = P + L_MOD1 + k * MOD_N;
        if (Q[MOD_SRC] == MS_OFF || Q[MOD_DST] == MD_OFF)
            continue;
        float amt = clampf((float)Q[MOD_AMT] + old[MD_MOD1_AMT + k], -127.0f, 127.0f);
        d[Q[MOD_DST]] += src[Q[MOD_SRC]] * amt;
    }
    /* the dedicated ones */
    d[P[L_MW_DST]] += src[MS_MODWHEEL] * (float)P[L_MW_AMT];
    d[P[L_AT_DST]] += src[MS_PRESSURE] * (float)P[L_AT_AMT];
    d[P[L_BR_DST]] += src[MS_BREATH] * (float)P[L_BR_AMT];
    d[P[L_VEL_DST]] += src[MS_VELOCITY] * (float)P[L_VEL_AMT];
    d[P[L_FT_DST]] += src[MS_FOOT] * (float)P[L_FT_AMT];
    /* the gated sequencer's tracks, straight to their destinations */
    if (P[L_SEQ_ON] && P[L_SEQ_TYPE] == ST_GATED)
        for (int t = 0; t < 4; t++) {
            int dd = P[L_SEQ_D1 + t];
            if (dd == MD_OFF || dd >= NMODDST)
                continue;
            float val = ly.gVal[t];
            if (isFreq(dd))
                semi[dd - MD_OSC1_FREQ] += val;
            else
                d[dd] += val;
        }
    d[MD_OFF] = 0.0f;
    /* pitch: semitones (the matrix's amounts on a curve, so small amounts are a vibrato) */
    d[MD_OSC1_FREQ] = freqSemis(d[MD_OSC1_FREQ]) + semi[0];
    d[MD_OSC2_FREQ] = freqSemis(d[MD_OSC2_FREQ]) + semi[1];
    d[MD_OSC_FREQ] = freqSemis(d[MD_OSC_FREQ]) + semi[2];
    for (int i = 0; i < NMODDST; i++)
        v.dst[i] = d[i];
}

void Engine::envChunk(Env &e, int ei, const Voice &v, const int16_t *P, float *tgt, float *k, bool *lin)
{
    *lin = false;
    static const int BASE[3] = {L_FDEL, L_ADEL, L_E3DEL};
    const int16_t *Q = P + BASE[ei];
    float dt = (float)CH / sr;
    if (e.st == ES_DELAY) {
        e.dly -= dt;
        if (e.dly <= 0.0f)
            e.st = ES_ATT;
    }
    if (e.st == ES_ATT && e.v >= 0.9999f)
        e.st = ES_DEC;
    float sus = (float)Q[3] / 127.0f;
    if (ei == 2 && P[L_E3_REPEAT] && e.st == ES_DEC && std::fabs(e.v - sus) < 0.004f + 0.02f * (1.0f - sus) * 0.0f) {
        e.dly = delaySec((float)Q[0]);           /* REPEAT: delay, attack and decay again */
        e.st = e.dly > 0.0f ? ES_DELAY : ES_ATT;
    }
    float mt = 0.0f;
    switch (e.st) {
    case ES_ATT: {
        mt = v.dst[MD_FENV_ATT + ei] + v.dst[MD_ENV_ATT];
        float T = envSec(clampf((float)Q[1] + mt, 0.0f, 127.0f));
        *tgt = 1.0f;                             /* (the Rev2's attack: a straight line, digital and quick) */
        *k = 1.0f / (T * sr);
        *lin = true;
        break;
    }
    case ES_DEC: {
        mt = v.dst[MD_FENV_DEC + ei] + v.dst[MD_ENV_DEC];
        float T = envSec(clampf((float)Q[2] + mt, 0.0f, 127.0f));
        *tgt = sus;
        *k = 1.0f - std::exp(-4.6f / (T * sr));
        break;
    }
    case ES_REL: {
        mt = v.dst[MD_FENV_REL + ei] + v.dst[MD_ENV_REL];
        float T = envSec(clampf((float)Q[4] + mt, 0.0f, 127.0f));
        *tgt = 0.0f;
        *k = 1.0f - std::exp(-4.6f / (T * sr));
        break;
    }
    default:
        *tgt = e.st == ES_DELAY ? e.v : 0.0f;
        *k = e.st == ES_DELAY ? 0.0f : 1.0f;
    }
}

/* --------------------------------------------------------------- the voice --- */
void Engine::voiceChunk(Voice &v, float *Lo, float *Ro)
{
    Layer &ly = L_[v.layer];
    const int16_t *P = ly.P;
    const float dt = (float)CH / sr, isr = 1.0f / sr;
    /* the sources */
    float src[NMODSRC];
    src[MS_OFF] = 0.0f;
    for (int t = 0; t < 4; t++)
        src[MS_SEQ1 + t] = (P[L_SEQ_ON] && P[L_SEQ_TYPE] == ST_GATED) ? ly.gVal[t] / 125.0f : 0.0f;
    for (int k = 0; k < 4; k++) {
        const int16_t *Q = P + L_LFO1 + k * LFO_N;
        float hz = lfoHz(ly, k, v.dst[MD_LFO1_FREQ + k] + v.dst[MD_LFO_FREQ]);
        float ph = v.lph[k] + hz * dt;
        if (ph >= 1.0f) {
            ph -= std::floor(ph);
            v.lsh[k] = rndF(v.seed);
        }
        v.lph[k] = ph;
        src[MS_LFO1 + k] = lfoShape(Q[LFO_SHAPE], ph, v.lsh[k]);
    }
    src[MS_ENV_LPF] = v.e[0].v;
    src[MS_ENV_VCA] = v.e[1].v;
    src[MS_ENV3] = v.e[2].v;
    src[MS_PITCHBEND] = ctl[0];
    src[MS_MODWHEEL] = ctl[1];
    src[MS_PRESSURE] = ctl[2];
    src[MS_BREATH] = ctl[3];
    src[MS_FOOT] = ctl[4];
    src[MS_EXPRESSION] = ctl[5];
    src[MS_VELOCITY] = v.vel;
    src[MS_NOTE] = (float)v.note / 127.0f;
    src[MS_NOISE] = rndF(v.seed);
    src[MS_DC] = 1.0f;
    src[MS_AUDIO] = ly.lastOut;
    modulate(v, ly, src);
    const float *d = v.dst;

    /* OSC SLOP: each oscillator's slow wander */
    float slop = clampf((float)P[L_SLOP] + d[MD_SLOP], 0.0f, 127.0f) / 127.0f;
    if (slop > 0.0f) {
        if (--v.drn <= 0) {
            v.dt1 = rndF(v.seed) * 14.0f * slop;
            v.dt2 = rndF(v.seed) * 14.0f * slop;
            v.drn = (int)((0.2f + 0.4f * (float)(rndU(v.seed) & 1023u) / 1023.0f) / dt);
        }
        float kd = 1.0f - std::exp(-dt / 0.25f);
        v.dr1 += (v.dt1 - v.dr1) * kd;
        v.dr2 += (v.dt2 - v.dr2) * kd;
    } else {
        v.dr1 = v.dr2 = 0.0f;
    }
    /* glide */
    if (v.gr1 > 0.0f) {
        float st = v.gr1 * dt;
        v.p1 = v.p1 < v.t1 ? minf(v.p1 + st, v.t1) : maxf(v.p1 - st, v.t1);
    } else
        v.p1 = v.t1;
    if (v.gr2 > 0.0f) {
        float st = v.gr2 * dt;
        v.p2 = v.p2 < v.t2 ? minf(v.p2 + st, v.t2) : maxf(v.p2 - st, v.t2);
    } else
        v.p2 = v.t2;
    float bendS = ctl[0] * (float)P[L_PB_RANGE];
    float k1 = P[L_O1_KEY] ? v.p1 : 60.0f, k2 = P[L_O2_KEY] ? v.p2 : 60.0f;
    float n1 = k1 + (float)(P[L_O1_FREQ] - 36) + P[L_O1_FINE] * 0.01f + (v.dr1 + v.uniDet) * 0.01f + bendS + d[MD_OSC1_FREQ] + d[MD_OSC_FREQ];
    float n2 = k2 + (float)(P[L_O2_FREQ] - 36) + P[L_O2_FINE] * 0.01f + (v.dr2 - v.uniDet * 0.7f) * 0.01f + bendS + d[MD_OSC2_FREQ] + d[MD_OSC_FREQ];
    float inc1 = minf(hzOf(n1) * isr, 0.45f), inc2 = minf(hzOf(n2) * isr, 0.45f);
    Wave w1, w2;
    w1.set(P[L_O1_SHAPE], ((float)P[L_O1_SHMOD] + (d[MD_OSC1_SHAPE] + d[MD_OSC_SHAPE]) * (99.0f / 127.0f)) / 99.0f);
    w2.set(P[L_O2_SHAPE], ((float)P[L_O2_SHMOD] + (d[MD_OSC2_SHAPE] + d[MD_OSC_SHAPE]) * (99.0f / 127.0f)) / 99.0f);
    bool sync = P[L_SYNC] != 0;
    /* the envelopes' targets this chunk (the attack a straight line, decay and release curves: the Rev2's) */
    float et[3], ek[3];
    bool el[3];
    for (int i = 0; i < 3; i++)
        envChunk(v.e[i], i, v, P, &et[i], &ek[i], &el[i]);
    /* the filter: cutoff in its units (0..164 over ten octaves from 20 Hz) */
    float famt = clampf((float)P[L_FENV_AMT] + d[MD_FENV_AMT] + d[MD_ENV_AMT], -127.0f, 127.0f);
    float cutU = (float)P[L_CUTOFF] + d[MD_CUTOFF] * (164.0f / 127.0f) + v.vel * (float)P[L_FVEL] * 0.6f;
    float oct0 = cutU * (10.0f / 164.0f) + ((float)P[L_KEYAMT] / 64.0f) * (v.p1 - 60.0f) / 12.0f;
    float envOct = famt / 127.0f * 9.0f;
    float reso = clampf((float)P[L_RESO] + d[MD_RESO], 0.0f, 127.0f) / 127.0f;
    bool pole4 = P[L_POLE4] != 0;
    /* the amplifier */
    float avs = 1.0f - (float)P[L_AVEL] / 127.0f * (1.0f - v.vel);
    float pgm = (float)P[L_PGM_VOL] / 100.0f * VOICE_GAIN;
    if ((v.key == K_UNI || (v.key >= K_ARP && P[L_UNISON])) && ly.uniN > 1)
        pgm *= 1.0f / std::sqrt((float)ly.uniN);   /* (a stack as loud as a fat note, not N notes) */
    /* the pan */
    float pan;
    if (P[L_PAN_MODE] == 0)
        pan = v.panSlot * clampf((float)P[L_PAN_SPREAD] + d[MD_PAN], 0.0f, 127.0f) / 127.0f;
    else
        pan = v.panSlot * (float)P[L_PAN_SPREAD] / 127.0f + d[MD_PAN] / 127.0f;
    pan = clampf(pan, -1.0f, 1.0f);
    /* everything a knob or a modulation moves, ramped across the chunk from where the last one ended: no zipper,
     * no click, at any speed of turning (SM_*) */
    float tgt[NSM];
    float mix = clampf((float)P[L_OSC_MIX] + d[MD_OSC_MIX], 0.0f, 127.0f) / 127.0f;
    tgt[SM_G1] = 0.5f * minf(1.0f, 2.0f * (1.0f - mix)) * clampf(1.0f + d[MD_OSC1_LEVEL] / 127.0f, 0.0f, 2.0f);
    tgt[SM_G2] = 0.5f * minf(1.0f, 2.0f * mix);
    /* (measured: at 100 the Rev2's sub is 5.5 dB under the saw's fundamental) */
    tgt[SM_GS] = 0.148f * clampf((float)P[L_SUB] + d[MD_SUB], 0.0f, 127.0f) / 127.0f;
    tgt[SM_GN] = 0.6f * clampf((float)P[L_NOISE] + d[MD_NOISE], 0.0f, 127.0f) / 127.0f * std::sqrt(sr / 44100.0f);
    tgt[SM_CUT] = oct0;
    tgt[SM_ENV] = envOct;
    tgt[SM_RES] = reso;
    tgt[SM_AM] = clampf((float)P[L_AUDIOMOD] + d[MD_AUDIOMOD], 0.0f, 127.0f) / 127.0f * 4.0f;   /* octaves */
    tgt[SM_AMT] = clampf((float)P[L_AENV_AMT] + d[MD_AENV_AMT] + d[MD_ENV_AMT], 0.0f, 127.0f) / 127.0f * avs;
    tgt[SM_BASE] = clampf((float)P[L_VCA_LEVEL] / 127.0f + d[MD_VCA] / 127.0f, 0.0f, 1.0f);
    tgt[SM_PGM] = pgm;
    tgt[SM_PAN] = pan;
    tgt[SM_INC1] = inc1;
    tgt[SM_INC2] = inc2;
    if (!v.smOk) {
        for (int i = 0; i < NSM; i++)
            v.sm[i] = tgt[i];
        v.smOk = true;
    }
    float cur[NSM], stp[NSM];
    const float inv = 1.0f / (float)CH;
    for (int i = 0; i < NSM; i++) {
        cur[i] = v.sm[i];
        stp[i] = (tgt[i] - v.sm[i]) * inv;
        v.sm[i] = tgt[i];
    }
    const float steal = 1.0f / (0.0045f * sr);
    /* the filter's rate: HIGH 4x, ECO 2x (each halved from 88.2 kHz up) */
    int os = quality ? 4 : 2;
    if (sr > 80000.0f)
        os /= 2;
    if (sr > 160000.0f)
        os = 1;
    const float fos = sr * (float)os, wmax = 0.42f * fos;
    const float wk = 2.0f * PI / fos;
    /* a filter step: the Curtis 4-pole (four OTA stages, the resonance taken from the last, averaged over two
     * samples; each stage's transconductor saturates, as the chip's do: the resonance rounds off and the stages slew
     * when driven hard; part of the bass the resonance takes given back), or the 2-pole (a state variable, its band
     * saturating) */
    auto f4 = [&v](float x, float g, float k) {
        float fb = 0.5f * (v.f[3] + v.ffb);
        v.ffb = v.f[3];
        float u = sat(x - 4.0f * k * fb);
        v.f[0] += g * ota(u - v.f[0]);
        v.f[1] += g * ota(v.f[0] - v.f[1]);
        v.f[2] += g * ota(v.f[1] - v.f[2]);
        v.f[3] += g * ota(v.f[2] - v.f[3]);
        return v.f[3];
    };
    auto f2 = [&v](float x, float g, float kd) {
        float a1 = 1.0f / (1.0f + g * (g + kd)), a2 = g * a1;
        float v3 = sat(x) - v.f[1], v1 = a1 * v.f[0] + a2 * v3, v2 = v.f[1] + g * v1;
        v.f[0] = softT(2.0f * v1 - v.f[0]);
        v.f[1] = 2.0f * v2 - v.f[1];
        return v2;
    };

    for (int s = 0; s < CH; s++) {
        for (int i = 0; i < NSM; i++)
            cur[i] += stp[i];
        for (int i = 0; i < 3; i++)
            v.e[i].v = el[i] ? minf(v.e[i].v + ek[i], 1.0f) : minf(v.e[i].v + (et[i] - v.e[i].v) * ek[i], 1.0f);
        float ae = v.e[1].v;
        if (v.fade < 1.0f) {                     /* taken: out on a smooth curve, then the note it was taken for */
            v.fade -= steal;
            if (v.fade <= 0.0f) {
                v.fade = 1.0f;
                for (auto &e : v.e) {
                    e.v = 0.0f;
                    e.st = ES_IDLE;
                }
                ae = 0.0f;
                if (v.pend) {
                    v.pend = false;
                    int l = v.layer;
                    startVoice(v, l, v.pkey, v.pnote, v.pvel, v.pdet, true);
                    for (int i = 0; i < 3; i++)
                        envChunk(v.e[i], i, v, P, &et[i], &ek[i], &el[i]);
                } else {
                    v.layer = -1;
                    v.key = -1;
                }
                continue;
            }
            float f = v.fade;
            ae *= f * f * (3.0f - 2.0f * f);
        }
        int wr1 = 0, wr2 = 0;
        float wd1 = 0.0f, wd2 = 0.0f;
        float o2 = v.o2.tick(tab, w2, cur[SM_INC2], -1.0f, wr2, wd2);
        float o1 = v.o1.tick(tab, w1, cur[SM_INC1], sync && wr2 ? wd2 : -1.0f, wr1, wd1);
        /* the sub: a square an octave under oscillator 1, its edges band-limited as the oscillators' */
        if (wr1) {
            v.flip = -v.flip;
            v.subR.add(tab, 2.0f * v.flip, minf(wd1, 0.999f), 0);
        }
        float sub = v.subR.out(v.flip);
        float x = cur[SM_G1] * o1 + cur[SM_G2] * o2 + cur[SM_GS] * sub;
        x += (cur[SM_GN] + 2e-5f) * rndF(v.seed);   /* (and the circuit's own hiss: what starts a filter singing) */
        /* the low-pass, oversampled */
        float oct = cur[SM_CUT] + cur[SM_ENV] * v.e[0].v + cur[SM_AM] * o1;
        float hz = minf(20.0f * fexp2(clampf(oct, -1.5f, 10.5f)), wmax);
        float res = cur[SM_RES], y;
        if (pole4) {
            /* (the cutoff as a one-pole's coefficient; tuned a little up as the explicit stages lag) */
            float g = 1.0f - fexp2(-wk * hz * 1.4427f * 1.06f);
            float k = res * 1.22f;               /* (from about 0.85 of RESONANCE it sings on its own) */
            if (os == 4) {
                float a, b, a0, a1, b0, b1;
                v.os1.up(x, a, b);
                v.os2.up(a, a0, a1);
                v.os2.up(b, b0, b1);
                a0 = f4(a0, g, k);
                a1 = f4(a1, g, k);
                b0 = f4(b0, g, k);
                b1 = f4(b1, g, k);
                /* (one after the other: in one expression the compiler may run the second first, MSVC does) */
                float da = v.os2.down(a0, a1);
                float db = v.os2.down(b0, b1);
                y = v.os1.down(da, db);
            } else if (os == 2) {
                float a, b;
                v.os1.up(x, a, b);
                float fa = f4(a, g, k);
                float fb = f4(b, g, k);
                y = v.os1.down(fa, fb);
            } else
                y = f4(x, g, k);
            y *= 1.0f + 0.45f * k;               /* (the bass the resonance takes, partly given back) */
        } else {
            float g = tanW(minf(PI * hz / fos, 1.2f)), kd = 2.0f - 1.9f * res;
            if (os == 4) {
                float a, b, a0, a1, b0, b1;
                v.os1.up(x, a, b);
                v.os2.up(a, a0, a1);
                v.os2.up(b, b0, b1);
                a0 = f2(a0, g, kd);
                a1 = f2(a1, g, kd);
                b0 = f2(b0, g, kd);
                b1 = f2(b1, g, kd);
                /* (one after the other: in one expression the compiler may run the second first, MSVC does) */
                float da = v.os2.down(a0, a1);
                float db = v.os2.down(b0, b1);
                y = v.os1.down(da, db);
            } else if (os == 2) {
                float a, b;
                v.os1.up(x, a, b);
                float fa = f2(a, g, kd);
                float fb = f2(b, g, kd);
                y = v.os1.down(fa, fb);
            } else
                y = f2(x, g, kd);
            y *= 1.0f / (1.0f + 0.6f * res);
        }
        float amp = clampf(cur[SM_BASE] + cur[SM_AMT] * ae, 0.0f, 1.0f);
        if (cur[SM_BASE] > 0.0f && v.fade < 1.0f)
            amp *= v.fade;
        float out = y * amp * cur[SM_PGM] * 0.9f;
        float p0 = cur[SM_PAN];
        float gl = minf(1.0f, 1.0f - p0), gr = minf(1.0f, 1.0f + p0);
        Lo[s] += out * gl;
        Ro[s] += out * gr;
    }
    /* finished: its amp envelope at its end, its key let go */
    if (v.e[1].st == ES_REL && v.e[1].v < 3.2e-4f && !v.pend) {
        for (auto &e : v.e) {
            e.v = 0.0f;
            e.st = ES_IDLE;
        }
        if (!v.gate)
            v.layer = -1;
    }
    for (float &f : v.f)
        f = flushDn(f);
    v.ffb = flushDn(v.ffb);
    v.os1.flush();
    v.os2.flush();
}

/* ------------------------------------------------------------------ render --- */
void Engine::chunk()
{
    const float dt = (float)CH / sr;
    /* the wheels, the pressure and the pedals glide (5 ms): no steps in the pitch, no crackle in a filter */
    {
        const float in[6] = {bendV, mwV, prV, brV, ftV, exV}, kc = 1.0f - std::exp(-dt / 0.005f);
        for (int i = 0; i < 6; i++)
            ctl[i] = flushDn(ctl[i] + (in[i] - ctl[i]) * kc);
    }
    for (int l = 0; l < 2; l++) {
        Layer &ly = L_[l];
        std::memset(bufL[l], 0, sizeof bufL[l]);
        std::memset(bufR[l], 0, sizeof bufR[l]);
        if (!layerActive(l) && l == 1) {
            /* (layer B off: its effect's tail still out) */
        }
        clockChunk(l);
        /* the layer's free-running LFOs (the voices without KEY SYNC start in step with these) */
        for (int k = 0; k < 4; k++) {
            const int16_t *Q = ly.P + L_LFO1 + k * LFO_N;
            if (Q[LFO_CLK]) {
                int i = (int)clampf((float)Q[LFO_FREQ] * 16.0f / 151.0f, 0.0f, 15.0f);
                double cyc = LFO_SYNC_STEPS[i] * 0.25;
                double ph = ly.pos / cyc;
                float nph = (float)(ph - std::floor(ph));
                if (nph < ly.lfoPh[k])
                    ly.lfoSh[k] = rndF(ly.lfoSeed);
                ly.lfoPh[k] = nph;
            } else {
                ly.lfoPh[k] += lfoHz(ly, k, 0.0f) * dt;
                if (ly.lfoPh[k] >= 1.0f) {
                    ly.lfoPh[k] -= std::floor(ly.lfoPh[k]);
                    ly.lfoSh[k] = rndF(ly.lfoSeed);
                }
            }
            ly.lfoShow[k] = lfoShape(Q[LFO_SHAPE], ly.lfoPh[k], ly.lfoSh[k]);
        }
        /* the gated sequencer's slew */
        for (int t = 0; t < 4; t++) {
            if (ly.gSlew[t] > 0.0f) {
                float T = 0.002f * std::exp2(ly.gSlew[t] * (10.0f / 125.0f));
                ly.gVal[t] += (ly.gTarget[t] - ly.gVal[t]) * (1.0f - std::exp(-dt / T));
            }
        }
    }
    /* the voices */
    int newest[2] = {-1, -1};
    uint32_t na[2] = {0, 0};
    float trackLo[2] = {1e9f, 1e9f};
    for (int i = 0; i < MAXV; i++) {
        Voice &v = V[i];
        if (v.layer < 0)
            continue;
        if (v.e[1].st == ES_IDLE && !v.pend && v.fade >= 1.0f) {
            if (!v.gate)
                v.layer = -1;
            continue;
        }
        int l = v.layer;
        voiceChunk(v, bufL[l], bufR[l]);
        if (v.layer >= 0 && v.age >= na[l]) {
            na[l] = v.age;
            newest[l] = i;
        }
        if (v.layer >= 0 && v.gate)
            trackLo[l] = minf(trackLo[l], (float)v.note);
    }
    /* the effects, each layer */
    for (int l = 0; l < 2; l++) {
        Layer &ly = L_[l];
        const int16_t *P = ly.P;
        if (newest[l] >= 0) {
            const float *d = V[newest[l]].dst;
            ly.fxDst[0] = d[MD_FX_MIX];
            ly.fxDst[1] = d[MD_FX_P1];
            ly.fxDst[2] = d[MD_FX_P2];
        }
        ly.fx.set(P[L_FX_ON] != 0, P[L_FX_TYPE], (float)P[L_FX_MIX] + ly.fxDst[0], (float)P[L_FX_P1] + ly.fxDst[1] * 2.0f,
                  (float)P[L_FX_P2] + ly.fxDst[2], P[L_FX_CLK] != 0, ly.bpmNow, trackLo[l] < 1e8f ? hzOf(trackLo[l]) : 0.0f);
        /* the layer's DC let go (8 Hz: a hard-driven filter or the sub never pushes the sum off the middle) */
        for (int i = 0; i < CH; i++) {
            float xl = bufL[l][i], xr = bufR[l][i];
            ly.dcL = xl - ly.dcxL + dcR * ly.dcL;
            ly.dcR = xr - ly.dcxR + dcR * ly.dcR;
            ly.dcxL = xl;
            ly.dcxR = xr;
            bufL[l][i] = ly.dcL;
            bufR[l][i] = ly.dcR;
        }
        ly.dcL = flushDn(ly.dcL);
        ly.dcR = flushDn(ly.dcR);
        ly.fx.process(bufL[l], bufR[l], CH);
        ly.lastOut = 0.5f * (bufL[l][CH - 1] + bufR[l][CH - 1]);
    }
    for (int s = 0; s < CH; s++) {
        outL[s] = bufL[0][s] + bufL[1][s];
        outR[s] = bufR[0][s] + bufR[1][s];
    }
}

void Engine::render(float *L, float *R, int n)
{
    while (n > 0) {
        if (gpos >= CH) {
            chunk();
            gpos = 0;
        }
        int k = std::min(CH - gpos, n);
        std::memcpy(L, outL + gpos, sizeof(float) * (size_t)k);
        std::memcpy(R, outR + gpos, sizeof(float) * (size_t)k);
        gpos += k;
        L += k;
        R += k;
        n -= k;
    }
}

int Engine::voicesSounding() const
{
    int n = 0;
    for (const auto &v : V)
        n += v.layer >= 0 && v.e[1].st != ES_IDLE;
    return n;
}
} // namespace rv
