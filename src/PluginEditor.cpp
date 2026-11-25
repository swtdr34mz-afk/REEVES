/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (REEVES) */
#include "PluginEditor.h"

using namespace rvui;
using namespace rv;
using Panel = ReevesPanel;

namespace
{
constexpr float KD = 41.0f;                      /* a knob's size */
constexpr float BW = 26.0f, BH = 15.0f;          /* a button's */
const juce::Rectangle<float> OLED_R{922.0f, 70.0f, 100.0f, 82.0f};

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
        float pos = endless ? turned * 0.04f : shownPos;
        drawKnob(g, getLocalBounds().toFloat(), endless ? pos - std::floor(pos) : pos, bipolar, endless);
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
    bool endless = false, bipolar = false;
    float shownPos = -1.0f, startY = 0.0f, acc = 0.0f, turned = 0.0f;
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
    void paint(juce::Graphics &g) override { drawButton(g, getLocalBounds().toFloat().reduced(1.0f), lit, down, isMouseOver()); }
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

/* ------------------------------------------------------------------- the wheels, the keys --- */
class Wheel : public juce::Component
{
public:
    Wheel(bool spring, std::function<void(float)> send) : spring(spring), send(std::move(send)) { pos = spring ? 0.5f : 0.0f; }
    void paint(juce::Graphics &g) override
    {
        using namespace juce;
        auto well = getLocalBounds().toFloat();
        g.setColour(Colour(0xff030303));
        g.fillRoundedRectangle(well, 3.0f);
        auto wheel = well.reduced(4.0f, 6.0f);
        float h = wheel.getHeight();
        for (float y = 0.0f; y < h; y += 1.0f) {
            float t = (y / h) * 2.0f - 1.0f, shade = std::sqrt(jmax(0.0f, 1.0f - t * t));
            float rib = std::fmod((std::asin(jlimit(-1.0f, 1.0f, t)) * 9.0f + pos * 18.0f) + 100.0f, 1.0f);
            float v = 0.05f + 0.22f * shade * (rib < 0.5f ? 1.0f : 0.5f);
            g.setColour(Colour::fromFloatRGBA(v, v, v * 1.02f, 1.0f));
            g.fillRect(wheel.getX(), wheel.getY() + y, wheel.getWidth(), 1.0f);
        }
        g.setColour(Colours::black);
        g.drawRect(wheel, 1.0f);
    }
    void mouseDown(const juce::MouseEvent &e) override
    {
        start = pos;
        startY = e.position.y;
    }
    void mouseDrag(const juce::MouseEvent &e) override
    {
        set(juce::jlimit(0.0f, 1.0f, start - (e.position.y - startY) / ((float)getHeight() * 0.8f)));
    }
    void mouseUp(const juce::MouseEvent &) override
    {
        if (spring)
            set(0.5f);
    }

private:
    void set(float p)
    {
        if (p == pos)
            return;
        pos = p;
        send(p);
        repaint();
    }
    bool spring;
    std::function<void(float)> send;
    float pos, start = 0.0f, startY = 0.0f;
};

class Keys : public juce::MidiKeyboardComponent
{
public:
    explicit Keys(juce::MidiKeyboardState &s) : MidiKeyboardComponent(s, horizontalKeyboard)
    {
        setBlackNoteLengthProportion(0.62f);
        setBlackNoteWidthProportion(0.58f);
        setWantsKeyboardFocus(false);
        setMouseClickGrabsKeyboardFocus(false);
        setScrollButtonsVisible(false);
        setColour(whiteNoteColourId, juce::Colour(0xff0c0c0d));
        setColour(shadowColourId, juce::Colours::transparentBlack);
    }
    void drawWhiteNote(int, juce::Graphics &g, juce::Rectangle<float> a, bool down, bool, juce::Colour, juce::Colour) override
    {
        using namespace juce;
        auto r = a.reduced(1.0f, 0.0f);
        float lip = down ? 3.0f : 9.0f;
        g.setGradientFill(ColourGradient(down ? Colour(0xffdedcd6) : Colour(0xfffbfaf7), r.getX(), r.getY(),
                                         down ? Colour(0xffcfccc5) : Colour(0xffeceae5), r.getX(), r.getBottom(), false));
        g.fillRect(r.withTrimmedBottom(lip));
        g.setGradientFill(ColourGradient(Colour(0xffd6d4cd), r.getX(), r.getBottom() - lip, Colour(0xffadaba4), r.getX(), r.getBottom(), false));
        g.fillRect(r.withTop(r.getBottom() - lip));
        g.setGradientFill(ColourGradient(Colours::black.withAlpha(0.5f), r.getX(), r.getY(), Colours::transparentBlack, r.getX(), r.getY() + 16.0f, false));
        g.fillRect(r.withHeight(16.0f));
    }
    void drawBlackNote(int, juce::Graphics &g, juce::Rectangle<float> a, bool down, bool, juce::Colour) override
    {
        using namespace juce;
        g.setColour(Colours::black.withAlpha(0.5f));
        g.fillRect(a.translated(2.0f, 3.0f));
        g.setColour(Colour(0xff030303));
        g.fillRect(a);
        auto top = a.reduced(a.getWidth() * 0.12f, 0.0f).withTrimmedBottom(down ? a.getHeight() * 0.05f : a.getHeight() * 0.11f);
        g.setGradientFill(ColourGradient(Colour(0xff161618), top.getX(), top.getY(), Colour(0xff2a2a2d), top.getX(), top.getBottom(), false));
        g.fillRect(top);
        g.setColour(Colours::white.withAlpha(0.14f));
        g.fillRect(top.withWidth(1.5f).translated(1.0f, 0.0f));
    }
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
        auto in = r.reduced(5.0f, 4.0f);
        auto &proc = panel.proc;
        int l = panel.layer();
        auto mono = [&](float h) { return font(Face::mono, h); };
        auto sans = [&](float h) { return font(Face::label, h); };
        /* the top line: the program, the layer, the tempo */
        int prog = proc.getCurrentProgram();
        String pn = prog < NFACTORY ? "F" + String(prog + 1).paddedLeft('0', 2) : "U" + String(prog - NFACTORY + 1).paddedLeft('0', 2);
        g.setColour(col::oled);
        g.setFont(mono(10.5f));
        auto top = in.removeFromTop(11.0f);
        g.drawText(pn + (l ? "  B" : "  A") + (proc.compare ? " CMP" : ""), top, Justification::centredLeft, false);
        g.drawText(String((int)std::lround(proc.bpmNow.load())), top, Justification::centredRight, false);
        in.removeFromTop(1.0f);
        auto page = panel.page;
        if (page == Panel::P_HOME) {
            g.setFont(sans(11.5f));
            g.drawText(proc.programName(), in.removeFromTop(13.0f), Justification::centredLeft, true);
            g.setColour(col::oledDim);
            g.fillRect(in.removeFromTop(1.0f));
            in.removeFromTop(3.0f);
            g.setColour(col::oled);
            String name, val;
            what(name, val);
            g.setFont(sans(10.0f));
            g.drawText(name, in.removeFromTop(12.0f), Justification::centred, true);
            g.setFont(sans(val.length() > 9 ? 13.0f : 18.0f));
            g.drawFittedText(val, in.toNearestInt(), Justification::centred, 1, 0.6f);
            return;
        }
        if (page == Panel::P_STEPS) {
            auto s = proc.seqCopy(l);
            int t = panel.track;
            g.setFont(sans(10.0f));
            int dd = proc.value(l, L_SEQ_D1 + t);
            g.drawText("Track " + String(t + 1) + "  " + MODDST_NAME[dd], in.removeFromTop(12.0f), Justification::centredLeft, true);
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
            g.setFont(mono(10.0f));
            int v = s.gated[t][panel.step];
            g.drawText("Step " + String(panel.step + 1) + ":  " + (v >= GSEQ_RESET ? String("Reset") : String(v)), in, Justification::centredLeft, false);
            return;
        }
        if (page == Panel::P_POLYREC) {
            auto s = proc.seqCopy(l);
            bool rec = proc.recording[l].load();
            int st = rec ? proc.recStepNow[l].load() : juce::jmax(0, proc.seqStepNow[l].load());
            g.setFont(sans(10.5f));
            g.drawText(String(rec ? "RECORD" : "POLY SEQ") + "  Step " + String(st + 1) + " / " + String(juce::jmax<int>(s.length, 1)),
                       in.removeFromTop(13.0f), Justification::centredLeft, true);
            g.setFont(mono(10.0f));
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
            g.drawText(rec ? "last: " : "now: ", in.removeFromTop(12.0f), Justification::centredLeft, false);
            g.drawFittedText(notes.isEmpty() ? String("rest") : notes, in.toNearestInt(), Justification::centredTop, 2, 0.7f);
            return;
        }
        /* a page of lines: the one in hand large, its neighbours small */
        auto es = panel.entries();
        if (es.empty())
            return;
        int it = juce::jlimit(0, (int)es.size() - 1, panel.item);
        static const char *const TITLE[] = {"", "MISC PARAMS", "MOD SLOT", "ARP", "SEQ", "", "GLOBAL", ""};
        g.setFont(sans(9.5f));
        g.setColour(col::oledDim);
        g.drawText(String(TITLE[page]) + "  " + String(it + 1) + "/" + String((int)es.size()), in.removeFromTop(11.0f), Justification::centredLeft, false);
        g.setColour(col::oled);
        g.setFont(sans(11.0f));
        g.drawText(es[(size_t)it].label, in.removeFromTop(13.0f), Justification::centredLeft, true);
        auto &e = es[(size_t)it];
        int v = e.get();
        g.setFont(sans(17.0f));
        g.drawFittedText(e.text ? e.text(v) : String(v), in.removeFromTop(22.0f).toNearestInt(), Justification::centred, 1, 0.6f);
        if (it + 1 < (int)es.size()) {
            g.setColour(col::oledDim);
            g.setFont(sans(9.5f));
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
            s << juce::String::toHexString(&sd, (int)sizeof sd, 0).hashCode64() << "|" << panel.track << "|" << panel.step << "|"
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
    bool many = e.hi - e.lo > 40;
    juce::PopupMenu sub;
    for (int v = e.lo; v <= e.hi; v++) {
        juce::String t = e.text ? e.text(v) : juce::String(v);
        if (many && (v - e.lo) % 32 == 0 && v > e.lo) {
            m.addSubMenu(e.text ? e.text(v - 32) + " ..." : juce::String(v - 32) + " ...", sub);
            sub = juce::PopupMenu();
        }
        (many && e.hi - e.lo > 80 ? sub : m).addItem(v - e.lo + 1, t, true, v == cur);
    }
    if (many && e.hi - e.lo > 80)
        m.addSubMenu("...", sub);
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
    w->enterModalState(true, juce::ModalCallbackFunction::create([self, w](int r) {
                           if (self && r == 1)
                               self->proc.writeProgram(w->getTextEditorContents("name"));
                       }),
                       true);
}

/* ------------------------------------------------------------------------------- the build --- */
void Panel::build()
{
    using juce::Rectangle;
    auto P = [this](int p) { return [this, p]() -> juce::RangedAudioParameter * { return proc.lp(layer(), p); }; };
    auto touchP = [this](int p) { return [this, p](juce::RangedAudioParameter *) { touch(layer(), p); }; };
    auto knob = [&](float cx, float cy, int p) {
        auto *k = add<Knob>(*this, P(p), touchP(p));
        k->setBounds(Rectangle<float>(KD, KD).withCentre({cx, cy}).toNearestInt());
        return k;
    };
    /* the LFO's knobs: the selected LFO's */
    auto lknob = [&](float cx, float cy, int off) {
        auto *k = add<Knob>(*this, [this, off]() -> juce::RangedAudioParameter * { return proc.lp(layer(), L_LFO1 + lfoSel * LFO_N + off); },
                            [this, off](juce::RangedAudioParameter *) { touch(layer(), L_LFO1 + lfoSel * LFO_N + off); });
        k->setBounds(Rectangle<float>(KD, KD).withCentre({cx, cy}).toNearestInt());
        return k;
    };
    auto button = [&](float cx, float cy, std::function<void()> press, std::function<bool()> on) {
        auto *b = add<Btn>(std::move(press), std::move(on));
        b->setBounds(Rectangle<float>(BW, BH).withCentre({cx, cy}).toNearestInt());
        return b;
    };
    auto toggle = [this](int p) {
        return [this, p] {
            proc.setValue(layer(), p, proc.value(layer(), p) ? 0 : 1);
            touch(layer(), p);
        };
    };
    auto isOn = [this](int p) { return [this, p] { return proc.value(layer(), p) != 0; }; };
    auto glob = [this](const char *id) { return proc.apvts.getParameter(id); };

    /* the left: MASTER VOLUME, RATE/TIME and GLIDE, UNISON and HOLD, TRANSPOSE, the wheels */
    {
        auto *k = add<Knob>(*this, [glob] { return glob("volume"); }, [this](juce::RangedAudioParameter *q) { touchGlobal(q); });
        k->setBounds(Rectangle<float>(KD, KD).withCentre({318.0f, 92.0f}).toNearestInt());
        auto *g = knob(282.0f, 175.0f, L_O1_GLIDE);
        g->afterSet = [this](int v) { proc.setValue(layer(), L_O2_GLIDE, v); };
        button(345.0f, 168.0f, toggle(L_GLIDE), isOn(L_GLIDE));
        button(282.0f, 232.0f,
               [this] {
                   int l = layer();
                   bool on = proc.value(l, L_UNISON) != 0;
                   proc.setValue(l, L_UNISON, on ? 0 : 1);
                   if (!on && proc.value(l, L_UNI_MODE) >= 17)
                       proc.command(ReevesProcessor::C_CHORD, l);
                   touch(l, L_UNISON);
               },
               isOn(L_UNISON));
        button(345.0f, 232.0f, [this] { proc.command(ReevesProcessor::C_HOLD, 0, 0, 0, proc.holdNow.load() ? 0 : 1); },
               [this] { return proc.holdNow.load(); });
        auto *tl = add<Leds>(5, [this](int i) { return (int)std::lround(proc.apvts.getRawParameterValue("transpose")->load()) == i - 2 ? 1.0f : 0.0f; },
                             [this, glob](int i) {
                                 auto *q = glob("transpose");
                                 q->beginChangeGesture();
                                 q->setValueNotifyingHost(q->convertTo0to1((float)(i - 2)));
                                 q->endChangeGesture();
                             },
                             true);
        tl->setBounds(272, 276, 84, 12);
        auto tr = [this, glob](int d) {
            return [this, glob, d] {
                auto *q = glob("transpose");
                int v = juce::jlimit(-2, 2, (int)std::lround(q->convertFrom0to1(q->getValue())) + d);
                q->beginChangeGesture();
                q->setValueNotifyingHost(q->convertTo0to1((float)v));
                q->endChangeGesture();
                touchGlobal(q);
            };
        };
        button(282.0f, 300.0f, tr(-1), [] { return false; });
        button(345.0f, 300.0f, tr(1), [] { return false; });
        auto *pw = add<Wheel>(true, [this](float v) {
            proc.uiMidi.addMessageToQueue(juce::MidiMessage::pitchWheel(1, juce::jlimit(0, 16383, (int)std::lround(v * 16383.0f)))
                                              .withTimeStamp(juce::Time::getMillisecondCounterHiRes() * 0.001));
        });
        pw->setBounds(84, 168, 34, 132);
        auto *mw = add<Wheel>(false, [this](float v) {
            proc.uiMidi.addMessageToQueue(juce::MidiMessage::controllerEvent(1, 1, juce::jlimit(0, 127, (int)std::lround(v * 127.0f)))
                                              .withTimeStamp(juce::Time::getMillisecondCounterHiRes() * 0.001));
        });
        mw->setBounds(150, 168, 34, 132);
    }

    /* MODULATION: SOURCE and DESTINATION (the slot's, as a menu and on the display), SELECT (the slot), AMOUNT */
    {
        auto modPick = [this](int off) {
            return [this, off] {
                openPage(P_MOD, off == MOD_SRC ? 1 : 2);
                if (oledView)
                    entryMenu(oledView->getScreenBounds());
            };
        };
        button(413.0f, 100.0f, modPick(MOD_SRC), [this] { return page == P_MOD && item == 1; });
        button(413.0f, 158.0f, modPick(MOD_DST), [this] { return page == P_MOD && item == 2; });
        auto *sel = add<Knob>(*this, [this](int d) {
            modSlot = juce::jlimit(0, 7, modSlot + d);
            openPage(P_MOD, 0);
        });
        sel->setBounds(Rectangle<float>(KD, KD).withCentre({467.0f, 90.0f}).toNearestInt());
        auto *amt = add<Knob>(*this, [this]() -> juce::RangedAudioParameter * { return proc.lp(layer(), L_MOD1 + modSlot * MOD_N + MOD_AMT); },
                              [this](juce::RangedAudioParameter *) { openPage(P_MOD, 3); });
        amt->setBounds(Rectangle<float>(KD, KD).withCentre({467.0f, 150.0f}).toNearestInt());
    }
    /* CLOCK */
    knob(522.0f, 90.0f, L_BPM);
    knob(588.0f, 90.0f, L_DIVIDE);
    button(558.0f, 158.0f,
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
    button(652.0f, 100.0f, [this] { openPage(page == P_ARP ? P_HOME : P_ARP); }, [this] { return page == P_ARP; });
    button(652.0f, 158.0f, toggle(L_ARP_ON), isOn(L_ARP_ON));
    /* SEQUENCER */
    {
        auto gated = [this] { return proc.value(layer(), L_SEQ_TYPE) == ST_GATED; };
        button(712.0f, 100.0f,
               [this, gated] {
                   int l = layer();
                   if (proc.value(l, L_SEQ_ON) && !gated())
                       proc.setValue(l, L_SEQ_ON, 0);
                   proc.setValue(l, L_SEQ_TYPE, gated() ? ST_POLY : ST_GATED);
                   touch(l, L_SEQ_TYPE);
               },
               [] { return false; });
        auto *tl = add<Leds>(2, [this](int i) { return (proc.value(layer(), L_SEQ_TYPE) == ST_POLY) == (i == 1) ? 1.0f : 0.0f; },
                             [this](int i) { proc.setValue(layer(), L_SEQ_TYPE, i ? ST_POLY : ST_GATED); });
        tl->setBounds(733, 87, 12, 26);
        button(740.0f, 158.0f, [this] {
            int l = layer();
            proc.setValue(l, L_SEQ_ON, proc.value(l, L_SEQ_ON) ? 0 : 1);
            if (proc.value(l, L_SEQ_TYPE) == ST_POLY)
                openPage(P_POLYREC);
        },
               isOn(L_SEQ_ON));
        button(788.0f, 100.0f,
               [this, gated] {
                   int l = layer();
                   if (gated())
                       openPage(page == P_GSEQ ? P_HOME : P_GSEQ, 0);
                   else if (proc.recording[l].load())
                       proc.command(ReevesProcessor::C_REST, l);
               },
               [this] { return page == P_GSEQ && item == 0; });
        auto *trk = add<Knob>(*this, [this](int d) {
            track = juce::jlimit(0, 3, track + d);
            if (page != P_STEPS)
                openPage(P_GSEQ, 1 + track);
        });
        trk->setBounds(Rectangle<float>(KD, KD).withCentre({836.0f, 90.0f}).toNearestInt());
        button(788.0f, 158.0f,
               [this, gated] {
                   int l = layer();
                   if (gated())
                       openPage(P_GSEQ, 1 + track);
                   else if (proc.recording[l].load())
                       proc.command(ReevesProcessor::C_TIE, l);
               },
               [this] { return page == P_GSEQ && item > 0; });
        button(836.0f, 158.0f,
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
    /* the display: PARAMETER and VALUE, the OLED, MISC PARAMS, PROGRAM and BANK */
    {
        auto *pk = add<Knob>(*this, [this](int d) { parameterTurn(d); });
        pk->setBounds(Rectangle<float>(KD, KD).withCentre({893.0f, 90.0f}).toNearestInt());
        auto *vk = add<Knob>(*this, [this](int d) { valueTurn(d); });
        vk->setBounds(Rectangle<float>(KD, KD).withCentre({893.0f, 150.0f}).toNearestInt());
        auto *o = add<Oled>(*this);
        o->setBounds(OLED_R.toNearestInt());
        oledView = o;
        button(972.0f, 172.0f, [this] { openPage(page == P_MISC ? P_HOME : P_MISC); }, [this] { return page == P_MISC; });
        auto *pg = add<Knob>(*this, [this](int d) {
            int n = proc.getNumPrograms();
            proc.setCurrentProgram(((proc.getCurrentProgram() + d) % n + n) % n);
            shownParam = -1;
            shownGlobal = nullptr;
            page = P_HOME;
        });
        pg->setBounds(Rectangle<float>(KD, KD).withCentre({1050.0f, 90.0f}).toNearestInt());
        auto *bk = add<Knob>(*this, [this](int d) {
            /* BANK: between the factory programs and your own */
            int cur = proc.getCurrentProgram(), n = proc.getNumPrograms();
            bool user = cur >= NFACTORY;
            if (d > 0 && !user && n > NFACTORY)
                proc.setCurrentProgram(NFACTORY);
            else if (d < 0 && user)
                proc.setCurrentProgram(0);
            shownParam = -1;
            page = P_HOME;
        });
        bk->setBounds(Rectangle<float>(KD, KD).withCentre({1050.0f, 150.0f}).toNearestInt());
    }
    /* Global, Write, Compare; Split A|B, Stack A+B, Edit Layer B */
    button(1104.0f, 96.0f, [this] { openPage(page == P_GLOBAL ? P_HOME : P_GLOBAL); }, [this] { return page == P_GLOBAL; });
    button(1104.0f, 131.0f, [this] { writeDialog(); }, [] { return false; });
    button(1104.0f, 166.0f, [this] { proc.setCompare(!proc.compare); }, [this] { return proc.compare; });
    button(1148.0f, 96.0f, [this] { proc.setProgValue(G_LAYER_MODE, proc.progValue(G_LAYER_MODE) == LM_SPLIT ? LM_SINGLE : LM_SPLIT); },
           [this] { return proc.progValue(G_LAYER_MODE) == LM_SPLIT; });
    button(1148.0f, 131.0f, [this] { proc.setProgValue(G_LAYER_MODE, proc.progValue(G_LAYER_MODE) == LM_STACK ? LM_SINGLE : LM_STACK); },
           [this] { return proc.progValue(G_LAYER_MODE) == LM_STACK; });
    button(1148.0f, 166.0f, [this] { proc.editLayer = proc.editLayer.load() ? 0 : 1; }, [this] { return proc.editLayer.load() != 0; });

    /* EFFECT */
    button(1215.0f, 100.0f, toggle(L_FX_ON), isOn(L_FX_ON));
    button(1215.0f, 158.0f, toggle(L_FX_CLK), isOn(L_FX_CLK));
    knob(1270.0f, 90.0f, L_FX_TYPE);
    knob(1327.0f, 90.0f, L_FX_MIX);
    knob(1270.0f, 150.0f, L_FX_P1);
    knob(1327.0f, 150.0f, L_FX_P2);
    /* AUXILIARY ENVELOPE */
    {
        const int top[4] = {L_E3_DEST, L_E3_AMT, L_E3_VEL, L_E3DEL}, bot[4] = {L_E3ATT, L_E3DEC, L_E3SUS, L_E3REL};
        for (int i = 0; i < 4; i++) {
            knob(1392.0f + 62.0f * (float)i, 90.0f, top[i]);
            knob(1392.0f + 62.0f * (float)i, 150.0f, bot[i]);
        }
    }
    /* VOICES and HOST SYNC (REEVES' own), the voices' LEDs */
    {
        auto *vb = add<Btn>(
            [this, glob] {
                auto *q = glob("voices");
                q->beginChangeGesture();
                q->setValueNotifyingHost(q->getValue() > 0.5f ? 0.0f : 1.0f);
                q->endChangeGesture();
                touchGlobal(q);
            },
            [] { return false; });
        vb->setBounds(Rectangle<float>(BW, BH).withCentre({1648.0f, 92.0f}).toNearestInt());
        auto *vl = add<Leds>(2, [this](int i) { return (proc.apvts.getRawParameterValue("voices")->load() > 0.5f) == (i == 1) ? 1.0f : 0.0f; },
                             [this, glob](int i) {
                                 auto *q = glob("voices");
                                 q->beginChangeGesture();
                                 q->setValueNotifyingHost(i ? 1.0f : 0.0f);
                                 q->endChangeGesture();
                             });
        vl->setBounds(1670, 79, 12, 26);
        auto *sb = add<Btn>(
            [this, glob] {
                auto *q = glob("host_sync");
                q->beginChangeGesture();
                q->setValueNotifyingHost(q->getValue() > 0.5f ? 0.0f : 1.0f);
                q->endChangeGesture();
                touchGlobal(q);
            },
            [this] { return proc.apvts.getRawParameterValue("host_sync")->load() > 0.5f; });
        sb->setBounds(Rectangle<float>(BW, BH).withCentre({1648.0f, 134.0f}).toNearestInt());
        auto *act = add<Leds>(16, [this](int i) {
            int n = proc.apvts.getRawParameterValue("voices")->load() > 0.5f ? 16 : 8;
            if (i >= n)
                return 0.0f;
            return std::sqrt(proc.voiceLevel[i].load());
        },
                              std::function<void(int)>(), true);
        act->setBounds(1630, 174, 156, 10);
    }

    /* LOW FREQUENCY OSCILLATORS */
    {
        for (int k = 0; k < 4; k++)
            button(396.0f, 216.0f + 26.0f * (float)k, [this, k] { lfoSel = k; },
                   [this, k] { return lfoSel == k; });
        auto *al = add<Leds>(4, [this](int k) {
            int l = layer();
            if (proc.value(l, L_LFO1 + k * LFO_N + LFO_AMT) == 0)
                return 0.0f;
            return std::fabs(proc.lfoNow[l][k].load());
        });
        al->setBounds(413, 203, 12, 104);
        button(446.0f, 232.0f,
               [this] {
                   int l = layer(), p = L_LFO1 + lfoSel * LFO_N + LFO_SHAPE;
                   proc.setValue(l, p, (proc.value(l, p) + 1) % NLFOSHAPE);
                   touch(l, p);
               },
               [] { return false; });
        auto *sl = add<Leds>(NLFOSHAPE, [this](int i) { return proc.value(layer(), L_LFO1 + lfoSel * LFO_N + LFO_SHAPE) == i ? 1.0f : 0.0f; },
                             [this](int i) { proc.setValue(layer(), L_LFO1 + lfoSel * LFO_N + LFO_SHAPE, i); });
        sl->setBounds(461, 205, 12, 54);
        lknob(548.0f, 228.0f, LFO_FREQ);
        lknob(476.0f, 287.0f, LFO_DEST);
        lknob(548.0f, 287.0f, LFO_AMT);
        auto lt = [this](int off) {
            return [this, off] {
                int l = layer(), p = L_LFO1 + lfoSel * LFO_N + off;
                proc.setValue(l, p, proc.value(l, p) ? 0 : 1);
                touch(l, p);
            };
        };
        auto lo = [this](int off) { return [this, off] { return proc.value(layer(), L_LFO1 + lfoSel * LFO_N + off) != 0; }; };
        button(590.0f, 222.0f, lt(LFO_KEY), lo(LFO_KEY));
        button(590.0f, 282.0f, lt(LFO_CLK), lo(LFO_CLK));
    }
    /* OSCILLATORS */
    {
        auto shapeBtn = [&](float cx, float cy, int p) {
            button(cx, cy,
                   [this, p] {
                       int l = layer();
                       proc.setValue(l, p, (proc.value(l, p) + 1) % NSHAPE);
                       touch(l, p);
                   },
                   [] { return false; });
            auto *sl = add<Leds>(4, [this, p](int i) { return proc.value(layer(), p) == i + 1 ? 1.0f : 0.0f; },
                                 [this, p](int i) {
                                     int l = layer();
                                     proc.setValue(l, p, proc.value(l, p) == i + 1 ? 0 : i + 1);
                                     touch(l, p);
                                 });
            sl->setBounds((int)cx + 15, (int)cy - 21, 12, 42);
        };
        knob(642.0f, 228.0f, L_O1_FREQ);
        knob(702.0f, 228.0f, L_O1_FINE);
        shapeBtn(746.0f, 228.0f, L_O1_SHAPE);
        knob(838.0f, 228.0f, L_O1_SHMOD);
        knob(894.0f, 228.0f, L_SUB);
        knob(950.0f, 228.0f, L_NOISE);
        button(672.0f, 268.0f, toggle(L_SYNC), isOn(L_SYNC));
        knob(642.0f, 288.0f, L_O2_FREQ);
        knob(702.0f, 288.0f, L_O2_FINE);
        shapeBtn(746.0f, 288.0f, L_O2_SHAPE);
        knob(838.0f, 288.0f, L_O2_SHMOD);
        knob(894.0f, 288.0f, L_OSC_MIX);
        knob(950.0f, 288.0f, L_SLOP);
    }
    /* LOW-PASS FILTER */
    {
        const int top[6] = {L_CUTOFF, L_RESO, L_FENV_AMT, L_FVEL, L_KEYAMT, L_AUDIOMOD};
        for (int i = 0; i < 6; i++)
            knob(1012.0f + 62.5f * (float)i, 228.0f, top[i]);
        button(1012.0f, 290.0f, toggle(L_POLE4), isOn(L_POLE4));
        const int bot[5] = {L_FDEL, L_FATT, L_FDEC, L_FSUS, L_FREL};
        for (int i = 0; i < 5; i++)
            knob(1074.5f + 62.5f * (float)i, 288.0f, bot[i]);
    }
    /* AMPLIFIER */
    {
        const int top[4] = {L_PAN_SPREAD, L_AENV_AMT, L_AVEL, L_ADEL}, bot[4] = {L_AATT, L_ADEC, L_ASUS, L_AREL};
        for (int i = 0; i < 4; i++) {
            knob(1392.0f + 62.0f * (float)i, 228.0f, top[i]);
            knob(1392.0f + 62.0f * (float)i, 288.0f, bot[i]);
        }
    }
    /* the keyboard: five octaves, C1 .. C6 */
    {
        auto *k = add<Keys>(proc.keyboardState);
        k->setAvailableRange(36, 96);
        k->setBounds(50, 350, W - 100, H - 356);
        k->setKeyWidth((float)(W - 100) / 36.0f);
        k->setOctaveForMiddleC(4);
    }
}

/* ------------------------------------------------------------------------------ painting --- */
void Panel::paintBackground(juce::Graphics &g)
{
    using namespace juce;
    auto all = Rectangle<float>(0.0f, 0.0f, (float)W, (float)H);
    auto face = all.reduced(48.0f, 0.0f).withBottom(345.0f);
    drawPanelFace(g, face, 12000);
    g.setColour(Colour(0xff0a0a0b));
    g.fillRect(all.reduced(48.0f, 0.0f).withTop(345.0f));
    g.setColour(Colours::black.withAlpha(0.8f));
    g.fillRect(Rectangle<float>(48.0f, 343.0f, (float)W - 96.0f, 5.0f));
    drawWood(g, all.withWidth(48.0f), 3);
    drawWood(g, all.withLeft((float)W - 48.0f), 4);
    for (float x : {48.0f, (float)W - 48.0f}) {
        bool left = x < 100.0f;
        g.setGradientFill(ColourGradient(Colours::black.withAlpha(0.55f), x, 0.0f, Colours::transparentBlack, x + (left ? 12.0f : -12.0f), 0.0f, false));
        g.fillRect(left ? Rectangle<float>(x, 0.0f, 12.0f, (float)H) : Rectangle<float>(x - 12.0f, 0.0f, 12.0f, (float)H));
    }

    /* the jacks' names along the top, as printed on the Rev2 */
    {
        g.setColour(col::legend.withAlpha(0.85f));
        g.setFont(font(Face::label, 9.5f));
        const std::pair<float, const char *> J[] = {{405, "Phones"}, {443, "Left"}, {478, "Right"}, {518, "Left"}, {553, "Right"},
                                                     {795, "Pedal/CV"}, {840, "Sustain"}, {890, "Sequencer"}, {945, "MIDI Thru"},
                                                     {1000, "MIDI Out"}, {1050, "MIDI In"}, {1092, "USB"}, {1700, "AC In"}, {1760, "On/Off"}};
        for (auto &j : J)
            g.drawText(j.second, Rectangle<float>(j.first - 30.0f, 22.0f, 60.0f, 11.0f), Justification::centred, false);
        g.setFont(font(Face::label, 8.0f));
        g.drawText("MAIN/A OUTPUT", Rectangle<float>(415.0f, 33.0f, 90.0f, 10.0f), Justification::centred, false);
        g.drawText("B OUTPUT", Rectangle<float>(500.0f, 33.0f, 70.0f, 10.0f), Justification::centred, false);
    }

    /* the name: REEVES, in the Rev2's wide face; under it the voices, outlined (paint()) */
    {
        auto f = font(Face::logo, 44.0f);
        GlyphArrangement ga;
        ga.addLineOfText(f, "REEVES", 0.0f, 0.0f);
        Path p;
        ga.createPath(p);
        auto b = p.getBounds();
        p.applyTransform(AffineTransform::translation(-b.getX(), -b.getY()).scaled(255.0f / b.getWidth(), 34.0f / b.getHeight()).translated(62.0f, 30.0f));
        g.setColour(col::legend);
        g.fillPath(p);
    }
    drawLegend(g, "MASTER VOLUME", Rectangle<float>(268.0f, 116.0f, 100.0f, 14.0f), 10.5f);
    drawLegend(g, "RATE/TIME", Rectangle<float>(242.0f, 198.0f, 80.0f, 13.0f), 10.5f);
    drawLegend(g, "GLIDE", Rectangle<float>(315.0f, 179.0f, 60.0f, 13.0f), 10.5f);
    drawLegend(g, "UNISON", Rectangle<float>(252.0f, 243.0f, 60.0f, 13.0f), 10.5f);
    drawLegend(g, "HOLD", Rectangle<float>(315.0f, 243.0f, 60.0f, 13.0f), 10.5f);
    {
        g.setFont(font(Face::label, 8.5f));
        g.setColour(col::legend);
        const char *T[5] = {"-2", "-1", "0", "+1", "+2"};
        for (int i = 0; i < 5; i++)
            g.drawText(T[i], Rectangle<float>(272.0f + 16.8f * (float)i, 265.0f, 16.8f, 10.0f), Justification::centred, false);
        drawLegend(g, "Down", Rectangle<float>(262.0f, 309.0f, 40.0f, 11.0f), 9.0f);
        drawLegend(g, "Up", Rectangle<float>(325.0f, 309.0f, 40.0f, 11.0f), 9.0f);
        drawLegend(g, "TRANSPOSE", Rectangle<float>(273.0f, 320.0f, 80.0f, 12.0f), 10.0f);
    }
    drawLegend(g, "PITCH", Rectangle<float>(71.0f, 308.0f, 60.0f, 13.0f), 10.5f);
    drawLegend(g, "MOD", Rectangle<float>(137.0f, 308.0f, 60.0f, 13.0f), 10.5f);
    for (auto x : {84.0f, 150.0f}) {
        g.setColour(Colours::black);
        g.fillRoundedRectangle(Rectangle<float>(x - 4.0f, 164.0f, 42.0f, 140.0f), 4.0f);
    }

    /* the sections */
    struct Sec {
        float x0, x1, y0, y1;
        const char *t;
    };
    const Sec S[] = {{386, 496, 63, 190, "MODULATION"},
                     {503, 616, 63, 190, "CLOCK"},
                     {623, 681, 63, 190, "ARP"},
                     {690, 862, 63, 190, "SEQUENCER"},
                     {1188, 1350, 63, 190, "EFFECT"},
                     {1362, 1606, 63, 190, "AUXILIARY ENVELOPE"},
                     {372, 608, 197, 322, "LOW FREQUENCY OSCILLATORS"},
                     {616, 978, 197, 322, "OSCILLATORS"},
                     {984, 1352, 197, 322, "LOW-PASS FILTER"},
                     {1362, 1606, 197, 322, "AMPLIFIER"}};
    for (auto &s : S)
        drawSection(g, Rectangle<float>(s.x0, s.y0, s.x1 - s.x0, s.y1 - s.y0), s.t);

    auto under = [&](float cx, float cy, const char *t, float size = 10.5f) {
        drawLegend(g, t, Rectangle<float>(cx - 40.0f, cy + 22.0f, 80.0f, 13.0f), size);
    };
    auto underB = [&](float cx, float cy, const char *t, float size = 10.0f) {
        drawLegend(g, t, Rectangle<float>(cx - 38.0f, cy + 9.0f, 76.0f, 13.0f), size);
    };
    under(467, 90, "SELECT");
    under(467, 150, "AMOUNT");
    underB(413, 100, "SOURCE");
    underB(413, 158, "DESTINATION", 9.0f);
    under(522, 90, "BPM");
    under(588, 90, "DIVIDE");
    underB(558, 158, "TAP TEMPO");
    underB(652, 100, "MODE");
    underB(652, 158, "ON/OFF");
    underB(712, 100, "TYPE");
    {
        g.setFont(font(Face::label, 9.0f));
        g.setColour(col::legend);
        g.drawText("Gated", Rectangle<float>(744.0f, 87.0f, 40.0f, 12.0f), Justification::centredLeft, false);
        g.drawText("Poly", Rectangle<float>(744.0f, 100.0f, 40.0f, 12.0f), Justification::centredLeft, false);
        g.drawRoundedRectangle(Rectangle<float>(730.0f, 100.0f, 40.0f, 13.0f), 5.0f, 1.0f);
        g.drawArrow(Line<float>(745.0f, 114.0f, 745.0f, 146.0f), 1.0f, 5.0f, 5.0f);
        g.setFont(font(Face::label, 8.5f));
        g.drawText("rest", Rectangle<float>(770.0f, 81.0f, 36.0f, 10.0f), Justification::centred, false);
        g.drawText("tie", Rectangle<float>(770.0f, 139.0f, 36.0f, 10.0f), Justification::centred, false);
        g.drawRoundedRectangle(Rectangle<float>(724.0f, 168.0f, 32.0f, 13.0f), 5.0f, 1.0f);
        drawLegend(g, "PLAY", Rectangle<float>(724.0f, 168.0f, 32.0f, 13.0f), 9.5f);
        g.drawText("show", Rectangle<float>(954.0f, 156.0f, 36.0f, 10.0f), Justification::centred, false);
    }
    underB(788, 100, "MODE");
    under(836, 90, "TRACK");
    underB(788, 158, "DESTINATION", 9.0f);
    underB(836, 158, "RECORD");
    under(893, 90, "PARAMETER");
    under(893, 150, "VALUE");
    underB(972, 172, "MISC PARAMS", 9.5f);
    under(1050, 90, "PROGRAM");
    under(1050, 150, "BANK");
    underB(1104, 96, "Global", 9.5f);
    underB(1104, 131, "Write", 9.5f);
    underB(1104, 166, "Compare", 9.5f);
    underB(1148, 96, "Split A|B", 9.5f);
    underB(1148, 131, "Stack A+B", 9.5f);
    underB(1148, 166, "Edit Layer B", 9.0f);
    underB(1215, 100, "ON/OFF");
    underB(1215, 158, "CLK SYNC");
    under(1270, 90, "SELECT");
    under(1327, 90, "MIX");
    under(1270, 150, "PARAM 1");
    under(1327, 150, "PARAM 2");
    {
        const char *top[4] = {"DESTINATION", "ENV AMOUNT", "VELOCITY", "DELAY"}, *bot[4] = {"ATTACK", "DECAY", "SUSTAIN", "RELEASE"};
        for (int i = 0; i < 4; i++) {
            under(1392.0f + 62.0f * (float)i, 90, top[i], 10.0f);
            under(1392.0f + 62.0f * (float)i, 150, bot[i]);
            under(1392.0f + 62.0f * (float)i, 228, i == 0 ? "PAN SPREAD" : top[i], 10.0f);
            under(1392.0f + 62.0f * (float)i, 288, bot[i]);
        }
    }
    /* VOICES, HOST SYNC */
    drawSection(g, Rectangle<float>(1618.0f, 63.0f, 178.0f, 127.0f), "VOICES");
    {
        g.setFont(font(Face::label, 9.5f));
        g.setColour(col::legend);
        g.drawText("8 Voice", Rectangle<float>(1686.0f, 79.0f, 60.0f, 13.0f), Justification::centredLeft, false);
        g.drawText("16 Voice", Rectangle<float>(1686.0f, 92.0f, 60.0f, 13.0f), Justification::centredLeft, false);
        g.setFont(font(Face::title, 9.5f));
        g.drawText("HOST SYNC", Rectangle<float>(1668.0f, 128.0f, 80.0f, 12.0f), Justification::centredLeft, false);
        g.setFont(font(Face::label, 8.5f));
        g.drawText("the DAW's tempo and transport", Rectangle<float>(1632.0f, 146.0f, 160.0f, 11.0f), Justification::centredLeft, false);
        g.drawText("voices playing", Rectangle<float>(1632.0f, 159.0f, 160.0f, 11.0f), Justification::centredLeft, false);
    }
    /* LFOs */
    {
        g.setFont(font(Face::title, 10.5f));
        g.setColour(col::legend);
        for (int k = 0; k < 4; k++)
            g.drawText(String(k + 1), Rectangle<float>(376.0f, 210.0f + 26.0f * (float)k, 8.0f, 12.0f), Justification::centred, false);
        underB(446, 232, "SHAPE", 9.5f);
        g.setFont(font(Face::label, 8.5f));
        for (int i = 0; i < NLFOSHAPE; i++)
            g.drawText(LFOSHAPE_NAME[i], Rectangle<float>(474.0f, 205.0f + 10.8f * (float)i, 46.0f, 10.8f), Justification::centredLeft, false);
        under(548, 228, "FREQUENCY", 9.5f);
        under(476, 287, "DESTINATION", 9.5f);
        under(548, 287, "AMOUNT", 10.0f);
        underB(590, 222, "KEY SYNC", 8.5f);
        underB(590, 282, "CLK SYNC", 8.5f);
    }
    /* OSCILLATORS */
    {
        under(642, 228, "OSC 1 FREQ", 9.5f);
        under(702, 228, "FINE TUNE", 9.5f);
        underB(746, 228, "SHAPE", 9.5f);
        under(838, 228, "SHAPE MOD", 9.5f);
        under(894, 228, "SUB OCTAVE", 9.5f);
        under(950, 228, "NOISE", 10.0f);
        g.setFont(font(Face::title, 9.5f));
        g.setColour(col::legend);
        g.drawText("SYNC", Rectangle<float>(687.0f, 262.0f, 34.0f, 12.0f), Justification::centredLeft, false);
        under(642, 288, "OSC 2 FREQ", 9.5f);
        under(702, 288, "FINE TUNE", 9.5f);
        underB(746, 288, "SHAPE", 9.5f);
        under(838, 288, "SHAPE MOD", 9.5f);
        under(894, 288, "OSC MIX", 9.5f);
        under(950, 288, "OSC SLOP", 9.5f);
        g.setFont(font(Face::title, 9.0f));
        g.drawText("1", Rectangle<float>(871.0f, 298.0f, 10.0f, 10.0f), Justification::centred, false);
        g.drawText("2", Rectangle<float>(907.0f, 298.0f, 10.0f, 10.0f), Justification::centred, false);
        g.setFont(font(Face::label, 8.5f));
        const char *SN[4] = {"Sawtooth", "Saw+Tri", "Triangle", "Square"};
        for (int r = 0; r < 2; r++)
            for (int i = 0; i < 4; i++)
                g.drawText(SN[i], Rectangle<float>(775.0f, 207.0f + 60.0f * (float)r + 10.5f * (float)i, 44.0f, 10.5f), Justification::centredLeft, false);
    }
    /* LOW-PASS FILTER */
    {
        const char *top[6] = {"CUTOFF", "RESONANCE", "ENV AMOUNT", "VELOCITY", "KEY AMOUNT", "AUDIO MOD"};
        for (int i = 0; i < 6; i++)
            under(1012.0f + 62.5f * (float)i, 228, top[i], 10.0f);
        underB(1012, 290, "4 POLE");
        const char *bot[5] = {"DELAY", "ATTACK", "DECAY", "SUSTAIN", "RELEASE"};
        for (int i = 0; i < 5; i++)
            under(1074.5f + 62.5f * (float)i, 288, bot[i]);
    }
    /* the maker's tag, where the badge is */
    drawTag(g, Rectangle<float>(1628.0f, 228.0f, 160.0f, 66.0f));
}

void Panel::paint(juce::Graphics &g)
{
    float sc = g.getInternalContext().getPhysicalPixelScaleFactor();
    if (sc != backgroundScale || background.isNull()) {
        backgroundScale = sc;
        background = juce::Image(juce::Image::RGB, juce::jmax(1, (int)std::ceil(W * sc)), juce::jmax(1, (int)std::ceil(H * sc)), true);
        juce::Graphics bg(background);
        bg.addTransform(juce::AffineTransform::scale(sc));
        paintBackground(bg);
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
        float w = sixteen ? 62.0f : 30.0f;
        p.applyTransform(AffineTransform::translation(-b.getX(), -b.getY()).scaled(w / b.getWidth(), 26.0f / b.getHeight()).translated(64.0f, 74.0f));
        g.setColour(col::legend);
        g.strokePath(p, PathStrokeType(1.6f));
        g.setFont(font(Face::title, 15.0f));
        g.drawText("VOICE", Rectangle<float>(72.0f + w, 80.0f, 80.0f, 20.0f), Justification::centredLeft, false);
    }
}

void Panel::timerCallback()
{
    for (auto *l : live)
        l->poll();
    int sixteen = proc.apvts.getRawParameterValue("voices")->load() > 0.5f ? 16 : 8;
    if (sixteen != shownVoices) {
        shownVoices = sixteen;
        repaint(juce::Rectangle<int>(60, 70, 200, 34));
    }
}

/* ============================================================================== the editor === */
ReevesEditor::ReevesEditor(ReevesProcessor &p) : AudioProcessorEditor(&p), proc(p), panel(p)
{
    addAndMakeVisible(panel);
    panel.setBounds(0, 0, ReevesPanel::W, ReevesPanel::H);
    const double W = ReevesPanel::W, H = ReevesPanel::H;
    setResizable(true, true);
    setResizeLimits((int)(W * 0.5), (int)(H * 0.5), (int)(W * 1.6), (int)(H * 1.6));
    getConstrainer()->setFixedAspectRatio(W / H);
    float s = juce::jlimit(0.5f, 1.6f, proc.uiScale.load());
    setSize((int)std::lround(W * s), (int)std::lround(H * s));
}

void ReevesEditor::resized()
{
    float s = (float)getWidth() / (float)ReevesPanel::W;
    panel.setTransform(juce::AffineTransform::scale(s));
    proc.uiScale = s;
}
