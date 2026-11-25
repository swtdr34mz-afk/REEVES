/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (REEVES) */
#include "PluginEditor.h"

using namespace rvui;
using namespace rv;
using Panel = ReevesPanel;

namespace
{
constexpr float KD = 76.0f;                      /* a knob's size */
constexpr float BW = 38.0f, BH = 21.0f;
constexpr float FS = 1.32f;                     /* the legends' size, over the base sizes below */
constexpr float LS = 15.0f;                     /* an LED list's step */          /* a button's */

/* ------------------------------------------------------------------------------- a knob --- */
/* a parameter's knob (target: which, now: the LFO or the layer chosen), or an endless encoder (onTurn) */
class Knob : public juce::Component, public Panel::Live
{
public:
    using Target = std::function<juce::RangedAudioParameter *()>;
    Knob(Panel &p, Target t, std::function<void(juce::RangedAudioParameter *)> touched)
        : panel(p), target(std::move(t)), onTouch(std::move(touched))
    {
    }
    Knob(Panel &p, std::function<void(int)> turn) : panel(p), onTurn(std::move(turn)) { endless = true; }
    std::function<void(int)> afterSet;           /* (RATE/TIME: both oscillators' glide) */

    void paint(juce::Graphics &g) override
    {
        /* the body (scale, ribs, chrome, cap) drawn once at the screen's density and kept; only the line drawn each
         * time: a knob turned costs a picture and a line (the Mac repaints a lot at once) */
        float sc = g.getInternalContext().getPhysicalPixelScaleFactor();
        auto r = getLocalBounds().toFloat();
        if (body.isNull() || sc != bodyScale || bipolar != bodyBipolar) {
            bodyScale = sc;
            bodyBipolar = bipolar;
            body = juce::Image(juce::Image::ARGB, juce::jmax(1, (int)std::ceil(r.getWidth() * sc)), juce::jmax(1, (int)std::ceil(r.getHeight() * sc)), true);
            juce::Graphics bg(body);
            bg.addTransform(juce::AffineTransform::scale(sc));
            drawKnob(bg, r, 0.0f, bipolar, endless, false);
        }
        g.drawImage(body, r, juce::RectanglePlacement::stretchToFit);
        float pos = endless ? turned * 0.04f : shownPos;
        drawKnobPointer(g, r, endless ? pos - std::floor(pos) : pos, endless);
    }
    void poll() override
    {
        if (endless)
            return;
        auto *p = target();
        float pos = p ? p->getValue() : 0.0f;
        bool bip = false;
        if (auto *ip = dynamic_cast<juce::AudioParameterInt *>(p))
            bip = ip->getRange().getStart() < 0;
        if (pos != shownPos || bip != bipolar) {
            shownPos = pos;
            bipolar = bip;
            repaint();
        }
    }
    void mouseDown(const juce::MouseEvent &e) override
    {
        startY = e.position.y;
        acc = 0.0f;
        if (endless)
            return;
        if (auto *p = target()) {
            if (auto *ip = dynamic_cast<juce::AudioParameterInt *>(p))
                startV = ip->get();
            else
                startV = (int)std::lround(p->getValue() * 1000.0f);
            p->beginChangeGesture();
            if (onTouch)
                onTouch(p);
        }
    }
    void mouseDrag(const juce::MouseEvent &e) override
    {
        float dy = startY - e.position.y;
        bool fine = e.mods.isShiftDown();
        if (endless) {
            float steps = (dy - acc) / (fine ? 14.0f : 7.0f);
            int n = (int)steps;
            if (n != 0) {
                acc += (float)n * (fine ? 14.0f : 7.0f);
                turned += (float)n;
                onTurn(n);
                repaint();
            }
            return;
        }
        auto *p = target();
        if (!p)
            return;
        if (auto *ip = dynamic_cast<juce::AudioParameterInt *>(p)) {
            int lo = ip->getRange().getStart(), hi = ip->getRange().getEnd(), range = hi - lo;
            float px = juce::jlimit(1.5f, 18.0f, 240.0f / (float)juce::jmax(1, range)) * (fine ? 4.0f : 1.0f);
            set(p, juce::jlimit(lo, hi, startV + (int)std::lround(dy / px)));
        } else {
            float v = juce::jlimit(0.0f, 1.0f, (float)startV / 1000.0f + dy / (fine ? 900.0f : 220.0f));
            p->setValueNotifyingHost(v);
        }
    }
    void mouseUp(const juce::MouseEvent &) override
    {
        if (!endless)
            if (auto *p = target())
                p->endChangeGesture();
    }
    void mouseWheelMove(const juce::MouseEvent &, const juce::MouseWheelDetails &w) override
    {
        int d = w.deltaY > 0 ? 1 : w.deltaY < 0 ? -1 : 0;
        if (d == 0)
            return;
        if (endless) {
            turned += (float)d;
            onTurn(d);
            repaint();
            return;
        }
        if (auto *p = target()) {
            if (onTouch)
                onTouch(p);
            if (auto *ip = dynamic_cast<juce::AudioParameterInt *>(p)) {
                p->beginChangeGesture();
                set(p, juce::jlimit(ip->getRange().getStart(), ip->getRange().getEnd(), ip->get() + d));
                p->endChangeGesture();
            } else {
                p->beginChangeGesture();
                p->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, p->getValue() + 0.01f * (float)d));
                p->endChangeGesture();
            }
        }
    }
    void mouseDoubleClick(const juce::MouseEvent &) override
    {
        if (endless)
            return;
        if (auto *p = target()) {
            p->beginChangeGesture();
            p->setValueNotifyingHost(p->getDefaultValue());
            p->endChangeGesture();
            if (afterSet)
                if (auto *ip = dynamic_cast<juce::AudioParameterInt *>(p))
                    afterSet(ip->get());
        }
    }

private:
    void set(juce::RangedAudioParameter *p, int v)
    {
        p->setValueNotifyingHost(p->convertTo0to1((float)v));
        if (afterSet)
            afterSet(v);
    }
    Panel &panel;
    Target target;
    std::function<void(juce::RangedAudioParameter *)> onTouch;
    std::function<void(int)> onTurn;
    bool endless = false, bipolar = false, bodyBipolar = false;
    float shownPos = -1.0f, startY = 0.0f, acc = 0.0f, turned = 0.0f, bodyScale = 0.0f;
    juce::Image body;
    int startV = 0;
};

/* ------------------------------------------------------------------------------ a button --- */
class Btn : public juce::Component, public Panel::Live
{
public:
    Btn(std::function<void()> press, std::function<bool()> on) : onPress(std::move(press)), isOn(std::move(on))
    {
        setRepaintsOnMouseActivity(true);
    }
    void paint(juce::Graphics &g) override
    {
        /* (each look drawn once at the screen's density and kept) */
        float sc = g.getInternalContext().getPhysicalPixelScaleFactor();
        int k = (lit ? 1 : 0) | (down ? 2 : 0) | (isMouseOver() ? 4 : 0);
        auto r = getLocalBounds().toFloat();
        if (sc != cacheScale) {
            for (auto &c : cache)
                c = juce::Image();
            cacheScale = sc;
        }
        if (cache[k].isNull()) {
            auto big = r.expanded(r.getWidth() * 0.6f, r.getHeight() * 1.2f);
            cache[k] = juce::Image(juce::Image::ARGB, juce::jmax(1, (int)std::ceil(big.getWidth() * sc)), juce::jmax(1, (int)std::ceil(big.getHeight() * sc)), true);
            juce::Graphics cg(cache[k]);
            cg.addTransform(juce::AffineTransform::scale(sc).translated(-big.getX() * sc, -big.getY() * sc));
            drawButton(cg, r.reduced(1.0f), lit, down, isMouseOver());
        }
        auto big = r.expanded(r.getWidth() * 0.6f, r.getHeight() * 1.2f);
        g.drawImage(cache[k], big, juce::RectanglePlacement::stretchToFit);
    }
    void poll() override
    {
        bool l = isOn ? isOn() : false;
        if (l != lit) {
            lit = l;
            repaint();
        }
    }
    void mouseDown(const juce::MouseEvent &) override
    {
        down = true;
        repaint();
        if (onPress)
            onPress();
        poll();
    }
    void mouseUp(const juce::MouseEvent &) override
    {
        down = false;
        repaint();
    }

private:
    std::function<void()> onPress;
    std::function<bool()> isOn;
    bool lit = false, down = false;
    juce::Image cache[8];
    float cacheScale = 0.0f;
};

/* ----------------------------------------------------------------------------- LEDs --- */
class Leds : public juce::Component, public Panel::Live
{
public:
    Leds(int n, std::function<float(int)> level, std::function<void(int)> pick = {}, bool horizontal = false)
        : n(n), level(std::move(level)), pick(std::move(pick)), horizontal(horizontal), shown((size_t)n, -1.0f)
    {
    }
    void paint(juce::Graphics &g) override
    {
        for (int i = 0; i < n; i++)
            drawLed(g, centre(i), 6.0f, shown[(size_t)i]);
    }
    void poll() override
    {
        bool ch = false;
        for (int i = 0; i < n; i++) {
            float v = juce::jlimit(0.0f, 1.0f, level(i));
            v = std::round(v * 16.0f) / 16.0f;
            if (v != shown[(size_t)i]) {
                shown[(size_t)i] = v;
                ch = true;
            }
        }
        if (ch)
            repaint();
    }
    void mouseDown(const juce::MouseEvent &e) override
    {
        if (!pick)
            return;
        float best = 1e9f;
        int bi = 0;
        for (int i = 0; i < n; i++) {
            float d = centre(i).getDistanceFrom(e.position);
            if (d < best) {
                best = d;
                bi = i;
            }
        }
        pick(bi);
    }

private:
    juce::Point<float> centre(int i) const
    {
        if (horizontal)
            return {((float)i + 0.5f) * (float)getWidth() / (float)n, (float)getHeight() * 0.5f};
        return {5.0f, ((float)i + 0.5f) * (float)getHeight() / (float)n};
    }
    int n;
    std::function<float(int)> level;
    std::function<void(int)> pick;
    bool horizontal;
    std::vector<float> shown;
};

} // namespace

/* ---------------------------------------------------------------------------------- OLED --- */
class Oled : public juce::Component, public Panel::Live
{
public:
    explicit Oled(Panel &p) : panel(p) {}
    void poll() override
    {
        juce::String s = signature();
        if (s != last) {
            last = s;
            repaint();
        }
    }
    void paint(juce::Graphics &g) override
    {
        using namespace juce;
        auto r = getLocalBounds().toFloat();
        g.setColour(Colour(0xff000000));
        g.fillRoundedRectangle(r, 2.0f);
        g.setColour(Colours::white.withAlpha(0.08f));
        g.drawRoundedRectangle(r.reduced(0.5f), 2.0f, 1.0f);
        auto in = r.reduced(8.0f, 6.0f);
        auto &proc = panel.proc;
        int l = panel.layer();
        auto mono = [&](float h) { return font(Face::mono, h); };
        auto sans = [&](float h) { return font(Face::label, h); };
        /* the top line: the program, the layer, the tempo */
        int prog = proc.getCurrentProgram();
        String pn = prog < NFACTORY ? "F" + String(prog + 1).paddedLeft('0', 2) : "U" + String(prog - NFACTORY + 1).paddedLeft('0', 2);
        g.setColour(col::oled);
        g.setFont(mono(14.2f));
        auto top = in.removeFromTop(15.0f);
        g.drawText(pn + (l ? "  B" : "  A") + (proc.compare ? " CMP" : ""), top, Justification::centredLeft, false);
        g.drawText(String((int)std::lround(proc.bpmNow.load())), top, Justification::centredRight, false);
        in.removeFromTop(1.0f);
        auto page = panel.page;
        if (page == Panel::P_HOME) {
            g.setFont(sans(15.5f));
            g.drawText(proc.programName(), in.removeFromTop(17.0f), Justification::centredLeft, true);
            g.setColour(col::oledDim);
            g.fillRect(in.removeFromTop(1.0f));
            in.removeFromTop(3.0f);
            g.setColour(col::oled);
            String name, val;
            what(name, val);
            g.setFont(sans(13.5f));
            g.drawText(name, in.removeFromTop(16.0f), Justification::centred, true);
            g.setFont(sans(val.length() > 9 ? 13.0f : 18.0f));
            g.drawFittedText(val, in.toNearestInt(), Justification::centred, 1, 0.6f);
            return;
        }
        if (page == Panel::P_STEPS) {
            auto s = proc.seqCopy(l);
            int t = panel.track;
            g.setFont(sans(13.5f));
            int dd = proc.value(l, L_SEQ_D1 + t);
            g.drawText("Track " + String(t + 1) + "  " + MODDST_NAME[dd], in.removeFromTop(16.0f), Justification::centredLeft, true);
            auto bars = in.removeFromTop(in.getHeight() - 12.0f);
            stepsArea = bars;
            float bw = bars.getWidth() / 16.0f;
            int playing = proc.seqStepNow[l].load();
            for (int i = 0; i < 16; i++) {
                int v = s.gated[t][i];
                auto b = Rectangle<float>(bars.getX() + bw * (float)i + 0.5f, bars.getY(), bw - 1.0f, bars.getHeight());
                if (v >= GSEQ_RESET) {
                    g.setColour(col::oledDim);
                    g.drawText("R", b, Justification::centred, false);
                    continue;
                }
                float hgt = b.getHeight() * (float)v / 125.0f;
                g.setColour(i == panel.step ? col::oled : col::oled.withAlpha(0.55f));
                g.fillRect(b.withTop(b.getBottom() - juce::jmax(1.0f, hgt)));
                if (playing >= 0 && (playing % 16) == i) {
                    g.setColour(col::oled);
                    g.fillRect(b.withHeight(2.0f).withY(b.getBottom() + 1.0f));
                }
            }
            g.setFont(mono(13.5f));
            int v = s.gated[t][panel.step];
            g.drawText("Step " + String(panel.step + 1) + ":  " + (v >= GSEQ_RESET ? String("Reset") : String(v)), in, Justification::centredLeft, false);
            return;
        }
        if (page == Panel::P_POLYREC) {
            auto s = proc.seqCopy(l);
            bool rec = proc.recording[l].load();
            int st = rec ? proc.recStepNow[l].load() : juce::jmax(0, proc.seqStepNow[l].load());
            g.setFont(sans(14.2f));
            g.drawText(String(rec ? "RECORD" : "POLY SEQ") + "  Step " + String(st + 1) + " / " + String(juce::jmax<int>(s.length, 1)),
                       in.removeFromTop(17.0f), Justification::centredLeft, true);
            g.setFont(mono(13.5f));
            String notes;
            static const char *const N[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
            int show = rec && st > 0 ? st - 1 : st;
            for (int k = 0; k < PSEQ_NOTES; k++) {
                int n = s.note[show % PSEQ_STEPS][k];
                if (n == PN_TIE && k == 0)
                    notes = "tie";
                else if (n >= 2)
                    notes += String(N[(n - 2) % 12]) + String((n - 2) / 12 - 2) + " ";
            }
            g.drawText(rec ? "last: " : "now: ", in.removeFromTop(16.0f), Justification::centredLeft, false);
            g.drawFittedText(notes.isEmpty() ? String("rest") : notes, in.toNearestInt(), Justification::centredTop, 2, 0.7f);
            return;
        }
        /* a page of lines: the one in hand large, its neighbours small */
        auto es = panel.entries();
        if (es.empty())
            return;
        int it = juce::jlimit(0, (int)es.size() - 1, panel.item);
        static const char *const TITLE[] = {"", "MISC PARAMS", "MOD SLOT", "ARP", "SEQ", "", "GLOBAL", ""};
        g.setFont(sans(12.8f));
        g.setColour(col::oledDim);
        g.drawText(String(TITLE[page]) + "  " + String(it + 1) + "/" + String((int)es.size()), in.removeFromTop(15.0f), Justification::centredLeft, false);
        g.setColour(col::oled);
        g.setFont(sans(14.9f));
        g.drawText(es[(size_t)it].label, in.removeFromTop(17.0f), Justification::centredLeft, true);
        auto &e = es[(size_t)it];
        int v = e.get();
        g.setFont(sans(23.0f));
        g.drawFittedText(e.text ? e.text(v) : String(v), in.removeFromTop(30.0f).toNearestInt(), Justification::centred, 1, 0.6f);
        if (it + 1 < (int)es.size()) {
            g.setColour(col::oledDim);
            g.setFont(sans(12.8f));
            g.drawText(es[(size_t)it + 1].label, in, Justification::centredLeft, true);
        }
    }
    void mouseWheelMove(const juce::MouseEvent &, const juce::MouseWheelDetails &w) override
    {
        int d = w.deltaY > 0 ? 1 : w.deltaY < 0 ? -1 : 0;
        if (d)
            panel.valueTurn(d);
    }
    void mouseDown(const juce::MouseEvent &e) override
    {
        if (panel.page == Panel::P_STEPS && stepsArea.contains(e.position)) {
            drawStep(e.position);
            return;
        }
        if (panel.page == Panel::P_HOME)
            panel.programMenu(getScreenBounds());
        else if (panel.page != Panel::P_POLYREC)
            panel.entryMenu(getScreenBounds());
    }
    void mouseDrag(const juce::MouseEvent &e) override
    {
        if (panel.page == Panel::P_STEPS)
            drawStep(e.position);
    }

private:
    void drawStep(juce::Point<float> p)
    {
        auto b = stepsArea;
        int i = juce::jlimit(0, 15, (int)((p.x - b.getX()) / (b.getWidth() / 16.0f)));
        int v = juce::jlimit(0, 125, (int)std::lround((b.getBottom() - p.y) / b.getHeight() * 125.0f));
        panel.step = i;
        panel.proc.command(ReevesProcessor::C_GSTEP, panel.layer(), panel.track, i, v);
        repaint();
    }
    void what(juce::String &name, juce::String &val) const
    {
        if (panel.shownGlobal) {
            name = panel.shownGlobal->getName(30);
            val = panel.shownGlobal->getCurrentValueAsText();
            return;
        }
        if (panel.shownParam < 0) {
            name = "REEVES";
            val = juce::String(panel.proc.apvts.getRawParameterValue("voices")->load() > 0.5f ? "16 Voice" : "8 Voice");
            return;
        }
        int l = panel.shownLayer, p = panel.shownParam;
        name = LAYER[p].name;
        val = panel.valueText(l, p, panel.proc.value(l, p));
    }
    juce::String signature() const
    {
        auto &proc = panel.proc;
        int l = panel.layer();
        juce::String s;
        s << (int)panel.page << "|" << panel.item << "|" << l << "|" << proc.getCurrentProgram() << "|"
          << (int)std::lround(proc.bpmNow.load()) << "|" << (int)proc.compare << "|" << panel.shownParam << "|" << panel.shownLayer;
        juce::String n, v;
        if (panel.page == Panel::P_HOME) {
            what(n, v);
            s << n << v << proc.programName();
        } else if (panel.page == Panel::P_STEPS || panel.page == Panel::P_POLYREC) {
            auto sd = proc.seqCopy(l);
            uint64_t hsh = 1469598103934665603ull;    /* (FNV-1a over the sequence's bytes) */
            for (size_t i = 0; i < sizeof sd; i++)
                hsh = (hsh ^ reinterpret_cast<const uint8_t *>(&sd)[i]) * 1099511628211ull;
            s << (juce::int64)hsh << "|" << panel.track << "|" << panel.step << "|"
              << proc.seqStepNow[l].load() << "|" << proc.recStepNow[l].load() << "|" << (int)proc.recording[l].load()
              << proc.value(l, L_SEQ_D1 + panel.track);
        } else {
            auto es = panel.entries();
            if (!es.empty()) {
                auto &e = es[(size_t)juce::jlimit(0, (int)es.size() - 1, panel.item)];
                s << e.label << e.get();
            }
        }
        return s;
    }
    Panel &panel;
    juce::String last;
    juce::Rectangle<float> stepsArea;
};

/* ============================================================================ the panel === */
template <class T, class... A> T *Panel::add(A &&...a)
{
    auto c = std::make_unique<T>(std::forward<A>(a)...);
    T *raw = c.get();
    addAndMakeVisible(*raw);
    if (auto *lv = dynamic_cast<Live *>(raw))
        live.push_back(lv);
    parts.push_back(std::move(c));
    return raw;
}

Panel::ReevesPanel(ReevesProcessor &p) : proc(p)
{
    setLookAndFeel(&look);
    setSize(W, H);
    build();
    startTimerHz(30);
}
Panel::~ReevesPanel()
{
    stopTimer();
    if (dialog != nullptr)                       /* (an open WRITE dialog goes with the editor: it uses its look) */
        delete dialog.getComponent();
    setLookAndFeel(nullptr);
}

juce::String Panel::valueText(int l, int p, int v) const
{
    char t[40];
    bool synced = false;
    int fxType = proc.value(l, L_FX_TYPE);
    if (p >= L_LFO1 && p < L_LFO_END && (p - L_LFO1) % LFO_N == LFO_FREQ)
        synced = proc.value(l, p - LFO_FREQ + LFO_CLK) != 0;
    if (p == L_FX_P1)
        synced = proc.value(l, L_FX_CLK) != 0;
    paramText(p, v, t, sizeof t, synced, fxType);
    return juce::String(t);
}

void Panel::touch(int l, int p)
{
    shownLayer = l;
    shownParam = p;
    shownGlobal = nullptr;
    if (page != P_STEPS && page != P_POLYREC)
        page = P_HOME;
}
void Panel::touchGlobal(juce::RangedAudioParameter *p)
{
    shownGlobal = p;
    if (page != P_STEPS && page != P_POLYREC)
        page = P_HOME;
}

/* the pages' lines */
std::vector<Panel::Entry> Panel::entries() const
{
    std::vector<Entry> es;
    int l = layer();
    auto *self = const_cast<Panel *>(this);
    auto pe = [&](int p, const char *label = nullptr) {
        Entry e;
        e.label = label ? label : LAYER[p].name;
        e.lo = LAYER[p].lo;
        e.hi = LAYER[p].hi;
        e.get = [self, l, p] { return self->proc.value(l, p); };
        e.set = [self, l, p](int v) { self->proc.setValue(l, p, v); };
        e.text = [self, l, p](int v) { return self->valueText(l, p, v); };
        es.push_back(e);
    };
    switch (page) {
    case P_MISC:
        for (int p : {L_PGM_VOL, L_UNI_MODE, L_UNI_DETUNE, L_KEY_MODE, L_PB_RANGE, L_GLIDE_MODE, L_O1_GLIDE, L_O2_GLIDE,
                      L_O1_KEY, L_O2_KEY, L_O1_RESET, L_O2_RESET, L_PAN_MODE, L_E3_REPEAT, L_VCA_LEVEL})
            pe(p);
        {
            Entry e;
            e.label = "Split Point";
            e.lo = 0;
            e.hi = 127;
            e.get = [self] { return self->proc.progValue(G_SPLIT_POINT); };
            e.set = [self](int v) { self->proc.setProgValue(G_SPLIT_POINT, v); };
            e.text = [](int v) {
                static const char *const N[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
                return juce::String(N[v % 12]) + juce::String(v / 12 - 2);
            };
            es.push_back(e);
        }
        for (int p : {L_MW_AMT, L_MW_DST, L_AT_AMT, L_AT_DST, L_BR_AMT, L_BR_DST, L_VEL_AMT, L_VEL_DST, L_FT_AMT, L_FT_DST})
            pe(p);
        break;
    case P_MOD: {
        Entry e;
        e.label = "Mod Slot";
        e.lo = 0;
        e.hi = 7;
        e.get = [self] { return self->modSlot; };
        e.set = [self](int v) { self->modSlot = juce::jlimit(0, 7, v); };
        e.text = [](int v) { return juce::String(v + 1); };
        es.push_back(e);
        int b = L_MOD1 + modSlot * MOD_N;
        pe(b + MOD_SRC, "Source");
        pe(b + MOD_DST, "Destination");
        pe(b + MOD_AMT, "Amount");
        break;
    }
    case P_ARP:
        for (int p : {L_ARP_MODE, L_ARP_RANGE, L_ARP_REPEAT, L_ARP_RELATCH, L_DIVIDE})
            pe(p);
        break;
    case P_GSEQ: {
        pe(L_SEQ_MODE);
        for (int t = 0; t < 4; t++)
            pe(L_SEQ_D1 + t);
        break;
    }
    case P_GLOBAL: {
        auto gp = [&](const char *id, const char *label) {
            auto *q = proc.apvts.getParameter(id);
            Entry e;
            e.label = label;
            auto *ip = dynamic_cast<juce::AudioParameterInt *>(q);
            e.lo = ip ? ip->getRange().getStart() : 0;
            e.hi = ip ? ip->getRange().getEnd() : 1;
            e.get = [q, ip] { return ip ? ip->get() : (q->getValue() > 0.5f ? 1 : 0); };
            e.set = [q, ip](int v) {
                q->beginChangeGesture();
                q->setValueNotifyingHost(ip ? q->convertTo0to1((float)v) : (v ? 1.0f : 0.0f));
                q->endChangeGesture();
            };
            e.text = [q](int v) { return q->getText(q->convertTo0to1((float)v), 20); };
            if (!ip)
                e.text = [q](int v) { return q->getText(v ? 1.0f : 0.0f, 20); };
            es.push_back(e);
        };
        gp("voices", "Voices");
        gp("quality", "Quality (CPU)");
        gp("host_sync", "Host Sync");
        gp("transpose", "Transpose");
        {
            Entry e;
            e.label = "Window Size";
            e.lo = 5;
            e.hi = 16;
            e.get = [self] { return (int)std::lround(self->proc.uiScale.load() * 10.0f); };
            e.set = [self](int v) {
                if (auto *ed = self->findParentComponentOfClass<ReevesEditor>())
                    ed->setSize((int)std::lround(W * v / 10.0), (int)std::lround(H * v / 10.0));
            };
            e.text = [](int v) { return juce::String(v * 10) + " %"; };
            es.push_back(e);
        }
        {
            Entry e;
            e.label = "Panic (all notes off)";
            e.lo = 0;
            e.hi = 1;
            e.get = [] { return 0; };
            e.set = [self](int) { self->proc.command(ReevesProcessor::C_PANIC); };
            e.text = [](int) { return juce::String("turn VALUE"); };
            es.push_back(e);
        }
        {
            Entry e;
            e.label = "Basic Program";
            e.lo = 0;
            e.hi = 1;
            e.get = [] { return 0; };
            e.set = [self](int) { self->proc.setCurrentProgram(NFACTORY - 1); };
            e.text = [](int) { return juce::String("turn VALUE"); };
            es.push_back(e);
        }
        break;
    }
    default: break;
    }
    return es;
}

void Panel::openPage(Page p, int at)
{
    page = p;
    item = at;
}

void Panel::parameterTurn(int d)
{
    if (page == P_STEPS) {
        step = juce::jlimit(0, 15, step + d);
        return;
    }
    if (page == P_POLYREC) {
        if (d < 0)
            proc.command(ReevesProcessor::C_BACK, layer());
        return;
    }
    if (page == P_HOME) {
        openPage(P_MISC);
        return;
    }
    auto es = entries();
    item = juce::jlimit(0, juce::jmax(0, (int)es.size() - 1), item + d);
}

void Panel::valueTurn(int d)
{
    if (page == P_STEPS) {
        auto s = proc.seqCopy(layer());
        int v = juce::jlimit(0, GSEQ_RESET, (int)s.gated[track][step] + d);
        proc.command(ReevesProcessor::C_GSTEP, layer(), track, step, v);
        return;
    }
    if (page == P_HOME) {
        if (shownGlobal) {
            if (auto *ip = dynamic_cast<juce::AudioParameterInt *>(shownGlobal)) {
                shownGlobal->beginChangeGesture();
                shownGlobal->setValueNotifyingHost(shownGlobal->convertTo0to1((float)(ip->get() + d)));
                shownGlobal->endChangeGesture();
            } else {
                shownGlobal->beginChangeGesture();
                shownGlobal->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, shownGlobal->getValue() + 0.01f * (float)d));
                shownGlobal->endChangeGesture();
            }
        } else if (shownParam >= 0)
            proc.setValue(shownLayer, shownParam, proc.value(shownLayer, shownParam) + d);
        return;
    }
    auto es = entries();
    if (es.empty())
        return;
    auto &e = es[(size_t)juce::jlimit(0, (int)es.size() - 1, item)];
    e.set(juce::jlimit(e.lo, e.hi, e.get() + d));
}

void Panel::entryMenu(juce::Rectangle<int> screen)
{
    auto es = entries();
    if (es.empty())
        return;
    int it = juce::jlimit(0, (int)es.size() - 1, item);
    auto e = es[(size_t)it];
    juce::PopupMenu m;
    m.addSectionHeader(e.label);
    if (e.hi - e.lo > 60 && !e.text) {
        return;
    }
    int cur = e.get();
    if (e.hi - e.lo > 64) {                     /* a long range: in groups of 32 */
        for (int g0 = e.lo; g0 <= e.hi; g0 += 32) {
            juce::PopupMenu sub;
            int g1 = juce::jmin(e.hi, g0 + 31);
            for (int v = g0; v <= g1; v++)
                sub.addItem(v - e.lo + 1, e.text ? e.text(v) : juce::String(v), true, v == cur);
            m.addSubMenu((e.text ? e.text(g0) : juce::String(g0)) + "  ..  " + (e.text ? e.text(g1) : juce::String(g1)), sub, true,
                         nullptr, cur >= g0 && cur <= g1);
        }
    } else {
        for (int v = e.lo; v <= e.hi; v++)
            m.addItem(v - e.lo + 1, e.text ? e.text(v) : juce::String(v), true, v == cur);
    }
    juce::Component::SafePointer<Panel> self(this);
    m.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea(screen).withMinimumWidth(180), [self, e](int r) {
        if (self && r > 0)
            e.set(e.lo + r - 1);
    });
}

void Panel::programMenu(juce::Rectangle<int> screen)
{
    juce::PopupMenu m;
    m.addSectionHeader("FACTORY");
    for (int i = 0; i < NFACTORY; i++)
        m.addItem(i + 1, "F" + juce::String(i + 1).paddedLeft('0', 2) + "   " + proc.getProgramName(i), true, i == proc.getCurrentProgram());
    m.addSectionHeader("YOUR PROGRAMS");
    for (int i = NFACTORY; i < proc.getNumPrograms(); i++)
        m.addItem(i + 1, "U" + juce::String(i - NFACTORY + 1).paddedLeft('0', 2) + "   " + proc.getProgramName(i), true,
                  i == proc.getCurrentProgram());
    if (proc.userPrograms.empty())
        m.addItem(-1, "(WRITE keeps the sound as a program of your own)", false, false);
    juce::Component::SafePointer<Panel> self(this);
    m.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea(screen).withMinimumWidth(260), [self](int r) {
        if (self && r > 0) {
            self->proc.setCurrentProgram(r - 1);
            self->shownParam = -1;
            self->shownGlobal = nullptr;
            self->page = P_HOME;
        }
    });
}

void Panel::writeDialog()
{
    auto *w = new juce::AlertWindow("WRITE", "Keep the sound as a program of your own:", juce::MessageBoxIconType::NoIcon, this);
    w->setLookAndFeel(&look);
    w->addTextEditor("name", proc.programName(), "Name");
    w->addButton("Write", 1, juce::KeyPress(juce::KeyPress::returnKey));
    w->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    juce::Component::SafePointer<Panel> self(this);
    dialog = w;
    w->enterModalState(true, juce::ModalCallbackFunction::create([self, w](int r) {
                           if (self && r == 1)
                               self->proc.writeProgram(w->getTextEditorContents("name"));
                       }),
                       true);
}

/* ------------------------------------------------------------------------------- the build --- */
/* the panel's places, in the Rev2 desktop module's own layout (its photo's pixels: 1.3 times across, 1.55 down) */
static float X(float px) { return px * 1.3f; }
static float Y(float py) { return py * 1.55f - 22.0f; }
static juce::Point<float> at(float px, float py) { return {X(px), Y(py)}; }

void Panel::build()
{
    using juce::Rectangle;
    auto P = [this](int p) { return [this, p]() -> juce::RangedAudioParameter * { return proc.lp(layer(), p); }; };
    auto touchP = [this](int p) { return [this, p](juce::RangedAudioParameter *) { touch(layer(), p); }; };
    auto place = [](juce::Component *c, float px, float py, float w, float h) {
        c->setBounds(Rectangle<float>(w, h).withCentre(at(px, py)).toNearestInt());
    };
    auto knob = [&](float px, float py, int p) {
        auto *k = add<Knob>(*this, P(p), touchP(p));
        place(k, px, py, KD, KD);
        return k;
    };
    auto lknob = [&](float px, float py, int off) {
        auto *k = add<Knob>(*this, [this, off]() -> juce::RangedAudioParameter * { return proc.lp(layer(), L_LFO1 + lfoSel * LFO_N + off); },
                            [this, off](juce::RangedAudioParameter *) { touch(layer(), L_LFO1 + lfoSel * LFO_N + off); });
        place(k, px, py, KD, KD);
        return k;
    };
    auto encoder = [&](float px, float py, std::function<void(int)> f) {
        auto *k = add<Knob>(*this, std::move(f));
        place(k, px, py, KD, KD);
        return k;
    };
    auto button = [&](float px, float py, std::function<void()> press, std::function<bool()> on) {
        auto *b = add<Btn>(std::move(press), std::move(on));
        place(b, px, py, BW, BH);
        return b;
    };
    auto leds = [&](float px, float py0, int n, float step, std::function<float(int)> lv, std::function<void(int)> pick = {}) {
        auto *l = add<Leds>(n, std::move(lv), std::move(pick));
        l->setBounds((int)(X(px) - 6.0f), (int)(Y(py0) - step * 0.5f), 12, (int)(step * (float)n));
        return l;
    };
    auto toggle = [this](int p) {
        return [this, p] {
            proc.setValue(layer(), p, proc.value(layer(), p) ? 0 : 1);
            touch(layer(), p);
        };
    };
    auto isOn = [this](int p) { return [this, p] { return proc.value(layer(), p) != 0; }; };
    auto glob = [this](const char *id) { return proc.apvts.getParameter(id); };
    auto globToggle = [this, glob](const char *id) {
        return [this, glob, id] {
            auto *q = glob(id);
            q->beginChangeGesture();
            q->setValueNotifyingHost(q->getValue() > 0.5f ? 0.0f : 1.0f);
            q->endChangeGesture();
            touchGlobal(q);
        };
    };

    /* the top: MASTER VOLUME; MODULATION; RATE/TIME, GLIDE, UNISON, HOLD */
    {
        auto *k = add<Knob>(*this, [glob] { return glob("volume"); }, [this](juce::RangedAudioParameter *q) { touchGlobal(q); });
        place(k, 85, 100, KD, KD);
        auto modPick = [this](int off) {
            return [this, off] {
                openPage(P_MOD, off == MOD_SRC ? 1 : 2);
                if (oledView)
                    entryMenu(oledView->getScreenBounds());
            };
        };
        button(157, 85, modPick(MOD_SRC), [this] { return page == P_MOD && item == 1; });
        button(157, 147, modPick(MOD_DST), [this] { return page == P_MOD && item == 2; });
        encoder(233, 83, [this](int d) {
            modSlot = juce::jlimit(0, 7, modSlot + d);
            openPage(P_MOD, 0);
        });
        auto *amt = add<Knob>(*this, [this]() -> juce::RangedAudioParameter * { return proc.lp(layer(), L_MOD1 + modSlot * MOD_N + MOD_AMT); },
                              [this](juce::RangedAudioParameter *) { openPage(P_MOD, 3); });
        place(amt, 233, 148, KD, KD);
        auto *g = knob(322, 80, L_O1_GLIDE);
        g->afterSet = [this](int v) { proc.setValue(layer(), L_O2_GLIDE, v); };
        button(392, 77, toggle(L_GLIDE), isOn(L_GLIDE));
        button(320, 145,
               [this] {
                   int l = layer();
                   bool on = proc.value(l, L_UNISON) != 0;
                   proc.setValue(l, L_UNISON, on ? 0 : 1);
                   if (!on && proc.value(l, L_UNI_MODE) >= 17)
                       proc.command(ReevesProcessor::C_CHORD, l);
                   touch(l, L_UNISON);
               },
               isOn(L_UNISON));
        button(392, 145, [this] { proc.command(ReevesProcessor::C_HOLD, 0, 0, 0, proc.holdNow.load() ? 0 : 1); },
               [this] { return proc.holdNow.load(); });
    }
    /* the display: MISC PARAMS, PARAMETER and VALUE, the OLED, PROGRAM and BANK */
    {
        button(468, 110, [this] { openPage(page == P_MISC ? P_HOME : P_MISC); }, [this] { return page == P_MISC; });
        encoder(545, 82, [this](int d) { parameterTurn(d); });
        encoder(545, 150, [this](int d) { valueTurn(d); });
        auto *o = add<Oled>(*this);
        o->setBounds(Rectangle<float>(X(588), Y(66), X(683) - X(588), Y(146) - Y(66)).toNearestInt());
        oledView = o;
        encoder(708, 82, [this](int d) {
            int n = proc.getNumPrograms();
            proc.setCurrentProgram(((proc.getCurrentProgram() + d) % n + n) % n);
            shownParam = -1;
            shownGlobal = nullptr;
            page = P_HOME;
        });
        encoder(708, 150, [this](int d) {
            int cur = proc.getCurrentProgram(), n = proc.getNumPrograms();
            if (d > 0 && cur < NFACTORY && n > NFACTORY)
                proc.setCurrentProgram(NFACTORY);
            else if (d < 0 && cur >= NFACTORY)
                proc.setCurrentProgram(0);
            shownParam = -1;
            page = P_HOME;
        });
    }
    /* Write, Compare, Global; Split A|B, Stack A+B, Edit Layer B */
    button(773, 85, [this] { writeDialog(); }, [] { return false; });
    button(833, 85, [this] { proc.setCompare(!proc.compare); }, [this] { return proc.compare; });
    button(895, 85, [this] { openPage(page == P_GLOBAL ? P_HOME : P_GLOBAL); }, [this] { return page == P_GLOBAL; });
    button(773, 138, [this] { proc.setProgValue(G_LAYER_MODE, proc.progValue(G_LAYER_MODE) == LM_SPLIT ? LM_SINGLE : LM_SPLIT); },
           [this] { return proc.progValue(G_LAYER_MODE) == LM_SPLIT; });
    button(833, 138, [this] { proc.setProgValue(G_LAYER_MODE, proc.progValue(G_LAYER_MODE) == LM_STACK ? LM_SINGLE : LM_STACK); },
           [this] { return proc.progValue(G_LAYER_MODE) == LM_STACK; });
    button(895, 138, [this] { proc.editLayer = proc.editLayer.load() ? 0 : 1; }, [this] { return proc.editLayer.load() != 0; });
    /* REEVES' own, under the name: VOICES (8 or 16), HOST SYNC, the voices playing */
    {
        button(950, 156, globToggle("voices"), [] { return false; });
        leds(972, 151, 2, LS, [this](int i) { return (proc.apvts.getRawParameterValue("voices")->load() > 0.5f) == (i == 1) ? 1.0f : 0.0f; },
             [this, glob](int i) {
                 auto *q = glob("voices");
                 q->beginChangeGesture();
                 q->setValueNotifyingHost(i ? 1.0f : 0.0f);
                 q->endChangeGesture();
                 touchGlobal(q);
             });
        button(1045, 156, globToggle("host_sync"), [this] { return proc.apvts.getRawParameterValue("host_sync")->load() > 0.5f; });
        auto *act = add<Leds>(16, [this](int i) {
            int n = proc.apvts.getRawParameterValue("voices")->load() > 0.5f ? 16 : 8;
            return i >= n ? 0.0f : std::sqrt(proc.voiceLevel[i].load());
        },
                              std::function<void(int)>(), true);
        act->setBounds(Rectangle<float>(X(1190) - X(1100), 8.0f).withCentre(at(1145, 178)).toNearestInt());
    }

    /* LOW FREQUENCY OSCILLATORS */
    {
        const float by[4] = {205, 237, 268, 300};
        for (int k = 0; k < 4; k++)
            button(85, by[k], [this, k] { lfoSel = k; }, [this, k] { return lfoSel == k; });
        auto *al = add<Leds>(4, [this](int k) {
            int l = layer();
            if (proc.value(l, L_LFO1 + k * LFO_N + LFO_AMT) == 0)
                return 0.0f;
            return std::fabs(proc.lfoNow[l][k].load());
        });
        al->setBounds((int)X(107) - 6, (int)Y(205) - 20, 12, (int)(Y(300) - Y(205)) + 40);
        button(128, 222,
               [this] {
                   int l = layer(), p = L_LFO1 + lfoSel * LFO_N + LFO_SHAPE;
                   proc.setValue(l, p, (proc.value(l, p) + 1) % NLFOSHAPE);
                   touch(l, p);
               },
               [] { return false; });
        leds(150, 196, NLFOSHAPE, LS, [this](int i) { return proc.value(layer(), L_LFO1 + lfoSel * LFO_N + LFO_SHAPE) == i ? 1.0f : 0.0f; },
             [this](int i) { proc.setValue(layer(), L_LFO1 + lfoSel * LFO_N + LFO_SHAPE, i); });
        lknob(222, 225, LFO_FREQ);
        lknob(145, 292, LFO_DEST);
        lknob(222, 292, LFO_AMT);
        auto lt = [this](int off) {
            return [this, off] {
                int l = layer(), p = L_LFO1 + lfoSel * LFO_N + off;
                proc.setValue(l, p, proc.value(l, p) ? 0 : 1);
                touch(l, p);
            };
        };
        auto lo = [this](int off) { return [this, off] { return proc.value(layer(), L_LFO1 + lfoSel * LFO_N + off) != 0; }; };
        button(268, 222, lt(LFO_CLK), lo(LFO_CLK));
        button(268, 288, lt(LFO_KEY), lo(LFO_KEY));
    }
    /* CLOCK */
    knob(334, 228, L_BPM);
    knob(402, 228, L_DIVIDE);
    button(370, 288,
           [this] {
               double now = juce::Time::getMillisecondCounterHiRes() * 0.001;
               if (!taps.empty() && now - taps.back() > 2.0)
                   taps.clear();
               taps.push_back(now);
               if (taps.size() > 5)
                   taps.erase(taps.begin());
               if (taps.size() >= 2) {
                   double avg = (taps.back() - taps.front()) / (double)(taps.size() - 1);
                   proc.setValue(layer(), L_BPM, (int)std::lround(60.0 / avg));
                   touch(layer(), L_BPM);
               }
           },
           [this] { return proc.beatLed[layer()].load(); });
    /* ARP */
    button(476, 222, [this] { openPage(page == P_ARP ? P_HOME : P_ARP); }, [this] { return page == P_ARP; });
    button(476, 288, toggle(L_ARP_ON), isOn(L_ARP_ON));
    /* SEQUENCER */
    {
        auto gated = [this] { return proc.value(layer(), L_SEQ_TYPE) == ST_GATED; };
        button(543, 222,
               [this, gated] {
                   int l = layer();
                   if (proc.value(l, L_SEQ_ON) && !gated())
                       proc.setValue(l, L_SEQ_ON, 0);
                   proc.setValue(l, L_SEQ_TYPE, gated() ? ST_POLY : ST_GATED);
                   touch(l, L_SEQ_TYPE);
               },
               [] { return false; });
        leds(565, 213, 2, LS, [this](int i) { return (proc.value(layer(), L_SEQ_TYPE) == ST_POLY) == (i == 1) ? 1.0f : 0.0f; },
             [this](int i) { proc.setValue(layer(), L_SEQ_TYPE, i ? ST_POLY : ST_GATED); });
        button(573, 288,
               [this] {
                   int l = layer();
                   proc.setValue(l, L_SEQ_ON, proc.value(l, L_SEQ_ON) ? 0 : 1);
                   if (proc.value(l, L_SEQ_TYPE) == ST_POLY)
                       openPage(P_POLYREC);
               },
               isOn(L_SEQ_ON));
        button(627, 222,
               [this, gated] {
                   int l = layer();
                   if (gated())
                       openPage(page == P_GSEQ ? P_HOME : P_GSEQ, 0);
                   else if (proc.recording[l].load())
                       proc.command(ReevesProcessor::C_REST, l);
               },
               [this] { return page == P_GSEQ && item == 0; });
        encoder(682, 225, [this](int d) {
            track = juce::jlimit(0, 3, track + d);
            if (page != P_STEPS)
                openPage(P_GSEQ, 1 + track);
        });
        button(627, 288,
               [this, gated] {
                   int l = layer();
                   if (gated())
                       openPage(P_GSEQ, 1 + track);
                   else if (proc.recording[l].load())
                       proc.command(ReevesProcessor::C_TIE, l);
               },
               [this] { return page == P_GSEQ && item > 0; });
        button(682, 288,
               [this, gated] {
                   int l = layer();
                   if (gated())
                       openPage(page == P_STEPS ? P_HOME : P_STEPS);
                   else {
                       bool rec = proc.recording[l].load();
                       proc.command(ReevesProcessor::C_RECORD, l, 0, 0, rec ? 0 : 1);
                       openPage(P_POLYREC);
                   }
               },
               [this] { return page == P_STEPS || proc.recording[layer()].load(); });
    }
    /* EFFECT */
    button(747, 222, toggle(L_FX_ON), isOn(L_FX_ON));
    button(747, 288, toggle(L_FX_CLK), isOn(L_FX_CLK));
    knob(815, 225, L_FX_TYPE);
    knob(880, 225, L_FX_MIX);
    knob(815, 292, L_FX_P1);
    knob(880, 292, L_FX_P2);
    /* AUXILIARY ENVELOPE */
    {
        const int top[4] = {L_E3_DEST, L_E3_AMT, L_E3_VEL, L_E3DEL}, bot[4] = {L_E3ATT, L_E3DEC, L_E3SUS, L_E3REL};
        for (int i = 0; i < 4; i++) {
            knob(955.0f + 70.0f * (float)i, 225, top[i]);
            knob(955.0f + 70.0f * (float)i, 292, bot[i]);
        }
    }
    /* OSCILLATORS */
    {
        auto shapeBtn = [&](float px, float py, int p) {
            button(px, py,
                   [this, p] {
                       int l = layer();
                       proc.setValue(l, p, (proc.value(l, p) + 1) % NSHAPE);
                       touch(l, p);
                   },
                   [] { return false; });
            leds(px + 22.0f, py - 19.0f, 4, LS, [this, p](int i) { return proc.value(layer(), p) == i + 1 ? 1.0f : 0.0f; },
                 [this, p](int i) {
                     int l = layer();
                     proc.setValue(l, p, proc.value(l, p) == i + 1 ? 0 : i + 1);
                     touch(l, p);
                 });
        };
        knob(85, 382, L_O1_FREQ);
        knob(152, 382, L_O1_FINE);
        shapeBtn(205, 374, L_O1_SHAPE);
        knob(300, 382, L_O1_SHMOD);
        knob(372, 382, L_SUB);
        knob(445, 382, L_NOISE);
        button(118, 420, toggle(L_SYNC), isOn(L_SYNC));
        knob(85, 448, L_O2_FREQ);
        knob(152, 448, L_O2_FINE);
        shapeBtn(205, 440, L_O2_SHAPE);
        knob(300, 448, L_O2_SHMOD);
        knob(372, 448, L_OSC_MIX);
        knob(445, 448, L_SLOP);
    }
    /* LOW-PASS FILTER */
    {
        const float xs[6] = {520, 590, 662, 735, 808, 880};
        const int top[6] = {L_CUTOFF, L_RESO, L_FENV_AMT, L_FVEL, L_KEYAMT, L_AUDIOMOD};
        for (int i = 0; i < 6; i++)
            knob(xs[i], 382, top[i]);
        button(518, 446, toggle(L_POLE4), isOn(L_POLE4));
        const int bot[5] = {L_FDEL, L_FATT, L_FDEC, L_FSUS, L_FREL};
        for (int i = 0; i < 5; i++)
            knob(xs[i + 1], 448, bot[i]);
    }
    /* AMPLIFIER */
    {
        const int top[4] = {L_PAN_SPREAD, L_AENV_AMT, L_AVEL, L_ADEL}, bot[4] = {L_AATT, L_ADEC, L_ASUS, L_AREL};
        for (int i = 0; i < 4; i++) {
            knob(955.0f + 70.0f * (float)i, 382, top[i]);
            knob(955.0f + 70.0f * (float)i, 448, bot[i]);
        }
    }
}

/* ------------------------------------------------------------------------------ painting --- */
void Panel::paintBackground(juce::Graphics &g, float sc)
{
    using namespace juce;
    auto all = Rectangle<float>(0.0f, 0.0f, (float)W, (float)H);
    const float cheek = 42.0f;
    auto face = all.reduced(cheek, 0.0f);
    drawPanelFace(g, face, sc);
    drawWood(g, all.withWidth(cheek), 3);
    drawWood(g, all.withLeft((float)W - cheek), 4);
    for (float x : {cheek, (float)W - cheek}) {
        bool left = x < 100.0f;
        g.setGradientFill(ColourGradient(Colours::black.withAlpha(0.55f), x, 0.0f, Colours::transparentBlack, x + (left ? 12.0f : -12.0f), 0.0f, false));
        g.fillRect(left ? Rectangle<float>(x, 0.0f, 12.0f, (float)H) : Rectangle<float>(x - 12.0f, 0.0f, 12.0f, (float)H));
    }
    auto R = [](float x0, float y0, float x1, float y1) { return Rectangle<float>(X(x0), Y(y0), X(x1) - X(x0), Y(y1) - Y(y0)); };
    auto C = [](float px, float py, float w, float h) { return Rectangle<float>(w, h).withCentre(at(px, py)); };

    /* the jacks' names along the top */
    {
        g.setColour(col::legend.withAlpha(0.85f));
        g.setFont(font(Face::label, 11.0f));
        const std::pair<float, const char *> J[] = {{125, "Phones"}, {165, "Left"}, {200, "Right"}, {245, "Left"}, {280, "Right"},
                                                     {585, "Pedal/CV"}, {628, "Sustain"}, {670, "Sequencer"}, {720, "MIDI Thru"},
                                                     {775, "MIDI Out"}, {830, "MIDI In"}, {880, "USB"}, {1060, "AC In"}, {1135, "On/Off"}};
        for (auto &j : J)
            g.drawText(j.second, C(j.first, 30, 70, 11), Justification::centred, false);
        g.setFont(font(Face::label, 8.0f));
        g.drawText("MAIN/A OUTPUT", C(183, 42, 100, 10), Justification::centred, false);
        g.drawText("B OUTPUT", C(262, 42, 80, 10), Justification::centred, false);
    }
    /* the name: REEVES, wide (its voices outlined under it: paint()) */
    {
        auto f = font(Face::logo, 44.0f);
        GlyphArrangement ga;
        ga.addLineOfText(f, "REEVES", 0.0f, 0.0f);
        Path p;
        ga.createPath(p);
        auto b = p.getBounds();
        auto dst = R(932, 82, 1188, 110);
        p.applyTransform(AffineTransform::translation(-b.getX(), -b.getY()).scaled(dst.getWidth() / b.getWidth(), dst.getHeight() / b.getHeight())
                             .translated(dst.getX(), dst.getY()));
        g.setColour(col::legend);
        g.fillPath(p);
    }

    auto under = [&](float px, float py, const char *t, float size = 10.5f) {
        auto c = at(px, py);
        drawLegend(g, t, Rectangle<float>(c.x - 50.0f, c.y + 36.0f, 100.0f, 17.0f), size * FS);
    };
    auto underB = [&](float px, float py, const char *t, float size = 10.0f) {
        auto c = at(px, py);
        drawLegend(g, t, Rectangle<float>(c.x - 52.0f, c.y + 13.0f, 104.0f, 17.0f), juce::jmax(10.0f, size) * FS);
    };
    auto small = [&](Rectangle<float> r, const char *t, Justification j = Justification::centredLeft, float size = 9.0f) {
        g.setFont(font(Face::title, size * FS));
        g.setColour(col::legend);
        g.drawText(t, r, j, false);
    };

    /* the top row */
    under(85, 100, "MASTER VOLUME", 10.0f);
    {
        /* MODULATION: its name over a line, as on the module */
        auto f = font(Face::title, 16.5f).withExtraKerningFactor(0.05f);
        auto r = R(130, 54, 265, 66);
        float tw = textWidth(f, "MODULATION");
        g.setColour(col::frame);
        g.fillRect(Rectangle<float>(r.getX(), r.getCentreY() - 0.8f, (r.getWidth() - tw) * 0.5f - 6.0f, 1.6f));
        g.fillRect(Rectangle<float>(r.getCentreX() + tw * 0.5f + 6.0f, r.getCentreY() - 0.8f, (r.getWidth() - tw) * 0.5f - 6.0f, 1.6f));
        g.setColour(col::legend);
        g.setFont(f);
        g.drawText("MODULATION", r, Justification::centred, false);
    }
    underB(157, 85, "ASSIGN SRC");
    underB(157, 147, "ASSIGN DEST");
    under(233, 83, "SELECT");
    under(233, 148, "AMOUNT");
    under(322, 80, "RATE/TIME");
    underB(392, 77, "GLIDE");
    underB(320, 145, "UNISON");
    underB(392, 145, "HOLD");
    small(C(468, 96, 50, 14), "show", Justification::centred, 8.5f);
    underB(468, 110, "MISC PARAMS", 9.5f);
    under(545, 82, "PARAMETER");
    under(545, 150, "VALUE");
    under(708, 82, "PROGRAM");
    under(708, 150, "BANK");
    underB(773, 85, "Write");
    underB(833, 85, "Compare");
    underB(895, 85, "Global");
    underB(773, 138, "Split A|B");
    underB(833, 138, "Stack A+B");
    underB(895, 138, "Edit Layer B", 9.5f);
    underB(950, 156, "VOICES");
    small(Rectangle<float>(X(972) + 9.0f, Y(151) - LS, 80.0f, LS), "8 Voice");
    small(Rectangle<float>(X(972) + 9.0f, Y(151), 80.0f, LS), "16 Voice");
    underB(1045, 156, "HOST SYNC");
    drawTag(g, R(1100, 140, 1190, 172));

    /* the sections */
    struct Sec {
        float x0, y0, x1, y1;
        const char *t;
    };
    const Sec S[] = {{55, 187, 293, 331, "LOW FREQUENCY OSCILLATORS"},
                     {305, 187, 435, 331, "CLOCK"},
                     {447, 187, 505, 331, "ARP"},
                     {517, 187, 710, 331, "SEQUENCER"},
                     {722, 187, 908, 331, "EFFECT"},
                     {920, 187, 1195, 331, "AUXILIARY ENVELOPE"},
                     {55, 341, 476, 483, "OSCILLATORS"},
                     {488, 341, 907, 483, "LOW-PASS FILTER"},
                     {920, 341, 1195, 483, "AMPLIFIER"}};
    for (auto &s : S)
        drawSection(g, R(s.x0, s.y0, s.x1, s.y1), s.t);

    /* LFOs */
    {
        const float by[4] = {205, 237, 268, 300};
        g.setFont(font(Face::title, 11.0f * FS));
        g.setColour(col::legend);
        for (int k = 0; k < 4; k++)
            g.drawText(String(k + 1), C(66, by[k], 16, 16), Justification::centred, false);
        underB(128, 222, "SHAPE");
        for (int i = 0; i < NLFOSHAPE; i++)
            small(Rectangle<float>(X(150) + 9.0f, Y(196) - LS * 0.5f + LS * (float)i, 80.0f, LS), LFOSHAPE_NAME[i], Justification::centredLeft, 8.5f);
        under(222, 225, "FREQUENCY", 10.0f);
        underB(268, 222, "CLK SYNC", 9.0f);
        under(145, 292, "DESTINATION", 10.0f);
        under(222, 292, "AMOUNT");
        underB(268, 288, "KEY SYNC", 9.0f);
    }
    under(334, 228, "BPM");
    under(402, 228, "DIVIDE");
    underB(370, 288, "TAP TEMPO");
    underB(476, 222, "MODE");
    underB(476, 288, "ON/OFF");
    /* SEQUENCER */
    {
        underB(543, 222, "TYPE");
        small(Rectangle<float>(X(565) + 9.0f, Y(213) - LS, 60.0f, LS), "Gated");
        small(Rectangle<float>(X(565) + 9.0f, Y(213), 60.0f, LS), "Poly");
        g.setColour(col::legend);
        g.drawRoundedRectangle(Rectangle<float>(X(565) - 10.0f, Y(213) - 1.0f, 60.0f, LS + 2.0f), 7.0f, 1.2f);
        g.drawArrow(Line<float>(X(573), Y(213) + LS + 2.0f, X(573), Y(288) - 13.0f), 1.2f, 7.0f, 7.0f);
        small(C(627, 203, 50, 14), "rest", Justification::centred, 8.5f);
        small(C(627, 269, 50, 14), "tie", Justification::centred, 8.5f);
        auto pb = C(573, 307, 50, 17);
        g.setColour(col::legend);
        g.drawRoundedRectangle(pb, 6.0f, 1.0f);
        drawLegend(g, "PLAY", pb, 10.0f * FS);
        underB(627, 222, "MODE");
        under(682, 225, "TRACK");
        underB(627, 288, "DESTINATION", 9.5f);
        underB(682, 288, "RECORD");
    }
    underB(747, 222, "ON/OFF");
    underB(747, 288, "CLK SYNC");
    under(815, 225, "SELECT");
    under(880, 225, "MIX");
    under(815, 292, "PARAM 1");
    under(880, 292, "PARAM 2");
    {
        const char *top[4] = {"DESTINATION", "ENV AMOUNT", "VELOCITY", "DELAY"}, *bot[4] = {"ATTACK", "DECAY", "SUSTAIN", "RELEASE"};
        for (int i = 0; i < 4; i++) {
            float x = 955.0f + 70.0f * (float)i;
            under(x, 225, top[i], 10.0f);
            under(x, 292, bot[i]);
            under(x, 382, i == 0 ? "PAN SPREAD" : top[i], 10.0f);
            under(x, 448, bot[i]);
        }
    }
    /* OSCILLATORS */
    {
        under(85, 382, "OSC 1 FREQ", 10.0f);
        under(152, 382, "FINE TUNE", 10.0f);
        underB(205, 374, "SHAPE");
        under(300, 382, "SHAPE MOD", 10.0f);
        under(372, 382, "SUB OCTAVE", 10.0f);
        under(445, 382, "NOISE");
        g.setFont(font(Face::title, 10.0f));
        g.setColour(col::legend);
        g.setFont(font(Face::title, 10.0f * FS));
        g.drawText("SYNC", Rectangle<float>(X(118) - 23.0f - 50.0f, Y(420) - 8.0f, 50.0f, 16.0f), Justification::centredRight, false);
        under(85, 448, "OSC 2 FREQ", 10.0f);
        under(152, 448, "FINE TUNE", 10.0f);
        underB(205, 440, "SHAPE");
        under(300, 448, "SHAPE MOD", 10.0f);
        under(372, 448, "OSC MIX", 10.0f);
        under(445, 448, "OSC SLOP", 10.0f);
        g.setFont(font(Face::title, 9.5f * FS));
        g.drawText("1", C(349, 471, 14, 14), Justification::centred, false);
        g.drawText("2", C(395, 471, 14, 14), Justification::centred, false);
        const char *SN[4] = {"Sawtooth", "Saw+Tri", "Triangle", "Pulse"};
        for (float py : {355.0f, 421.0f})
            for (int i = 0; i < 4; i++)
                small(Rectangle<float>(X(227) + 9.0f, Y(py) - LS * 0.5f + LS * (float)i, 80.0f, LS), SN[i], Justification::centredLeft, 8.5f);
    }
    /* LOW-PASS FILTER */
    {
        const float xs[6] = {520, 590, 662, 735, 808, 880};
        const char *top[6] = {"CUTOFF", "RESONANCE", "ENV AMOUNT", "VELOCITY", "KEY AMOUNT", "AUDIO MOD"};
        for (int i = 0; i < 6; i++)
            under(xs[i], 382, top[i], 10.0f);
        underB(518, 446, "4 POLE");
        const char *bot[5] = {"DELAY", "ATTACK", "DECAY", "SUSTAIN", "RELEASE"};
        for (int i = 0; i < 5; i++)
            under(xs[i + 1], 448, bot[i]);
    }
    /* the screws, the sheet's edge, and a few years of use over it all (worn round the knobs played most) */
    for (float x : {60.0f, (float)W - 60.0f})
        for (float y : {12.0f, (float)H - 12.0f})
            drawScrew(g, {x, y}, 6.0f);
    drawPanelEdge(g, face);
    drawWear(g, face, 1.0f / sc, {at(520, 382), at(590, 382), at(85, 100), at(708, 82), at(545, 150), at(662, 382), at(880, 225)});
}

void Panel::paint(juce::Graphics &g)
{
    float sc = g.getInternalContext().getPhysicalPixelScaleFactor();
    if (sc != backgroundScale || background.isNull()) {
        backgroundScale = sc;
        background = juce::Image(juce::Image::RGB, juce::jmax(1, (int)std::ceil(W * sc)), juce::jmax(1, (int)std::ceil(H * sc)), true);
        juce::Graphics bg(background);
        bg.addTransform(juce::AffineTransform::scale(sc));
        paintBackground(bg, sc);
    }
    g.drawImage(background, getLocalBounds().toFloat(), juce::RectanglePlacement::stretchToFit);
    /* the voices, outlined under the name (as the Rev2's model) */
    {
        using namespace juce;
        bool sixteen = proc.apvts.getRawParameterValue("voices")->load() > 0.5f;
        auto f = font(Face::logo, 30.0f);
        GlyphArrangement ga;
        ga.addLineOfText(f, sixteen ? "16" : "8", 0.0f, 0.0f);
        Path p;
        ga.createPath(p);
        auto b = p.getBounds();
        float w = sixteen ? 56.0f : 27.0f, h = 24.0f;
        p.applyTransform(AffineTransform::translation(-b.getX(), -b.getY()).scaled(w / b.getWidth(), h / b.getHeight()).translated(X(933), Y(117)));
        g.setColour(col::legend);
        g.strokePath(p, PathStrokeType(1.6f));
        g.setFont(font(Face::title, 15.0f));
        g.drawText("VOICE", Rectangle<float>(X(933) + w + 8.0f, Y(117) + 4.0f, 80.0f, 18.0f), Justification::centredLeft, false);
    }
}

void Panel::timerCallback()
{
    for (auto *l : live)
        l->poll();
    int sixteen = proc.apvts.getRawParameterValue("voices")->load() > 0.5f ? 16 : 8;
    if (sixteen != shownVoices) {
        shownVoices = sixteen;
        repaint(juce::Rectangle<int>((int)X(930), (int)Y(112), 200, 40));
    }
}

/* ============================================================================== the editor === */
ReevesEditor::ReevesEditor(ReevesProcessor &p) : AudioProcessorEditor(&p), proc(p), panel(p)
{
    addAndMakeVisible(panel);
    panel.setBounds(0, 0, ReevesPanel::W, ReevesPanel::H);
    const double W = ReevesPanel::W, H = ReevesPanel::H;
    /* the window: never larger than the screen it opens on (a MacBook Air too); the first time as large as the screen
     * comfortably allows, after that as it was left */
    auto area = juce::Rectangle<int>(0, 0, 1920, 1080);
    if (auto *d = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
        area = d->userArea;
    double fit = juce::jmin((area.getWidth() - 40) / W, (area.getHeight() - 90) / H);
    fit = juce::jlimit(0.45, 1.6, fit);
    setResizable(true, true);
    setResizeLimits((int)(W * 0.45), (int)(H * 0.45), (int)(W * juce::jmax(0.5, fit)), (int)(H * juce::jmax(0.5, fit)));
    getConstrainer()->setFixedAspectRatio(W / H);
    double s = proc.uiScaleSet ? (double)proc.uiScale.load() : juce::jmin(fit, 1.0);
    s = juce::jlimit(0.45, fit, s);
    setSize((int)std::lround(W * s), (int)std::lround(H * s));
}

void ReevesEditor::resized()
{
    float s = (float)getWidth() / (float)ReevesPanel::W;
    panel.setTransform(juce::AffineTransform::scale(s));
    proc.uiScale = s;
    proc.uiScaleSet = true;
}
