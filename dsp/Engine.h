/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (REEVES) */
/* REEVES' engine: a Prophet Rev2 in the manner of PROPHET 67 (its oscillators, ladder and envelopes).
 *
 *   voices     8 or 16 (the Rev2's two models), shared by the layers: SINGLE all to A, STACK A+B and SPLIT A|B half
 *              each. Two oscillators (sawtooth, saw+tri, triangle, pulse, each with SHAPE MOD), a square sub an octave
 *              under oscillator 1, white noise, hard sync, OSC SLOP (each oscillator's own slow drift), glide (four
 *              modes, each oscillator its own rate), KEY ON/OFF and NOTE RESET per oscillator
 *   filter     a Curtis-style low-pass, 4-pole (24 dB, four saturating OTA stages, self-oscillating) or 2-pole (12 dB, a
 *              state-variable), at 4x (HIGH) or 2x (ECO), with AUDIO MOD (oscillator 1 into the cutoff, per sample)
 *   envelopes  three DADSR (LPF, VCA, AUX with REPEAT), the Rev2's digital shapes: a straight attack, curved decay
 *              and release
 *   modulation four LFOs per voice (free or KEY SYNC, CLK SYNC), eight mod slots of 22 sources and 53 destinations,
 *              the dedicated mod wheel, pressure, breath, velocity and foot routes, the gated sequencer's four tracks
 *   layers     two of everything, each with its own EFFECT, clock, ARP, poly sequencer (64 steps of up to six notes)
 *              and gated sequencer (four tracks of 16 steps, SLEW)
 *   unison     1..16 voices on a note, DETUNE, KEY MODE; CHORD memory
 *
 * The engine renders in chunks of 16 samples (the modulation's rate, 0.36 ms); notes land on the next chunk. All
 * calls from the audio thread. */
#pragma once
#include "Dsp.h"
#include "Fx.h"
#include "Params.h"
#include <atomic>

namespace rv
{
constexpr int MAXV = 16, CH = 16, NKEYS = 32;
/* the values ramped across a chunk */
enum { SM_G1, SM_G2, SM_GS, SM_GN, SM_CUT, SM_ENV, SM_RES, SM_AM, SM_AMT, SM_BASE, SM_PGM, SM_PAN, SM_INC1, SM_INC2, NSM };

class Engine
{
public:
    Engine();
    void prepare(double sampleRate);
    /* the sound: the whole program (both layers), any time (a block's start) */
    void setProgram(const Program &p);
    const Program &program() const { return prog; }
    void setParam(int layer, int p, int v);      /* one of a layer's (the host's automation) */
    void setProgramParam(int p, int v);
    void setVoices(int n);                       /* 8 or 16 */
    void setQuality(int q) { quality = q ? 1 : 0; }   /* 1 HIGH: the filters at 4x; 0 ECO: at 2x */
    int voiceCount() const { return nVoices; }

    void noteOn(int note, float vel);            /* vel 0..1 */
    void noteOff(int note);
    void sustain(bool on);
    void bend(float v);                          /* -1..1 */
    void modWheel(float v);                      /* 0..1 */
    void pressure(float v);
    void breath(float v);
    void foot(float v);
    void expression(float v);
    void allOff();
    void panic();
    /* the host's transport: playing, its position (quarters), its tempo; sync: follow it (else the layers' BPM) */
    void host(bool playing, double ppq, double bpm, bool sync);

    void render(float *L, float *R, int n);

    /* the panel's buttons that act on the engine */
    void seqPlay(int layer, bool on);            /* the poly sequencer's PLAY */
    void seqRecord(int layer, bool on);          /* RECORD: notes played go into the poly sequence */
    void seqRest(int layer);                     /* while recording: a rest, the next step */
    void seqTie(int layer);                      /* a tie */
    void seqBack(int layer);                     /* a step back */
    int seqRecStep(int layer) const { return L_[layer].recStep; }
    bool seqRecording(int layer) const { return L_[layer].rec; }
    void hold(bool on);
    bool holding() const { return holdOn; }
    void unisonChord(int layer);                 /* UNISON pressed with keys held and CHORD set: the chord kept */
    const SeqData &seqData(int layer) const { return prog.seq[layer]; }
    void setGatedStep(int layer, int track, int step, int v);

    /* for the editor */
    int voicesSounding() const;
    float voiceLevel(int v) const { return v >= 0 && v < MAXV ? V[v].e[1].v : 0.0f; }
    int voiceLayer(int v) const { return v >= 0 && v < MAXV ? V[v].layer : -1; }
    int seqStep(int layer) const;                /* the step playing (-1 none) */
    bool clockBeat(int layer) const;             /* the TAP TEMPO LED: on for the first half of a beat */
    float bpmNow(int layer) const { return L_[layer].bpmNow; }
    float lfoValue(int layer, int k) const { return L_[layer].lfoShow[k]; }

private:
    struct Env {
        float v = 0.0f, dly = 0.0f;
        uint8_t st = 0;                          /* ES_* */
    };
    enum { ES_IDLE, ES_DELAY, ES_ATT, ES_DEC, ES_REL };

    struct Voice {
        int layer = -1, note = -1, key = -1;     /* key: the id of what plays it (a key, the arp's, the seq's) */
        bool gate = false, sus = false, stepGate = true;
        float vel = 0.0f, uniDet = 0.0f;
        float p1 = 60.0f, p2 = 60.0f, t1 = 60.0f, t2 = 60.0f;   /* the oscillators' pitch now, where it glides */
        float gr1 = 0.0f, gr2 = 0.0f;            /* glide speed (semitones a second), 0: none */
        Osc o1, o2;
        BlRing subR;
        float flip = 1.0f;
        Env e[3];
        float lph[4] = {}, lsh[4] = {};
        float f[4] = {}, ffb = 0.0f;             /* the filter's stages (the 2-pole: the first two), its feedback */
        HalfBand os1, os2;                       /* its 2x and 4x */
        float sm[NSM] = {};                      /* the ramped values, where the last chunk ended */
        bool smOk = false;
        float dr1 = 0, dr2 = 0, dt1 = 0, dt2 = 0;
        int drn = 0;
        uint32_t seed = 1, age = 0;
        float dst[NMODDST] = {};
        float fade = 1.0f;
        int pnote = -1, pkey = -1;
        float pvel = 0.0f, pdet = 0.0f;
        bool pend = false;
        float panSlot = 0.0f;                    /* its place in PAN SPREAD (-1..1) */
        float press = 0.0f;
    };

    struct Layer {
        const int16_t *P = nullptr;              /* the program's layer */
        Fx fx;
        float lfoPh[4] = {}, lfoSh[4] = {}, lfoShow[4] = {};
        uint32_t lfoSeed = 0x9E3779B9u;
        /* keys held (in order), for unison, the arp */
        int8_t keys[NKEYS];
        float keyVel[NKEYS];
        int nKeys = 0;
        int lastNote = 60;
        float lastPitch = 60.0f;
        bool anyHeld = false;
        /* the clock */
        double pos = 0.0;                        /* in quarters */
        float bpmNow = 120.0f;
        long stepIdx = -1;
        bool stepHigh = false;
        float stepPhase = 0.0f;
        /* the arp */
        int8_t arpNotes[NKEYS];
        float arpVel[NKEYS];
        int arpN = 0, arpI = -1, arpDir = 1, arpRep = 0, arpOct = 0, arpPlaying = -1;
        bool arpLatched = false;
        /* the poly sequencer */
        bool playing = false, rec = false;
        int pStep = -1, recStep = 0, recHeld = 0;
        bool recFresh = true;
        uint8_t sounding[PSEQ_NOTES] = {};       /* notes the sequencer started (note + 2, 0 none) */
        /* the gated sequencer */
        int gStep = 0;
        float gVal[4] = {}, gTarget[4] = {}, gSlew[4] = {};
        bool gRunning = false;
        float outLvl = 0.0f, lastOut = 0.0f;
        float trackHz = 0.0f;
        float dcL = 0, dcR = 0, dcxL = 0, dcxR = 0;   /* the DC blocker */
        int uniN = 1;                            /* voices in the unison now */
        float fxDst[3] = {};                     /* FX MIX, P1, P2 from the newest voice's modulation */
    };

    Program prog;
    Layer L_[2];
    Voice V[MAXV];
    int nVoices = 16, quality = 1;
    float sr = 44100.0f;
    const float *tab = nullptr;
    uint32_t ageN = 0, noiseSeed = 0x2545F491u;
    float bendV = 0, mwV = 0, prV = 0, brV = 0, ftV = 0, exV = 0;
    float ctl[6] = {};                           /* the same, gliding (chunk) */
    float dcR = 0.999f;
    bool susOn = false, holdOn = false;
    bool hostPlaying = false, hostSync = true, hostWasPlaying = false;
    double hostPpq = 0.0, hostBpm = 120.0;
    float bufL[2][CH] = {}, bufR[2][CH] = {};
    float outL[CH] = {}, outR[CH] = {};
    int gpos = CH;
    float sumLvl = 0;

    /* parts */
    int cap(int layer) const;
    bool layerActive(int layer) const;
    void layerKey(int l, int note, float vel, bool down);
    void playNote(int l, int key, int note, float vel);
    void stopNote(int l, int key);
    void startVoice(Voice &v, int l, int key, int note, float vel, float det, bool retrig);
    int findVoice(int l);
    void unisonUpdate(int l, bool newKey);
    void releaseVoice(Voice &v);
    void voiceKill(Voice &v);
    void chunk();
    void clockChunk(int l);
    void arpStep(int l);
    void arpRebuild(int l);
    void polyStep(int l);
    void gatedStep(int l, bool fromKey);
    void voiceChunk(Voice &v, float *L, float *R);
    void modulate(Voice &v, Layer &ly, float *src);
    float lfoHz(const Layer &ly, int k, float extra) const;
    float stepsPerBeat(int l) const;
    void envGate(Env &e, const int16_t *P, int base, const Voice &v, int ei);
    void envChunk(Env &e, int ei, const Voice &v, const int16_t *P, float *tgt, float *k, bool *lin);
};
} // namespace rv
