/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (REEVES) */
#include "Look.h"
#include "BinaryData.h"

namespace rvui
{
/* the fonts and the tag: plain pointers, made when first drawn and freed when the last editor closes. Never function
 * statics: those are freed while the host unloads the DLL, under Windows' loader lock (PROPHET 67's lesson) */
struct Store
{
    juce::Typeface::Ptr t[4];
    juce::Image tag;
};
static Store *store = nullptr;
static Store &get()
{
    if (store == nullptr)
    {
        store = new Store();
        store->t[0] = juce::Typeface::createSystemTypefaceFor(BinaryData::FiraSansSemiBold_ttf, BinaryData::FiraSansSemiBold_ttfSize);
        store->t[1] = juce::Typeface::createSystemTypefaceFor(BinaryData::FiraSansBold_ttf, BinaryData::FiraSansBold_ttfSize);
        store->t[2] = juce::Typeface::createSystemTypefaceFor(BinaryData::FiraSansExtraBold_ttf, BinaryData::FiraSansExtraBold_ttfSize);
        store->t[3] = juce::Typeface::createSystemTypefaceFor(BinaryData::AnonymousProBold_ttf, BinaryData::AnonymousProBold_ttfSize);
        store->tag = juce::ImageFileFormat::loadFrom(BinaryData::swtdr34mztag_png, BinaryData::swtdr34mztag_pngSize);
    }
    return *store;
}
UiResources::~UiResources()
{
    delete store;
    store = nullptr;
}

juce::Font font(Face f, float height) { return juce::Font(juce::FontOptions(get().t[(int)f]).withHeight(height)); }

float textWidth(const juce::Font &f, const juce::String &t)
{
    juce::GlyphArrangement ga;
    ga.addLineOfText(f, t, 0.0f, 0.0f);
    return ga.getBoundingBox(0, -1, true).getWidth();
}

Look::Look()
{
    using namespace juce;
    setColour(ResizableWindow::backgroundColourId, col::panelBottom);
    setColour(PopupMenu::backgroundColourId, Colour(0xff1d1e21));
    setColour(PopupMenu::textColourId, col::legend);
    setColour(PopupMenu::highlightedBackgroundColourId, Colour(0xffb3241a));
    setColour(PopupMenu::highlightedTextColourId, Colours::white);
    setColour(PopupMenu::headerTextColourId, col::dim);
    setColour(AlertWindow::backgroundColourId, Colour(0xff1d1e21));
    setColour(AlertWindow::textColourId, col::legend);
    setColour(TextEditor::backgroundColourId, Colour(0xff0b0b0c));
    setColour(TextEditor::textColourId, col::legend);
    setColour(TextButton::buttonColourId, Colour(0xff2a2b2e));
}

juce::Typeface::Ptr Look::getTypefaceForFont(const juce::Font &) { return get().t[0]; }

void Look::drawPopupMenuBackground(juce::Graphics &g, int w, int h)
{
    g.fillAll(findColour(juce::PopupMenu::backgroundColourId));
    g.setColour(col::frame.withAlpha(0.6f));
    g.drawRect(0, 0, w, h, 1);
}

/* ------------------------------------------------------------------------------- the knob --- */
/* PROPHET 67's knob, seen from above: white scale ticks around it, the black ribbed body, a chrome ring, a dark
 * spun-metal cap and its white line */
void drawKnob(juce::Graphics &g, juce::Rectangle<float> r, float pos, bool bipolar, bool endless)
{
    using namespace juce;
    const float d = jmin(r.getWidth(), r.getHeight()), R = d * 0.5f;
    const Point<float> c = r.getCentre();
    const float a0 = -2.4f, a1 = 2.4f;
    const float ang = endless ? pos * MathConstants<float>::twoPi : a0 + pos * (a1 - a0);
    const float rb = R * 0.64f;

    if (!endless) {                      /* the scale */
        g.setColour(col::legend);
        for (int i = 0; i <= 10; i++) {
            float a = a0 + (a1 - a0) * (float)i / 10.0f;
            g.drawLine(Line<float>(c.getPointOnCircumference(R * 0.80f, a), c.getPointOnCircumference(R * (i == 5 && bipolar ? 0.99f : 0.93f), a)),
                       jmax(1.0f, d * 0.03f));
        }
        if (bipolar) {
            g.setFont(font(Face::title, jmax(8.0f, d * 0.2f)));
            auto pm = c.getPointOnCircumference(R * 0.9f, a0 - 0.42f), pp = c.getPointOnCircumference(R * 0.9f, a1 + 0.42f);
            g.drawText("-", Rectangle<float>(12.0f, 12.0f).withCentre(pm), Justification::centred, false);
            g.drawText("+", Rectangle<float>(12.0f, 12.0f).withCentre(pp), Justification::centred, false);
        }
    }
    g.setGradientFill(ColourGradient(Colours::black.withAlpha(0.75f), c.x, c.y + rb * 0.3f, Colours::transparentBlack, c.x, c.y + rb * 1.5f, true));
    g.fillEllipse(Rectangle<float>(rb * 2.7f, rb * 2.7f).withCentre(c.translated(rb * 0.08f, rb * 0.22f)));
    auto body = Rectangle<float>(rb * 2.0f, rb * 2.0f).withCentre(c);
    g.setColour(Colour(0xff070708));
    g.fillEllipse(body);
    const int ribs = 36;
    for (int i = 0; i < ribs; i++) {
        float a = MathConstants<float>::twoPi * (float)i / (float)ribs;
        float lit = 0.5f + 0.5f * std::cos(a + 0.9f);
        g.setColour(Colours::white.withAlpha(0.04f + 0.10f * lit));
        g.drawLine(Line<float>(c.getPointOnCircumference(rb * 0.90f, a), c.getPointOnCircumference(rb * 0.995f, a)), jmax(0.8f, d * 0.018f));
    }
    const float ro = rb * 0.89f, ri = rb * 0.70f;
    const int slices = 48;
    for (int i = 0; i < slices; i++) {
        float a = MathConstants<float>::twoPi * (float)i / (float)slices, a2 = a + MathConstants<float>::twoPi / (float)slices + 0.01f;
        float k = 0.5f + 0.5f * std::cos(2.0f * (a + 0.75f));
        float v = 0.38f + 0.55f * std::pow(k, 1.6f);
        Path p;
        p.addCentredArc(c.x, c.y, ro, ro, 0.0f, a, a2, true);
        p.lineTo(c.getPointOnCircumference(ri, a2));
        p.addCentredArc(c.x, c.y, ri, ri, 0.0f, a2, a, false);
        p.closeSubPath();
        g.setColour(Colour::fromFloatRGBA(v, v, v * 1.02f, 1.0f));
        g.fillPath(p);
    }
    g.setColour(Colours::black.withAlpha(0.6f));
    g.drawEllipse(Rectangle<float>(ro * 2.0f, ro * 2.0f).withCentre(c), 0.8f);
    g.setGradientFill(ColourGradient(Colour(0xff2a2a2d), c.x - ri * 0.5f, c.y - ri * 0.6f, Colour(0xff0d0d0f), c.x + ri * 0.6f, c.y + ri * 0.7f, false));
    g.fillEllipse(Rectangle<float>(ri * 2.0f, ri * 2.0f).withCentre(c));
    g.setColour(Colours::white.withAlpha(0.06f));
    g.drawEllipse(Rectangle<float>(ri * 1.5f, ri * 1.5f).withCentre(c), 0.6f);
    g.setColour(Colours::black.withAlpha(0.8f));
    g.drawEllipse(Rectangle<float>(ri * 2.0f, ri * 2.0f).withCentre(c), 0.8f);
    auto p0 = c.getPointOnCircumference(ri * 0.15f, ang), p1 = c.getPointOnCircumference(ri * 0.96f, ang);
    g.setColour(Colour(0xfff4f2ec));
    g.drawLine({p0, p1}, jmax(1.5f, d * 0.045f));
}

/* ---------------------------------------------------------------------------- the button --- */
void drawButton(juce::Graphics &g, juce::Rectangle<float> r, bool on, bool down, bool over)
{
    using namespace juce;
    if (on) {
        g.setGradientFill(ColourGradient(Colour(0xffff2a10).withAlpha(0.5f), r.getCentreX(), r.getCentreY(), Colours::transparentBlack,
                                         r.getRight() + r.getWidth() * 0.45f, r.getCentreY(), true));
        g.fillEllipse(r.expanded(r.getWidth() * 0.5f, r.getHeight() * 1.1f));
    }
    g.setColour(Colours::black.withAlpha(0.75f));
    g.fillRoundedRectangle(r.translated(0.6f, 1.8f).expanded(0.6f), 2.5f);
    g.setColour(Colour(0xff020202));
    g.fillRoundedRectangle(r, 2.5f);
    auto face = r.reduced(1.8f).translated(0.0f, down ? 0.8f : 0.0f);
    if (on) {
        ColourGradient lit(Colour(0xffffa070), face.getCentreX(), face.getCentreY(), Colour(0xffd81c0a), face.getRight(), face.getBottom(), true);
        lit.addColour(0.5, Colour(0xffff4a1c));
        g.setGradientFill(lit);
    } else
        g.setGradientFill(ColourGradient(Colour(0xff2c2c30), face.getX(), face.getY(), Colour(0xff151517), face.getX(), face.getBottom(), false));
    g.fillRoundedRectangle(face, 1.8f);
    /* the fine grid of a Rev2 button's face */
    g.setColour(on ? Colour(0x40a00a00) : Colour(0x30000000));
    for (float x = face.getX() + 2.0f; x < face.getRight() - 1.0f; x += 2.6f)
        g.drawVerticalLine((int)x, face.getY() + 1.5f, face.getBottom() - 1.5f);
    for (float y = face.getY() + 2.0f; y < face.getBottom() - 1.0f; y += 2.6f)
        g.drawHorizontalLine((int)y, face.getX() + 1.5f, face.getRight() - 1.5f);
    g.setColour(Colours::white.withAlpha(on ? 0.3f : 0.09f));
    g.drawLine(face.getX() + 2.0f, face.getY() + 0.7f, face.getRight() - 2.0f, face.getY() + 0.7f, 1.0f);
    if (over && !down) {
        g.setColour(Colours::white.withAlpha(0.05f));
        g.fillRoundedRectangle(face, 1.8f);
    }
}

void drawLed(juce::Graphics &g, juce::Point<float> c, float s, float level)
{
    using namespace juce;
    level = jlimit(0.0f, 1.0f, level);
    auto body = Rectangle<float>(s * 1.5f, s).withCentre(c);
    if (level > 0.02f) {
        g.setGradientFill(ColourGradient(Colour(0xffff3a1a).withAlpha(0.6f * level), c.x, c.y, Colours::transparentBlack, c.x + s * 2.2f, c.y, true));
        g.fillEllipse(Rectangle<float>(s * 4.4f, s * 3.6f).withCentre(c));
    }
    g.setColour(Colours::black.withAlpha(0.9f));
    g.fillRect(body.expanded(1.0f));
    g.setGradientFill(ColourGradient(Colour(0xff4a120e).interpolatedWith(Colour(0xffffb070), level), c.x, c.y,
                                     Colour(0xff2a0a08).interpolatedWith(Colour(0xffff2a10), level), body.getRight(), body.getBottom(), true));
    g.fillRect(body);
}

/* ------------------------------------------------------------------- sections and the face --- */
void drawLegend(juce::Graphics &g, const juce::String &t, juce::Rectangle<float> r, float size, juce::Justification j)
{
    g.setColour(col::legend);
    g.setFont(font(Face::title, size).withExtraKerningFactor(0.02f));
    g.drawFittedText(t, r.toNearestInt(), j, 2, 0.75f);
}

void drawSection(juce::Graphics &g, juce::Rectangle<float> r, const juce::String &title)
{
    using namespace juce;
    auto f = font(Face::title, 13.0f).withExtraKerningFactor(0.05f);
    float tw = textWidth(f, title), cx = r.getCentreX(), gl = cx - tw * 0.5f - 6.0f, gr = cx + tw * 0.5f + 6.0f, rad = 9.0f;
    Path p;
    p.startNewSubPath(gr, r.getY());
    p.lineTo(r.getRight() - rad, r.getY());
    p.quadraticTo(r.getRight(), r.getY(), r.getRight(), r.getY() + rad);
    p.lineTo(r.getRight(), r.getBottom() - rad);
    p.quadraticTo(r.getRight(), r.getBottom(), r.getRight() - rad, r.getBottom());
    p.lineTo(r.getX() + rad, r.getBottom());
    p.quadraticTo(r.getX(), r.getBottom(), r.getX(), r.getBottom() - rad);
    p.lineTo(r.getX(), r.getY() + rad);
    p.quadraticTo(r.getX(), r.getY(), r.getX() + rad, r.getY());
    if (title.isNotEmpty())
        p.lineTo(gl, r.getY());
    else
        p.closeSubPath();
    g.setColour(col::frame);
    g.strokePath(p, PathStrokeType(1.6f));
    if (title.isNotEmpty()) {
        g.setColour(col::legend);
        g.setFont(f);
        g.drawText(title, Rectangle<float>(gl, r.getY() - 9.0f, gr - gl, 18.0f), Justification::centred, false);
    }
}

void drawPanelFace(juce::Graphics &g, juce::Rectangle<float> r, int dots, int seed)
{
    using namespace juce;
    g.setGradientFill(ColourGradient(col::panelTop, 0.0f, r.getY(), col::panelBottom, 0.0f, r.getBottom(), false));
    g.fillRect(r);
    Random rnd(seed);
    for (int i = 0; i < dots; i++) {
        float x = r.getX() + rnd.nextFloat() * r.getWidth(), y = r.getY() + rnd.nextFloat() * r.getHeight();
        g.setColour((rnd.nextBool() ? Colours::white : Colours::black).withAlpha(0.02f + 0.03f * rnd.nextFloat()));
        g.fillRect(x, y, 1.0f, 1.0f);
    }
}

void drawWood(juce::Graphics &g, juce::Rectangle<float> r, int seed)
{
    using namespace juce;
    ColourGradient base(Colour(0xff4a2412), r.getX(), 0.0f, Colour(0xff3c1d0f), r.getRight(), 0.0f, false);
    base.addColour(0.35, Colour(0xff7a3e1f));
    base.addColour(0.7, Colour(0xff653319));
    g.setGradientFill(base);
    g.fillRect(r);
    Random rnd(seed);
    for (int i = 0; i < 34; i++) {
        float x0 = r.getX() + rnd.nextFloat() * r.getWidth(), amp = 1.0f + rnd.nextFloat() * 3.5f,
              f = 0.004f + rnd.nextFloat() * 0.01f, ph = rnd.nextFloat() * 6.28f;
        Path p;
        p.startNewSubPath(x0, r.getY());
        for (float y = r.getY(); y <= r.getBottom(); y += 12.0f)
            p.lineTo(x0 + amp * std::sin(y * f + ph) + 1.2f * std::sin(y * f * 3.7f + ph * 2.0f), y);
        bool dark = rnd.nextFloat() < 0.7f;
        g.setColour(dark ? Colour(0xff200d05).withAlpha(0.16f + 0.2f * rnd.nextFloat())
                         : Colour(0xffa86a40).withAlpha(0.10f + 0.12f * rnd.nextFloat()));
        g.strokePath(p, PathStrokeType(0.5f + rnd.nextFloat() * 1.6f));
    }
    g.setGradientFill(ColourGradient(Colours::white.withAlpha(0.10f), r.getX(), 0.0f, Colours::transparentWhite, r.getX() + r.getWidth() * 0.3f, 0.0f, false));
    g.fillRect(r);
}

void drawTag(juce::Graphics &g, juce::Rectangle<float> r)
{
    const juce::Image &tag = get().tag;
    if (tag.isNull())
        return;
    auto fit = juce::RectanglePlacement(juce::RectanglePlacement::centred).appliedTo(tag.getBounds().toFloat(), r);
    g.setColour(col::legend);
    g.drawImage(tag, fit, juce::RectanglePlacement::stretchToFit, true);
}
} // namespace rvui
