/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (REEVES) */
/* The MPC skin's pictures, drawn with the VST3's own art (src/ui/Look.cpp: the powder-coated panel, the knobs, the
 * buttons, the LEDs, the section frames, the legends, the maker's tag), on the MPC's six pages (its function keys;
 * a second press: the same page for layer B):
 *
 *   OSC      OSCILLATORS / GLIDE, KEYS, UNISON
 *   FILTER   LOW-PASS FILTER / AMPLIFIER, VOICE
 *   MOD      the eight MODULATION slots (source, destination, amount)
 *   LFO      LFO 1, LFO 2 / LFO 3, LFO 4
 *   ENV3/FX  AUXILIARY ENVELOPE / EFFECT
 *   PLAY     ARP, CLOCK, SEQUENCER / LAYERS, VOICES, QUALITY, HOLD, BEND, VOLUME
 *
 * Each page is drawn at the MPC's 1280 x 628 and cut up: its background (the face, the frames, the legends), and
 * every control at every value it can take (a knob's film strip, a button's look for each value with the value a
 * press gives, an LED list's rows lit and unlit). manifest.json says what each is, for make_skin.py.
 *
 *   reeves_skin_render <out folder> */
#include "Params.h"
#include "ui/Look.h"
#include <juce_gui_basics/juce_gui_basics.h>

using namespace juce;
using namespace rvui;
using namespace rv;

namespace {
constexpr int TW = 1280, TH = 628;
constexpr float KS = 74.0f;              /* a knob */
constexpr float BW = 52.0f, BH = 28.0f;  /* a button */
constexpr float LR = 21.0f;              /* an LED row */

/* a control on a page: which parameter (the VST's id without its layer: "cutoff"; or a global one), where */
struct Ctl {
    enum Kind { knob, button, leds } kind;
    String id;                           /* a layer's: LAYER[].id; else "layer_mode", "voices", ... */
    float x, y;                          /* the centre (knob, button) or the top left (LEDs) */
    String legend;
    StringArray names;                   /* LEDs: each row's name */
    float w = 0.0f;                      /* LEDs: the rows' width */
    bool cycle = false;                  /* a button that steps through the values (else a switch) */
    float rh = LR;                       /* LEDs: a row's height */
};
struct Sec {
    Rectangle<float> r;
    String title;
};
struct Page {
    String name;
    std::vector<Sec> secs;
    std::vector<Ctl> ctls;
    std::vector<std::pair<Point<float>, String>> notes;   /* small print */
};

int layerIndex(const String &id)
{
    for (int i = 0; i < NLAYER; i++)
        if (id == LAYER[i].id)
            return i;
    return -1;
}
struct Range {
    int lo, hi;
    StringArray names;
};
Range rangeOf(const String &id)
{
    int p = layerIndex(id);
    if (p >= 0) {
        const ParamDef &d = LAYER[p];
        Range r{d.lo, d.hi, {}};
        if (d.kind == K_CHOICE || d.kind == K_ONOFF)
            for (int k = d.lo; k <= d.hi; k++)
                r.names.add(d.choice[k]);
        return r;
    }
    if (id == "layer_mode")
        return {0, 2, {"Single", "Stack A+B", "Split A|B"}};
    if (id == "split_point")
        return {0, 127, {}};
    if (id == "voices")
        return {0, 1, {"8 Voice", "16 Voice"}};
    if (id == "quality")
        return {0, 1, {"ECO", "HIGH"}};
    if (id == "hold")
        return {0, 1, {"Off", "On"}};
    if (id == "vol")
        return {0, 127, {}};
    std::fprintf(stderr, "unknown %s\n", id.toRawUTF8());
    std::exit(1);
}
bool bipolarOf(const String &id)
{
    int p = layerIndex(id);
    return p >= 0 && LAYER[p].lo < 0;
}

/* ------------------------------------------------------------------------------- the pages --- */
struct Builder {
    Page pg;
    void sec(float x0, float y0, float x1, float y1, const char *t) { pg.secs.push_back({{x0, y0, x1 - x0, y1 - y0}, t}); }
    void K(float x, float y, const char *id, const char *leg) { pg.ctls.push_back({Ctl::knob, id, x, y, leg, {}, 0.0f, false}); }
    void B(float x, float y, const char *id, const char *leg, bool cycle = false) { pg.ctls.push_back({Ctl::button, id, x, y, leg, {}, 0.0f, cycle}); }
    void L(float x, float y, const char *id, float w, StringArray names = {}, float rh = LR)
    {
        auto r = rangeOf(id);
        pg.ctls.push_back({Ctl::leds, id, x, y, "", names.isEmpty() ? r.names : names, w, false, rh});
    }
    void note(float x, float y, const char *t) { pg.notes.push_back({{x, y}, t}); }
};

/* rows: the top band's knob lines at 78 and 206, the bottom band's at 366 and 494 */
std::vector<Page> pages()
{
    std::vector<Page> ps;
    const float T1 = 80, T2 = 208, B1 = 368, B2 = 496;
    {
        Builder b;
        b.pg.name = "OSC";
        b.sec(14, 16, 1266, 284, "OSCILLATORS");
        b.K(80, T1, "o1_freq", "OSC 1 FREQ");
        b.K(200, T1, "o1_fine", "FINE TUNE");
        b.B(304, T1 - 8, "o1_shape", "SHAPE", true);
        b.L(344, T1 - 50, "o1_shape", 120);
        b.K(560, T1, "o1_shmod", "SHAPE MOD");
        b.K(690, T1, "sub", "SUB OCTAVE");
        b.K(820, T1, "noise", "NOISE");
        b.K(80, T2, "o2_freq", "OSC 2 FREQ");
        b.K(200, T2, "o2_fine", "FINE TUNE");
        b.B(304, T2 - 8, "o2_shape", "SHAPE", true);
        b.L(344, T2 - 50, "o2_shape", 120);
        b.K(560, T2, "o2_shmod", "SHAPE MOD");
        b.K(690, T2, "osc_mix", "OSC MIX");
        b.K(820, T2, "slop", "OSC SLOP");
        b.B(960, T1 - 8, "sync", "SYNC");
        b.B(1060, T1 - 8, "o1_key", "OSC 1 KEY");
        b.B(1160, T1 - 8, "o2_key", "OSC 2 KEY");
        b.B(1060, T2 - 8, "o1_reset", "OSC 1 RESET");
        b.B(1160, T2 - 8, "o2_reset", "OSC 2 RESET");
        b.sec(14, 304, 640, 572, "GLIDE");
        b.B(80, B1 - 8, "glide", "GLIDE");
        b.K(200, B1, "o1_glide", "OSC 1 RATE");
        b.K(330, B1, "o2_glide", "OSC 2 RATE");
        b.K(470, B1, "glide_mode", "MODE");
        b.L(540, B1 - 44, "glide_mode", 96, {"Rate", "Rate A", "Time", "Time A"});
        b.K(200, B2, "pb_range", "BEND RANGE");
        b.sec(652, 304, 1266, 572, "UNISON");
        b.B(720, B1 - 8, "unison", "UNISON");
        b.K(840, B1, "uni_mode", "VOICES");
        b.K(970, B1, "uni_detune", "DETUNE");
        b.K(1100, B1, "key_mode", "KEY MODE");
        b.L(1150, B1 - 64, "key_mode", 110, {"Low", "High", "Last", "Low R", "High R", "Last R"});
        b.note(840, B2 - 10, "16 voices, or CHORD:");
        b.note(840, B2 + 12, "the chord held when UNISON went on");
        ps.push_back(b.pg);
    }
    {
        Builder b;
        b.pg.name = "FILTER";
        b.sec(14, 16, 1266, 284, "LOW-PASS FILTER");
        const char *top[6][2] = {{"cutoff", "CUTOFF"}, {"reso", "RESONANCE"}, {"fenv_amt", "ENV AMOUNT"}, {"fvel", "VELOCITY"},
                                 {"keyamt", "KEY AMOUNT"}, {"audiomod", "AUDIO MOD"}};
        for (int i = 0; i < 6; i++)
            b.K(110.0f + 205.0f * (float)i, T1, top[i][0], top[i][1]);
        b.B(110, T2 - 8, "pole4", "4 POLE");
        const char *bot[5][2] = {{"fdel", "DELAY"}, {"fatt", "ATTACK"}, {"fdec", "DECAY"}, {"fsus", "SUSTAIN"}, {"frel", "RELEASE"}};
        for (int i = 0; i < 5; i++)
            b.K(315.0f + 205.0f * (float)i, T2, bot[i][0], bot[i][1]);
        b.sec(14, 304, 900, 572, "AMPLIFIER");
        const char *at[4][2] = {{"pan_spread", "PAN SPREAD"}, {"aenv_amt", "ENV AMOUNT"}, {"avel", "VELOCITY"}, {"adel", "DELAY"}};
        const char *ab[4][2] = {{"aatt", "ATTACK"}, {"adec", "DECAY"}, {"asus", "SUSTAIN"}, {"arel", "RELEASE"}};
        for (int i = 0; i < 4; i++) {
            b.K(110.0f + 205.0f * (float)i, B1, at[i][0], at[i][1]);
            b.K(110.0f + 205.0f * (float)i, B2, ab[i][0], ab[i][1]);
        }
        b.sec(912, 304, 1266, 572, "LEVELS");
        b.K(1000, B1, "pgm_vol", "PROGRAM VOL");
        b.K(1170, B1, "vca_level", "VCA LEVEL");
        b.B(1000, B2 - 8, "pan_mode", "PAN FIXED");
        ps.push_back(b.pg);
    }
    {
        Builder b;
        b.pg.name = "MOD";
        b.sec(14, 16, 1266, 284, "MODULATION 1-4");
        b.sec(14, 304, 1266, 572, "MODULATION 5-8");
        for (int k = 0; k < 8; k++) {
            float x0 = 30.0f + 310.0f * (float)(k % 4), y = k < 4 ? T1 + 50.0f : B1 + 50.0f;
            String n = String(k + 1);
            static char ids[8][3][16];
            std::snprintf(ids[k][0], 16, "mod%d_src", k + 1);
            std::snprintf(ids[k][1], 16, "mod%d_dst", k + 1);
            std::snprintf(ids[k][2], 16, "mod%d_amt", k + 1);
            b.K(x0 + 50.0f, y, ids[k][0], "SOURCE");
            b.K(x0 + 145.0f, y, ids[k][1], "DEST");
            b.K(x0 + 240.0f, y, ids[k][2], "AMOUNT");
            static char lbl[8][16];
            std::snprintf(lbl[k], 16, "SLOT %d", k + 1);
            b.note(x0 + 145.0f, y - 74.0f, lbl[k]);
        }
        ps.push_back(b.pg);
    }
    {
        Builder b;
        b.pg.name = "LFO";
        const char *T[4] = {"LFO 1", "LFO 2", "LFO 3", "LFO 4"};
        for (int k = 0; k < 4; k++) {
            float x0 = k % 2 ? 652.0f : 14.0f, y0 = k < 2 ? 16.0f : 304.0f;
            b.sec(x0, y0, x0 + 614.0f, y0 + 268.0f, T[k]);
            static char ids[4][6][16];
            const char *suf[6] = {"freq", "shape", "amt", "dest", "clk", "key"};
            for (int i = 0; i < 6; i++)
                std::snprintf(ids[k][i], 16, "lfo%d_%s", k + 1, suf[i]);
            float y = y0 + 64.0f;
            b.B(x0 + 60.0f, y - 8.0f, ids[k][1], "SHAPE", true);
            b.L(x0 + 100.0f, y - 52.0f, ids[k][1], 110);
            b.K(x0 + 290.0f, y, ids[k][0], "FREQUENCY");
            b.K(x0 + 420.0f, y, ids[k][2], "AMOUNT");
            b.K(x0 + 540.0f, y, ids[k][3], "DESTINATION");
            b.B(x0 + 290.0f, y + 128.0f - 8.0f, ids[k][4], "CLK SYNC");
            b.B(x0 + 420.0f, y + 128.0f - 8.0f, ids[k][5], "KEY SYNC");
        }
        ps.push_back(b.pg);
    }
    {
        Builder b;
        b.pg.name = "ENV3/FX";
        b.sec(14, 16, 1266, 284, "AUXILIARY ENVELOPE");
        const char *t[8][2] = {{"e3_dest", "DESTINATION"}, {"e3_amt", "ENV AMOUNT"}, {"e3_vel", "VELOCITY"}, {"e3del", "DELAY"},
                               {"e3att", "ATTACK"}, {"e3dec", "DECAY"}, {"e3sus", "SUSTAIN"}, {"e3rel", "RELEASE"}};
        for (int i = 0; i < 4; i++) {
            b.K(110.0f + 205.0f * (float)i, T1, t[i][0], t[i][1]);
            b.K(110.0f + 205.0f * (float)i, T2, t[i + 4][0], t[i + 4][1]);
        }
        b.B(1000, T1 - 8, "e3_repeat", "REPEAT");
        b.sec(14, 304, 1266, 572, "EFFECT");
        b.B(80, B1 - 8, "fx_on", "ON/OFF");
        b.B(80, B2 - 8, "fx_clk", "CLK SYNC");
        b.K(250, B1, "fx_type", "SELECT");
        b.L(320, 318, "fx_type", 150, {}, 18.0f);
        b.K(620, B1, "fx_mix", "MIX");
        b.K(780, B1, "fx_p1", "PARAM 1");
        b.K(940, B1, "fx_p2", "PARAM 2");
        ps.push_back(b.pg);
    }
    {
        Builder b;
        b.pg.name = "PLAY";
        b.sec(14, 16, 560, 284, "ARP");
        b.B(80, T1 - 8, "arp_on", "ON/OFF");
        b.K(200, T1, "arp_mode", "MODE");
        b.L(250, T1 - 44, "arp_mode", 100);
        b.K(420, T1, "arp_range", "RANGE");
        b.K(200, T2, "arp_repeat", "REPEATS");
        b.B(420, T2 - 8, "arp_relatch", "RELATCH");
        b.B(80, T2 - 8, "hold", "HOLD");
        b.sec(572, 16, 760, 284, "CLOCK");
        b.K(666, T1, "divide", "DIVIDE");
        b.note(666, T2 - 20, "on the MPC's tempo");
        b.sec(772, 16, 1266, 284, "SEQUENCER");
        b.B(840, T1 - 8, "seq_on", "PLAY");
        b.L(900, T1 - 30, "seq_type", 90);
        b.K(1080, T1, "seq_mode", "GATED MODE");
        b.note(1019, T2 - 8, "the program's sequence: PLAY");
        b.sec(14, 304, 760, 572, "LAYERS");
        b.L(40, B1 - 30, "layer_mode", 140);
        b.K(330, B1, "split_point", "SPLIT POINT");
        b.K(330, B2, "pgm_vol", "PROGRAM VOL");
        b.K(560, B1, "pb_range", "BEND RANGE");
        b.note(160, B2 - 20, "layer B: press the function key again");
        b.sec(772, 304, 1266, 572, "VOICES");
        b.L(800, B1 - 30, "voices", 120);
        b.L(1000, B1 - 30, "quality", 110, {"ECO (2x)", "HIGH (4x)"});
        b.note(1019, B2 - 20, "HIGH: the filters at 4x (more CPU)");
        ps.push_back(b.pg);
    }
    return ps;
}

/* ----------------------------------------------------------------------------- drawing --- */
void drawPage(Graphics &g, const Page &pg, int layer)
{
    auto r = Rectangle<float>(0.0f, 0.0f, (float)TW, (float)TH);
    drawPanelFace(g, r, 1.0f);
    for (auto &s : pg.secs)
        drawSection(g, s.r, s.title);
    for (auto &c : pg.ctls) {
        if (c.kind == Ctl::knob)
            drawLegend(g, c.legend, Rectangle<float>(c.x - 70.0f, c.y + KS * 0.5f + 4.0f, 140.0f, 20.0f), 16.0f);
        else if (c.kind == Ctl::button)
            drawLegend(g, c.legend, Rectangle<float>(c.x - 70.0f, c.y + BH * 0.5f + 6.0f, 140.0f, 20.0f), 15.0f);
        else {
            g.setColour(col::legend);
            g.setFont(font(Face::title, 15.0f));
            for (int i = 0; i < c.names.size(); i++)
                g.drawText(c.names[i], Rectangle<float>(c.x + 20.0f, c.y + c.rh * (float)i, c.w, c.rh), Justification::centredLeft, false);
        }
    }
    g.setColour(col::legend);
    g.setFont(font(Face::label, 14.0f));
    for (auto &[p, t] : pg.notes)
        g.drawText(t, Rectangle<float>(p.x - 160.0f, p.y - 9.0f, 320.0f, 18.0f), Justification::centred, false);
    /* the bottom strip: the name, the page, the layer; the maker's tag */
    g.setFont(font(Face::logo, 20.0f));
    g.drawText("REEVES", Rectangle<float>(18.0f, 586.0f, 140.0f, 30.0f), Justification::centredLeft, false);
    g.setFont(font(Face::title, 16.0f));
    if (layer >= 0)   /* (the layer's name: a picture of its own over the one background both layers share) */
        g.drawText(pg.name + "   LAYER " + (layer ? "B" : "A"), Rectangle<float>(160.0f, 586.0f, 400.0f, 30.0f), Justification::centredLeft, false);
    drawTag(g, Rectangle<float>(1280.0f - 18.0f - 200.0f, 628.0f - 14.0f - 24.0f, 200.0f, 24.0f));
    drawWear(g, r, 1.0f, {});
}

Image snapBackground(const Page &pg, int layer)
{
    Image img(Image::RGB, TW, TH, true);
    Graphics g(img);
    drawPage(g, pg, layer);
    return img;
}
const Rectangle<int> LABEL{160, 586, 400, 30};

bool writePng(const Image &img, const File &f)
{
    f.deleteFile();
    FileOutputStream out(f);
    PNGImageFormat png;
    return out.openedOk() && png.writeImageToStream(img, out);
}
var rectVar(Rectangle<int> r) { return Array<var>{r.getX(), r.getY(), r.getWidth(), r.getHeight()}; }

/* the control alone over its patch of the background, at a value */
Image drawCtl(const Image &bg, const Ctl &c, Rectangle<int> r, int index, int steps, int row = -1)
{
    Image img(Image::RGB, r.getWidth(), r.getHeight(), true);
    Graphics g(img);
    g.drawImageAt(bg.getClippedImage(r), 0, 0);
    g.addTransform(AffineTransform::translation((float)-r.getX(), (float)-r.getY()));
    if (c.kind == Ctl::knob) {
        float pos = steps > 1 ? (float)index / (float)(steps - 1) : 0.0f;
        drawKnob(g, Rectangle<float>(KS, KS).withCentre({c.x, c.y}), pos, bipolarOf(c.id));
    } else if (c.kind == Ctl::button) {
        bool lit = !c.cycle && index > 0;
        drawButton(g, Rectangle<float>(BW, BH).withCentre({c.x, c.y}), lit, false, false);
    } else {
        drawLed(g, {c.x + 8.0f, c.y + c.rh * ((float)row + 0.5f)}, 9.0f, index == row ? 1.0f : 0.0f);
    }
    return img;
}
} // namespace

int main(int argc, char **argv)
{
    ScopedJuceInitialiser_GUI gui;
    SharedResourcePointer<UiResources> res;
    if (argc < 2) {
        std::fprintf(stderr, "usage: reeves_skin_render <out folder>\n");
        return 2;
    }
    File dir = File::getCurrentWorkingDirectory().getChildFile(argv[1]);
    dir.deleteRecursively();
    dir.createDirectory();
    Array<var> pageList;
    for (auto &pg : pages()) {
        String tag = pg.name.toLowerCase().retainCharacters("abcdefghijklmnopqrstuvwxyz0123456789");
        Image bgA = snapBackground(pg, -1);
        writePng(bgA, dir.getChildFile(tag + "_panel.png"));
        writePng(snapBackground(pg, 0).getClippedImage(LABEL), dir.getChildFile(tag + "_label_a.png"));
        writePng(snapBackground(pg, 1).getClippedImage(LABEL), dir.getChildFile(tag + "_label_b.png"));
        Array<var> list;
        int n = 0;
        for (auto &c : pg.ctls) {
            auto rg = rangeOf(c.id);
            int steps = rg.hi - rg.lo + 1;
            auto *o = new DynamicObject();
            o->setProperty("id", c.id);
            o->setProperty("steps", steps);
            Array<var> names;
            for (auto &t : rg.names)
                names.add(t);
            o->setProperty("names", names);
            String base = tag + "_" + String(n++).paddedLeft('0', 3) + "_" + c.id.retainCharacters("abcdefghijklmnopqrstuvwxyz0123456789_");
            if (c.kind == Ctl::knob) {
                auto r = Rectangle<float>(KS + 6.0f, KS + 6.0f).withCentre({c.x, c.y}).toNearestInt();
                int frames = jmax(2, jmin(steps, 128, 9800 / r.getHeight()));
                Image strip(Image::RGB, r.getWidth(), r.getHeight() * frames, true);
                {
                    Graphics sg(strip);
                    for (int f = 0; f < frames; f++) {
                        int idx = frames == steps ? f : (int)std::lround((double)f * (steps - 1) / (frames - 1));
                        sg.drawImageAt(drawCtl(bgA, c, r, idx, steps), 0, f * r.getHeight());
                    }
                }
                writePng(strip, dir.getChildFile(base + "_knob.png"));
                auto fr = r.getUnion(Rectangle<float>(c.x - 64.0f, c.y + KS * 0.5f, 128.0f, 26.0f).toNearestInt());
                o->setProperty("kind", "knob");
                o->setProperty("bipolar", bipolarOf(c.id));
                o->setProperty("rect", rectVar(r));
                o->setProperty("frame", rectVar(fr));
                o->setProperty("frames", frames);
                o->setProperty("strip", base + "_knob.png");
            } else if (c.kind == Ctl::button) {
                auto r = Rectangle<float>(BW + 6.0f, BH + 6.0f).withCentre({c.x, c.y}).toNearestInt();
                o->setProperty("kind", "button");
                o->setProperty("rect", rectVar(r));
                Array<var> rs;
                for (int i = 0; i < steps; i++) {
                    auto *ro = new DynamicObject();
                    ro->setProperty("a", i);
                    ro->setProperty("b", i);
                    ro->setProperty("next", c.cycle ? (i + 1) % steps : (i ? 0 : 1));
                    String f = base + "_button_" + String(i) + ".png";
                    writePng(drawCtl(bgA, c, r, i, steps), dir.getChildFile(f));
                    ro->setProperty("image", f);
                    rs.add(var(ro));
                }
                o->setProperty("ranges", rs);
            } else {
                o->setProperty("kind", "leds");
                Array<var> rows;
                for (int k = 0; k < c.names.size(); k++) {
                    auto r = Rectangle<float>(c.x, c.y + c.rh * (float)k, 20.0f + c.w, c.rh).toNearestInt();
                    auto *ro = new DynamicObject();
                    ro->setProperty("rect", rectVar(r));
                    String on = base + "_led" + String(k) + "_on.png", off = base + "_led" + String(k) + "_off.png";
                    writePng(drawCtl(bgA, c, r, k, steps, k), dir.getChildFile(on));
                    writePng(drawCtl(bgA, c, r, -1, steps, k), dir.getChildFile(off));
                    ro->setProperty("on", on);
                    ro->setProperty("off", off);
                    rows.add(var(ro));
                }
                o->setProperty("rows", rows);
            }
            list.add(var(o));
        }
        auto *po = new DynamicObject();
        po->setProperty("name", pg.name);
        po->setProperty("background", tag + "_panel.png");
        po->setProperty("labelA", tag + "_label_a.png");
        po->setProperty("labelB", tag + "_label_b.png");
        po->setProperty("labelRect", rectVar(LABEL));
        po->setProperty("controls", list);
        pageList.add(var(po));
        std::printf("%-8s %d controls\n", pg.name.toRawUTF8(), (int)pg.ctls.size());
    }
    auto *m = new DynamicObject();
    m->setProperty("pages", pageList);
    dir.getChildFile("manifest.json").replaceWithText(JSON::toString(var(m)));
    std::printf("manifest.json in %s\n", dir.getFullPathName().toRawUTF8());
    return 0;
}
