/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (REEVES) */
/* The MPC skin's pictures, drawn with the VST3's own art (src/ui/Look.cpp: the powder-coated panel, the knobs, the
 * buttons, the LEDs, the section frames, the legends, the maker's tag), on the MPC's six pages (its function keys;
 * a second press: the same page for layer B):
 *
 *   OSC      OSCILLATORS
 *   FILTER   LOW-PASS FILTER
 *   ENV      AMPLIFIER, AUXILIARY ENVELOPE
 *   LFO      LFO 1-4
 *   MOD      the eight MODULATION slots on a display; the knobs edit the slot tapped
 *   PLAY     EFFECT, ARP, GLIDE / UNISON / HOLD / PROGRAM, MISC PARAMS
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
    float ks = KS;                       /* a knob's size */
    float vw = 120.0f;                   /* its value's width */
    int first = 0;                       /* LEDs: the value of the first row (OSC SHAPE: no LED for OFF) */
    int slot = -1;                       /* MOD: shown only while this slot is selected (-1: always) */
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
    /* MOD: the display, a table of the eight slots (each cell its parameter's value), a row tapped selects it */
    Rectangle<int> oled;
    struct Cell {
        String id;
        Rectangle<int> r;
        bool left = true;
    };
    std::vector<Cell> cells;
    std::vector<Rectangle<int>> selRows;   /* the slot numbers (tapped: that slot) */
    Rectangle<int> slotShow;               /* "SLOT n" over the knobs */
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
    void K(float x, float y, const char *id, const char *leg) { pg.ctls.push_back({Ctl::knob, id, x, y, leg, {}, 0.0f, false, LR, KS}); }
    void B(float x, float y, const char *id, const char *leg, bool cycle = false) { pg.ctls.push_back({Ctl::button, id, x, y, leg, {}, 0.0f, cycle}); }
    void L(float x, float y, const char *id, float w, StringArray names = {}, float rh = LR)
    {
        auto r = rangeOf(id);
        pg.ctls.push_back({Ctl::leds, id, x, y, "", names.isEmpty() ? r.names : names, w, false, rh});
    }
    void note(float x, float y, const char *t) { pg.notes.push_back({{x, y}, t}); }
};

/* The pages: the Rev2's own sections, each laid out as on the Rev2's panel (two rows where the Rev2 has two), filling
 * the screen; every knob with its legend and, under it, its value (ARIA's way). Six pages, each for layer A (a press)
 * and layer B (a second press):
 *   OSC     OSCILLATORS (both rows: OSC 1 above, OSC 2 below, SYNC between)
 *   FILTER  LOW-PASS FILTER (its knobs above, 4 POLE and its envelope below)
 *   ENV     AMPLIFIER | AUXILIARY ENVELOPE
 *   LFO     the four LFOs, each as the Rev2's LFO section
 *   MOD     the eight MODULATION slots
 *   PLAY    EFFECT | ARP + CLOCK | GLIDE, UNISON, HOLD; PROGRAM | MISC PARAMS */
constexpr float TOP0 = 18, TOP1 = 290, BOT0 = 306, BOT1 = 576;
struct It {
    char kind;                           /* 'k' knob, 'b' button, 'l' an LED list (its id's choices), ' ' a gap */
    const char *id, *legend;
    int first = 0;                       /* 'l': the value of its first LED */
    int count = 0;                       /* 'l': how many (0: all) */
};
/* a section of rows, each row's items spread evenly across it; ks: the knobs' size */
void gridSec(Builder &b, float x0, float x1, float y0, float y1, const char *title, std::vector<std::vector<It>> rows, float ks)
{
    b.sec(x0, y0, x1, y1, title);
    float h = (y1 - y0 - 8.0f) / (float)rows.size();
    for (size_t r = 0; r < rows.size(); r++) {
        float w = (x1 - x0 - 12.0f) / (float)rows[r].size();
        float cy = y0 + 8.0f + h * (float)r + (h - ks - 46.0f) * 0.5f + ks * 0.5f;
        for (size_t i = 0; i < rows[r].size(); i++) {
            const It &it = rows[r][i];
            float cx = x0 + 6.0f + w * ((float)i + 0.5f);
            if (it.kind == 'k')
                b.pg.ctls.push_back({Ctl::knob, it.id, cx, cy, it.legend, {}, 0.0f, false, LR, ks, std::min(w - 8.0f, 170.0f)});
            else if (it.kind == 'b')
                b.B(cx, cy, it.id, it.legend, std::string(it.legend) == "SHAPE");
            else if (it.kind == 'l') {
                auto rg = rangeOf(it.id);
                StringArray names;
                int n = it.count ? it.count : rg.names.size() - it.first;
                for (int k = 0; k < n; k++)
                    names.add(rg.names[it.first + k]);
                float rh = std::min(24.0f, (h - 34.0f) / (float)n), mid = y0 + 8.0f + h * ((float)r + 0.5f) - 4.0f;
                Ctl c{Ctl::leds, it.id, cx - w * 0.5f + 4.0f, mid - rh * (float)n * 0.5f, "", names, w - 30.0f, false, rh};
                c.first = it.first;
                b.pg.ctls.push_back(c);
            }
        }
    }
}

std::vector<Page> pages()
{
    std::vector<Page> ps;
    const float L = 14, R = 1266;
    {
        Builder b;
        b.pg.name = "OSC";
        gridSec(b, L, R, TOP0, BOT1, "OSCILLATORS",
                {{{'k', "o1_freq", "OSC 1 FREQ"}, {'k', "o1_fine", "FINE TUNE"}, {'b', "o1_shape", "SHAPE"},
                  {'l', "o1_shape", "", 1}, {'k', "o1_shmod", "SHAPE MOD"}, {'k', "sub", "SUB OCTAVE"}, {'k', "noise", "NOISE"}},
                 {{'k', "o2_freq", "OSC 2 FREQ"}, {'k', "o2_fine", "FINE TUNE"}, {'b', "o2_shape", "SHAPE"},
                  {'l', "o2_shape", "", 1}, {'k', "o2_shmod", "SHAPE MOD"}, {'k', "osc_mix", "OSC MIX"}, {'k', "slop", "OSC SLOP"}}},
                104);
        /* SYNC: between the rows, between FREQ and FINE TUNE (as on the Rev2) */
        float w = (R - L - 12.0f) / 7.0f;
        b.B(L + 6.0f + w, (TOP0 + BOT1) * 0.5f + 4.0f, "sync", "SYNC");
        ps.push_back(b.pg);
    }
    {
        Builder b;
        b.pg.name = "FILTER";
        gridSec(b, L, R, TOP0, BOT1, "LOW-PASS FILTER",
                {{{'k', "cutoff", "CUTOFF"}, {'k', "reso", "RESONANCE"}, {'k', "fenv_amt", "ENV AMOUNT"}, {'k', "fvel", "VELOCITY"},
                  {'k', "keyamt", "KEY AMOUNT"}, {'k', "audiomod", "AUDIO MOD"}},
                 {{'b', "pole4", "4 POLE"}, {'k', "fdel", "DELAY"}, {'k', "fatt", "ATTACK"}, {'k', "fdec", "DECAY"},
                  {'k', "fsus", "SUSTAIN"}, {'k', "frel", "RELEASE"}}},
                110);
        ps.push_back(b.pg);
    }
    {
        Builder b;
        b.pg.name = "ENV";
        gridSec(b, L, 634, TOP0, BOT1, "AMPLIFIER",
                {{{'k', "pan_spread", "PAN SPREAD"}, {'k', "aenv_amt", "ENV AMOUNT"}, {'k', "avel", "VELOCITY"}, {'k', "adel", "DELAY"}},
                 {{'k', "aatt", "ATTACK"}, {'k', "adec", "DECAY"}, {'k', "asus", "SUSTAIN"}, {'k', "arel", "RELEASE"}}},
                100);
        gridSec(b, 646, R, TOP0, BOT1, "AUXILIARY ENVELOPE",
                {{{'k', "e3_dest", "DESTINATION"}, {'k', "e3_amt", "ENV AMOUNT"}, {'k', "e3_vel", "VELOCITY"}, {'k', "e3del", "DELAY"}},
                 {{'k', "e3att", "ATTACK"}, {'k', "e3dec", "DECAY"}, {'k', "e3sus", "SUSTAIN"}, {'k', "e3rel", "RELEASE"}}},
                100);
        ps.push_back(b.pg);
    }
    {
        Builder b;
        b.pg.name = "LFO";
        static char ids[4][6][16];
        static const char *T[4] = {"LFO 1", "LFO 2", "LFO 3", "LFO 4"};
        const char *suf[6] = {"freq", "shape", "amt", "dest", "clk", "key"};
        for (int k = 0; k < 4; k++) {
            for (int i = 0; i < 6; i++)
                std::snprintf(ids[k][i], 16, "lfo%d_%s", k + 1, suf[i]);
            float x0 = k % 2 ? 646.0f : L, x1 = k % 2 ? R : 634.0f;
            /* as the Rev2's LFO section: SHAPE and its LEDs, FREQUENCY, CLK SYNC; DESTINATION, AMOUNT, KEY SYNC */
            gridSec(b, x0, x1, k < 2 ? TOP0 : BOT0, k < 2 ? TOP1 : BOT1, T[k],
                    {{{'b', ids[k][1], "SHAPE"}, {'l', ids[k][1], ""}, {'k', ids[k][0], "FREQUENCY"}, {'b', ids[k][4], "CLK SYNC"}},
                     {{' ', "", ""}, {'k', ids[k][3], "DESTINATION"}, {'k', ids[k][2], "AMOUNT"}, {'b', ids[k][5], "KEY SYNC"}}},
                    68);
        }
        ps.push_back(b.pg);
    }
    {
        Builder b;
        b.pg.name = "MOD";
        /* the display: all eight slots at once, as the Rev2's screen shows the one being edited */
        b.pg.oled = Rectangle<int>(24, 28, 822, 538);
        static char ids[8][3][16];
        for (int k = 0; k < 8; k++) {
            std::snprintf(ids[k][0], 16, "mod%d_src", k + 1);
            std::snprintf(ids[k][1], 16, "mod%d_dst", k + 1);
            std::snprintf(ids[k][2], 16, "mod%d_amt", k + 1);
            int y = 28 + 52 + k * 60;
            b.pg.selRows.push_back(Rectangle<int>(30, y, 82, 56));
            b.pg.cells.push_back({ids[k][0], Rectangle<int>(24 + 120, y + 8, 260, 40), true});
            b.pg.cells.push_back({ids[k][2], Rectangle<int>(24 + 390, y + 8, 120, 40), true});
            b.pg.cells.push_back({ids[k][1], Rectangle<int>(24 + 530, y + 8, 280, 40), true});
        }
        /* as the Rev2's MODULATION section: SOURCE and DESTINATION on the left, the slot and AMOUNT on the right; the
         * knobs edit the slot selected (a slot number tapped on the display) */
        b.sec(864, TOP0, R, BOT1, "MODULATION");
        b.pg.slotShow = Rectangle<int>(1065 + 12, 92, 150, 56);
        for (int k = 0; k < 8; k++) {
            const float xs[2] = {970.0f, 1152.0f};
            Ctl src{Ctl::knob, ids[k][0], xs[0], 150.0f, "SOURCE", {}, 0.0f, false, LR, 104, 176};
            Ctl dst{Ctl::knob, ids[k][1], xs[0], 420.0f, "DESTINATION", {}, 0.0f, false, LR, 104, 176};
            Ctl amt{Ctl::knob, ids[k][2], xs[1], 420.0f, "AMOUNT", {}, 0.0f, false, LR, 104, 176};
            src.slot = dst.slot = amt.slot = k;
            b.pg.ctls.push_back(src);
            b.pg.ctls.push_back(dst);
            b.pg.ctls.push_back(amt);
        }
        ps.push_back(b.pg);
    }
    {
        Builder b;
        b.pg.name = "PLAY";
        gridSec(b, L, 430, TOP0, TOP1, "EFFECT",
                {{{'b', "fx_on", "ON/OFF"}, {'k', "fx_type", "SELECT"}, {'k', "fx_mix", "MIX"}},
                 {{'b', "fx_clk", "CLK SYNC"}, {'k', "fx_p1", "PARAM 1"}, {'k', "fx_p2", "PARAM 2"}}},
                58);
        gridSec(b, 442, 858, TOP0, TOP1, "ARP",
                {{{'b', "arp_on", "ON/OFF"}, {'k', "arp_mode", "MODE"}, {'k', "divide", "DIVIDE"}},
                 {{'b', "arp_relatch", "RELATCH"}, {'k', "arp_range", "RANGE"}, {'k', "arp_repeat", "REPEATS"}}},
                58);
        gridSec(b, 870, R, TOP0, TOP1, "GLIDE / UNISON",
                {{{'k', "o1_glide", "RATE OSC 1"}, {'k', "o2_glide", "RATE OSC 2"}, {'b', "glide", "GLIDE"}},
                 {{'b', "unison", "UNISON"}, {'b', "hold", "HOLD"}, {'k', "pgm_vol", "VOLUME"}}},
                58);
        /* (the bottom band: PROGRAM and MISC PARAMS across the whole width, room for a finger) */
        gridSec(b, L, 614, BOT0, BOT1, "PROGRAM",
                {{{'k', "layer_mode", "STACK / SPLIT"}, {'k', "split_point", "SPLIT POINT"}, {'k', "voices", "VOICES"},
                  {'k', "quality", "QUALITY"}}},
                84);
        gridSec(b, 626, R, BOT0, BOT1, "MISC PARAMS",
                {{{'k', "glide_mode", "GLIDE MODE"}, {'k', "uni_mode", "UNI VOICES"}, {'k', "uni_detune", "DETUNE"},
                  {'k', "key_mode", "KEY MODE"}, {'k', "pb_range", "BEND"}}},
                84);
        ps.push_back(b.pg);
    }
    return ps;
}

/* ----------------------------------------------------------------------------- drawing --- */
/* MOD's display: a dark glass with a thin bezel, the table's heads and lines */
void drawOled(Graphics &g, const Page &pg)
{
    auto r = pg.oled.toFloat();
    g.setColour(Colour(0xff0b0c0e));
    g.fillRoundedRectangle(r.expanded(6.0f), 9.0f);
    g.setColour(Colour(0xff2a2c30));
    g.drawRoundedRectangle(r.expanded(6.0f), 9.0f, 1.5f);
    g.setColour(col::oledBg);
    g.fillRoundedRectangle(r, 5.0f);
    g.setColour(col::oledDim);
    g.setFont(font(Face::mono, 17.0f));
    const char *head[4] = {"SLOT", "SOURCE", "AMOUNT", "DESTINATION"};
    float hx[4] = {r.getX() + 16.0f, r.getX() + 120.0f, r.getX() + 390.0f, r.getX() + 530.0f};
    for (int i = 0; i < 4; i++)
        g.drawText(head[i], Rectangle<float>(hx[i], r.getY() + 12.0f, 260.0f, 24.0f), Justification::centredLeft, false);
    g.drawHorizontalLine((int)r.getY() + 44, r.getX() + 12.0f, r.getRight() - 12.0f);
    for (int k = 1; k < 8; k++) {
        float y = (float)pg.selRows[(size_t)k].getY() - 1.0f;
        g.setColour(col::oledDim.withAlpha(0.35f));
        g.drawHorizontalLine((int)y, r.getX() + 12.0f, r.getRight() - 12.0f);
    }
}

void drawPage(Graphics &g, const Page &pg, int layer)
{
    auto r = Rectangle<float>(0.0f, 0.0f, (float)TW, (float)TH);
    drawPanelFace(g, r, 1.0f);
    for (auto &s : pg.secs)
        drawSection(g, s.r, s.title);
    if (!pg.oled.isEmpty())
        drawOled(g, pg);
    for (auto &c : pg.ctls) {
        if (c.kind == Ctl::knob)
            drawLegend(g, c.legend, Rectangle<float>(c.x - 70.0f, c.y + c.ks * 0.5f + 4.0f, 140.0f, 20.0f), 15.5f);
        else if (c.kind == Ctl::button)
            drawLegend(g, c.legend, Rectangle<float>(c.x - 70.0f, c.y + BH * 0.5f + 6.0f, 140.0f, 20.0f), 15.0f);
        else {
            g.setColour(col::legend);
            g.setFont(font(Face::title, 16.0f));
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

/* a knob's value, as the VST shows it */
String valueText(const String &id, int v)
{
    int p = layerIndex(id);
    if (p >= 0) {
        char t[32];
        paramText(p, v, t, sizeof t);
        return t;
    }
    auto r = rangeOf(id);
    if (!r.names.isEmpty())
        return r.names[v - r.lo];
    if (id == "split_point") {
        static const char *N[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
        return String(N[v % 12]) + String(v / 12 - 2);   /* (note 60: C3, as the VST shows it) */
    }
    return String(v);
}
Rectangle<int> valueRect(const Ctl &c) { return Rectangle<float>(c.x - c.vw * 0.5f, c.y + c.ks * 0.5f + 24.0f, c.vw, 18.0f).toNearestInt(); }

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
        drawKnob(g, Rectangle<float>(c.ks, c.ks).withCentre({c.x, c.y}), pos, bipolarOf(c.id));
    } else if (c.kind == Ctl::button) {
        bool lit = !c.cycle && index > 0;
        drawButton(g, Rectangle<float>(BW, BH).withCentre({c.x, c.y}), lit, false, false);
    } else {
        drawLed(g, {c.x + 8.0f, c.y + c.rh * ((float)row + 0.5f)}, 10.0f, index == row ? 1.0f : 0.0f);
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
            o->setProperty("slot", c.slot);
            String base = tag + "_" + String(n++).paddedLeft('0', 3) + "_" + c.id.retainCharacters("abcdefghijklmnopqrstuvwxyz0123456789_");
            if (c.kind == Ctl::knob) {
                auto r = Rectangle<float>(c.ks + 6.0f, c.ks + 6.0f).withCentre({c.x, c.y}).toNearestInt();
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
                auto vr = valueRect(c);
                {           /* the value under the legend: a strip of its own, a frame a value */
                    Image vs(Image::RGB, vr.getWidth(), vr.getHeight() * steps, true);
                    Graphics vg(vs);
                    for (int f = 0; f < steps; f++) {
                        vg.drawImageAt(bgA.getClippedImage(vr), 0, f * vr.getHeight());
                        vg.setColour(col::dim);
                        vg.setFont(font(Face::mono, 14.5f));
                        vg.drawText(valueText(c.id, rg.lo + f), Rectangle<int>(0, f * vr.getHeight(), vr.getWidth(), vr.getHeight()),
                                    Justification::centred, false);
                    }
                    writePng(vs, dir.getChildFile(base + "_value.png"));
                }
                o->setProperty("value", base + "_value.png");
                o->setProperty("valueRect", rectVar(vr));
                auto fr = r.getUnion(Rectangle<float>(c.x - c.vw * 0.5f, c.y + c.ks * 0.5f, c.vw, 44.0f).toNearestInt());
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
                    ro->setProperty("value", c.first + k);
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
        if (!pg.oled.isEmpty()) {
            /* the display's cells: a strip each, a frame a value; the slot numbers (two looks); "SLOT n" */
            Array<var> cells;
            int ci = 0;
            for (auto &cell : pg.cells) {
                auto rg = rangeOf(cell.id);
                int steps = rg.hi - rg.lo + 1;
                Image st(Image::RGB, cell.r.getWidth(), cell.r.getHeight() * steps, true);
                Graphics sg(st);
                for (int f = 0; f < steps; f++) {
                    sg.drawImageAt(bgA.getClippedImage(cell.r), 0, f * cell.r.getHeight());
                    String t = valueText(cell.id, rg.lo + f);
                    sg.setColour(t == "Off" ? col::oledDim : col::oled);
                    sg.setFont(font(Face::mono, 23.0f));
                    sg.drawText(t, Rectangle<int>(0, f * cell.r.getHeight(), cell.r.getWidth(), cell.r.getHeight()),
                                Justification::centredLeft, false);
                }
                String f = tag + "_cell" + String(ci++).paddedLeft('0', 2) + ".png";
                writePng(st, dir.getChildFile(f));
                auto *co = new DynamicObject();
                co->setProperty("id", cell.id);
                co->setProperty("rect", rectVar(cell.r));
                co->setProperty("strip", f);
                co->setProperty("steps", steps);
                cells.add(var(co));
            }
            po->setProperty("cells", cells);
            Array<var> sel;
            for (int k = 0; k < (int)pg.selRows.size(); k++) {
                auto r = pg.selRows[(size_t)k];
                auto *so = new DynamicObject();
                so->setProperty("rect", rectVar(r));
                for (int on = 0; on < 2; on++) {
                    Image im(Image::RGB, r.getWidth(), r.getHeight(), true);
                    Graphics ig(im);
                    ig.drawImageAt(bgA.getClippedImage(r), 0, 0);
                    auto box = Rectangle<float>(4.0f, 8.0f, (float)r.getWidth() - 8.0f, (float)r.getHeight() - 16.0f);
                    if (on) {
                        ig.setColour(col::oled);
                        ig.fillRoundedRectangle(box, 4.0f);
                    } else {
                        ig.setColour(col::oledDim);
                        ig.drawRoundedRectangle(box, 4.0f, 1.2f);
                    }
                    ig.setColour(on ? col::oledBg : col::oled);
                    ig.setFont(font(Face::mono, 25.0f));
                    ig.drawText(String(k + 1), box, Justification::centred, false);
                    String f = tag + "_sel" + String(k) + (on ? "_on.png" : "_off.png");
                    writePng(im, dir.getChildFile(f));
                    so->setProperty(on ? "on" : "off", f);
                }
                sel.add(var(so));
            }
            po->setProperty("select", sel);
            auto r = pg.slotShow;
            Image st(Image::RGB, r.getWidth(), r.getHeight() * 8, true);
            Graphics sg(st);
            for (int k = 0; k < 8; k++) {
                sg.drawImageAt(bgA.getClippedImage(r), 0, k * r.getHeight());
                auto box = Rectangle<float>(2.0f, (float)(k * r.getHeight()) + 2.0f, (float)r.getWidth() - 4.0f, (float)r.getHeight() - 4.0f);
                sg.setColour(col::oledBg);
                sg.fillRoundedRectangle(box, 5.0f);
                sg.setColour(col::oled);
                sg.setFont(font(Face::mono, 26.0f));
                sg.drawText("SLOT " + String(k + 1), box, Justification::centred, false);
            }
            writePng(st, dir.getChildFile(tag + "_slot.png"));
            po->setProperty("slotShow", rectVar(r));
            po->setProperty("slotStrip", tag + "_slot.png");
        }
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
