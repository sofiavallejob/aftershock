// Aftershock, a reverb made from earthquakes. (C) 2026 Sofia Vallejo Budziszewski, AGPL-3.0.
// Free to port, by humans. Bring your own ears. See PORTING.md.

#include "Widgets.h"

namespace ui
{
// ---------------------------------------------------------------- look
Look::Look() { refresh(); }

void Look::refresh()
{
    const auto p = Settings::get().palette();
    setColour (juce::ResizableWindow::backgroundColourId, p.bg);
    setColour (juce::Label::textColourId, p.t1);
    setColour (juce::TextButton::textColourOffId, p.t1);
    setColour (juce::TextButton::textColourOnId, p.onAccent);
    setColour (juce::ToggleButton::textColourId, p.t1);
    setColour (juce::ComboBox::textColourId, p.t1);
    setColour (juce::ComboBox::arrowColourId, p.t3);
    setColour (juce::PopupMenu::backgroundColourId, p.card);
    setColour (juce::PopupMenu::textColourId, p.t1);
    setColour (juce::PopupMenu::headerTextColourId, p.t3);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, p[lime]);
    setColour (juce::PopupMenu::highlightedTextColourId, p.onAccent);
    setColour (juce::TextEditor::textColourId, p.t1);
    setColour (juce::TextEditor::highlightColourId, p[lav].withAlpha (0.4f));
    setColour (juce::TextEditor::highlightedTextColourId, p.t1);
    setColour (juce::CaretComponent::caretColourId, p[lime]);
    setColour (juce::TextEditor::backgroundColourId, p.wellColour());
    setColour (juce::ScrollBar::thumbColourId, p.ink.withAlpha (0.25f));
    setColour (juce::AlertWindow::backgroundColourId, p.card);
    setColour (juce::AlertWindow::textColourId, p.t1);
    setColour (juce::AlertWindow::outlineColourId, p.border());
    setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
    setDefaultSansSerifTypeface (pixelTypeface());
}

void Look::drawFocusOutline (juce::Graphics& g, juce::Component& c)
{
    if (! Settings::get().focusRing() || ! c.hasKeyboardFocus (false)) return;
    g.setColour (Settings::get().palette().focus);
    g.drawRoundedRectangle (c.getLocalBounds().toFloat().reduced (1.0f), 8.0f, 2.0f);
}

void Look::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& s)
{
    const auto p = Settings::get().palette();
    const auto acc = p[(Accent) (int) s.getProperties().getWithDefault ("accent", 0)];
    auto r = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    const float d = juce::jmin (r.getWidth(), r.getHeight());
    auto box = r.withSizeKeepingCentre (d, d);
    const auto c = box.getCentre();
    const float ringW = juce::jmax (5.0f, d * 0.085f), rad = d * 0.5f - ringW * 0.5f;
    const float ang = a0 + pos * (a1 - a0);

    juce::Path track; track.addCentredArc (c.x, c.y, rad, rad, 0, a0, a1, true);
    g.setColour (p.ink.withAlpha (p.highContrast ? 0.35f : 0.10f));
    g.strokePath (track, juce::PathStrokeType (ringW, juce::PathStrokeType::curved, juce::PathStrokeType::butt));
    juce::Path val; val.addCentredArc (c.x, c.y, rad, rad, 0, a0, ang, true);
    g.setColour (s.isEnabled() ? acc : p.t3);
    g.strokePath (val, juce::PathStrokeType (ringW, juce::PathStrokeType::curved, juce::PathStrokeType::butt));

    // cap: tinted by the accent, lit from the top left
    const float capR = d * 0.5f - ringW - d * 0.07f;
    auto cap = juce::Rectangle<float> (capR * 2, capR * 2).withCentre (c);
    juce::DropShadow (juce::Colours::black.withAlpha (p.highContrast ? 0.0f : 0.3f), 8, { 0, 3 }).drawForRectangle (g, cap.toNearestInt());
    juce::ColourGradient cg (acc.withAlpha (0.38f).overlaidWith (p.globe2.withAlpha (0.0f)), cap.getX() + capR * 0.76f, cap.getY() + capR * 0.6f,
                             p.globe2, cap.getRight(), cap.getBottom(), true);
    cg.addColour (0.55, acc.withAlpha (0.12f).overlaidWith (p.globe2.withAlpha (0.0f)));
    g.setColour (p.globe2); g.fillEllipse (cap);
    g.setGradientFill (cg); g.fillEllipse (cap);
    g.setColour (juce::Colours::white.withAlpha (p.highContrast ? 0.5f : 0.10f)); g.drawEllipse (cap.reduced (0.5f), 1.0f);
    const auto dot = c.getPointOnCircumference (capR - 7.0f, ang);
    g.setColour (acc); g.fillEllipse (juce::Rectangle<float> (5, 5).withCentre (dot));
}

void Look::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float, juce::Slider::SliderStyle st, juce::Slider& s)
{
    if (! s.getProperties().contains ("gain")) { LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, pos, 0, 0, st, s); return; }
    const auto p = Settings::get().palette();
    auto r = s.getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (p.wellColour()); g.fillRoundedRectangle (r, 7.0f);
    const float zero = r.getX() + r.getWidth() * (float) s.valueToProportionOfLength (0.0);
    const float at = (float) pos;
    g.setColour (p[rose].withAlpha (0.3f));
    g.fillRect (juce::Rectangle<float>::leftTopRightBottom (juce::jmin (zero, at), r.getY() + 3, juce::jmax (zero, at), r.getBottom() - 3));
    g.setColour (p.ink.withAlpha (p.highContrast ? 0.7f : p.borderAlpha + 0.04f)); g.drawRoundedRectangle (r, 7.0f, 1.0f);
    g.setColour (p.t1); g.setFont (font (12.0f));
    g.drawText ("Gain " + s.getTextFromValue (s.getValue()), r, juce::Justification::centred);
    if (Settings::get().focusRing() && s.hasKeyboardFocus (false)) { g.setColour (p.focus); g.drawRoundedRectangle (r.expanded (1.5f), 9, 2.0f); }
    juce::ignoreUnused (x, y, w, h);
}

void setStyle (juce::Button& b, const juce::String& style, Accent a)
{
    b.getProperties().set ("style", style);
    b.getProperties().set ("accent", (int) a);
    b.setMouseCursor (juce::MouseCursor::PointingHandCursor);
    b.repaint();
}

void Look::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    const auto p = Settings::get().palette();
    const auto style = b.getProperties().getWithDefault ("style", "plain").toString();
    const auto acc = p[(Accent) (int) b.getProperties().getWithDefault ("accent", 0)];
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const float rad = (float) b.getProperties().getWithDefault ("radius", 7.0);

    if (style == "seg")
    {
        if (b.getToggleState()) { g.setColour (acc); g.fillRoundedRectangle (r, 8.0f); }
        else if (over) { g.setColour (p.ink.withAlpha (0.06f)); g.fillRoundedRectangle (r, 8.0f); }
    }
    else if (style == "accent" || (style == "toggle" && b.getToggleState()))
    {
        g.setColour (down ? acc.darker (0.15f) : over ? acc.brighter (0.08f) : acc);
        g.fillRoundedRectangle (r, rad);
    }
    else if (style == "outline")
    {
        g.setColour (p.ink.withAlpha (over ? 0.08f : 0.03f)); g.fillRoundedRectangle (r, rad);
        g.setColour (acc); g.drawRoundedRectangle (r, rad, 1.2f);
    }
    else if (style == "ghost")
    {
        if (over) { g.setColour (p.ink.withAlpha (0.06f)); g.fillRoundedRectangle (r, rad); }
    }
    else
    {
        g.setColour (p.ink.withAlpha (down ? 0.12f : over ? 0.08f : 0.04f)); g.fillRoundedRectangle (r, rad);
        g.setColour (p.ink.withAlpha (p.highContrast ? 0.7f : p.borderAlpha + 0.04f)); g.drawRoundedRectangle (r, rad, 1.0f);
    }
    if (Settings::get().focusRing() && b.hasKeyboardFocus (false))
    {
        g.setColour (p.focus); g.drawRoundedRectangle (r.expanded (1.5f), rad + 2, 2.0f);
    }
}

void Look::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    const auto p = Settings::get().palette();
    const auto style = b.getProperties().getWithDefault ("style", "plain").toString();
    const bool filled = style == "accent" || ((style == "toggle" || style == "seg") && b.getToggleState());
    juce::Colour c = filled ? p.onAccent : p.t1;
    if (style == "outline") c = p.bg.getPerceivedBrightness() > 0.5f ? p.t1 : p[(Accent) (int) b.getProperties().getWithDefault ("accent", 0)];
    if (b.getProperties().contains ("textColour") && ! filled) c = p[(Accent) (int) b.getProperties()["textColour"]];
    if (! b.isEnabled()) c = c.withAlpha (0.4f);
    const auto sub = b.getProperties().getWithDefault ("sub", "").toString();
    const float fs = (float) b.getProperties().getWithDefault ("fontSize", 12.0);
    auto r = b.getLocalBounds().reduced (6, 2);
    if (sub.isNotEmpty())
    {
        auto top = r.removeFromTop (r.getHeight() / 2);
        g.setColour (c); g.setFont (font (fs)); g.drawText (b.getButtonText(), top, juce::Justification::centredBottom);
        g.setColour (filled ? c.withAlpha (0.7f) : p.t3); g.setFont (font (10.0f)); g.drawText (sub, r.withTrimmedTop (2), juce::Justification::centredTop);
        return;
    }
    g.setColour (c); g.setFont (font (fs));
    g.drawFittedText (b.getButtonText(), r, (int) b.getProperties().getWithDefault ("just", juce::Justification::centred), 1, 0.9f);
}

void Look::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool over, bool)
{
    const auto p = Settings::get().palette();
    auto r = b.getLocalBounds().toFloat();
    const bool switchRight = b.getProperties().getWithDefault ("switchRight", false);
    auto sw = switchRight ? r.removeFromRight (34).withSizeKeepingCentre (30, 16) : r.removeFromLeft (34).withSizeKeepingCentre (30, 16);
    const bool on = b.getToggleState();
    g.setColour (on ? p[lime] : p.ink.withAlpha (over ? 0.26f : 0.18f)); g.fillRoundedRectangle (sw, 8.0f);
    if (p.highContrast) { g.setColour (p.ink); g.drawRoundedRectangle (sw, 8.0f, 1.0f); }
    auto knob = juce::Rectangle<float> (12, 12).withCentre ({ on ? sw.getRight() - 8 : sw.getX() + 8, sw.getCentreY() });
    g.setColour (on ? p.onAccent : p.t1); g.fillEllipse (knob);
    if (Settings::get().focusRing() && b.hasKeyboardFocus (false)) { g.setColour (p.focus); g.drawRoundedRectangle (sw.expanded (3), 10, 2.0f); }
    if (b.getButtonText().isNotEmpty() && ! switchRight)
    {
        g.setColour (p.t1); g.setFont (font (12.0f));
        g.drawText (b.getButtonText(), r.withTrimmedLeft (6), juce::Justification::centredLeft);
    }
}

void Look::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& cb)
{
    const auto p = Settings::get().palette();
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    g.setColour (p.wellColour()); g.fillRoundedRectangle (r, 7.0f);
    g.setColour (p.ink.withAlpha (p.highContrast ? 0.7f : p.borderAlpha + 0.04f)); g.drawRoundedRectangle (r, 7.0f, 1.0f);
    g.setColour (p.t3); g.setFont (font (11.0f));
    g.drawText (juce::String (juce::CharPointer_UTF8 ("\xe2\x96\xbe")), juce::Rectangle<float> ((float) w - 22, 0, 16, (float) h), juce::Justification::centred);
    if (Settings::get().focusRing() && cb.hasKeyboardFocus (true)) { g.setColour (p.focus); g.drawRoundedRectangle (r.expanded (1.5f), 9, 2.0f); }
}

void Look::positionComboBoxText (juce::ComboBox& cb, juce::Label& l)
{
    l.setBounds (8, 0, cb.getWidth() - 30, cb.getHeight());
    l.setFont (font (12.0f));
}

void Look::drawPopupMenuBackground (juce::Graphics& g, int w, int h)
{
    const auto p = Settings::get().palette();
    g.fillAll (p.card);
    g.setColour (p.border()); g.drawRect (0, 0, w, h);
}

juce::Rectangle<int> Look::getTooltipBounds (const juce::String& tip, juce::Point<int> pos, juce::Rectangle<int> parent)
{
    const auto lines = juce::StringArray::fromLines (tip);
    juce::AttributedString s; s.append (tip, font (12.0f));
    juce::TextLayout tl; tl.createLayoutWithBalancedLineLengths (s, 260.0f);
    const int w = (int) std::ceil (juce::jmin (280.0f, tl.getWidth())) + 26, h = (int) std::ceil (tl.getHeight()) + 22;
    return juce::Rectangle<int> (pos.x > parent.getCentreX() ? pos.x - (w + 12) : pos.x + 12,
                                 pos.y > parent.getCentreY() ? pos.y - (h + 8) : pos.y + 18, w, h).constrainedWithin (parent);
}

void Look::drawTooltip (juce::Graphics& g, const juce::String& text, int w, int h)
{
    const auto p = Settings::get().palette();
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h);
    g.setColour (p.card); g.fillRoundedRectangle (r, 10.0f);
    g.setColour (p[teal].withAlpha (0.6f)); g.drawRoundedRectangle (r.reduced (0.5f), 10.0f, 1.0f);
    juce::AttributedString s;
    auto lines = juce::StringArray::fromLines (text);
    if (lines.size() > 1)
    {
        s.append (lines[0] + "\n", font (12.5f), p[teal]);
        lines.remove (0);
        s.append (lines.joinIntoString ("\n"), font (12.0f), p.t1);
    }
    else s.append (text, font (12.0f), p.t1);
    juce::TextLayout tl; tl.createLayoutWithBalancedLineLengths (s, (float) w - 26);
    tl.draw (g, r.reduced (13, 11));
}

void Look::fillTextEditorBackground (juce::Graphics& g, int w, int h, juce::TextEditor&)
{
    const auto p = Settings::get().palette();
    g.setColour (p.wellColour()); g.fillRoundedRectangle (0, 0, (float) w, (float) h, 7.0f);
}

void Look::drawTextEditorOutline (juce::Graphics& g, int w, int h, juce::TextEditor& t)
{
    const auto p = Settings::get().palette();
    g.setColour (t.hasKeyboardFocus (true) ? p.focus : p.ink.withAlpha (p.borderAlpha + 0.04f));
    g.drawRoundedRectangle (0.5f, 0.5f, (float) w - 1, (float) h - 1, 7.0f, t.hasKeyboardFocus (true) ? 1.5f : 1.0f);
}

void Look::drawScrollbar (juce::Graphics& g, juce::ScrollBar&, int x, int y, int w, int h, bool vertical, int thumbStart, int thumbSize, bool over, bool)
{
    const auto p = Settings::get().palette();
    auto r = vertical ? juce::Rectangle<int> (x + w - 6, thumbStart, 4, thumbSize) : juce::Rectangle<int> (thumbStart, y + h - 6, thumbSize, 4);
    g.setColour (p.ink.withAlpha (over ? 0.35f : 0.2f)); g.fillRoundedRectangle (r.toFloat(), 2.0f);
}

// ---------------------------------------------------------------- knob
Knob::Knob (juce::AudioProcessorValueTreeState& s, const juce::String& id, const juce::String& t, Accent a, bool inv)
    : param (*s.getParameter (id)), title (t), accent (a), invert (inv)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    slider.setRange (0.0, 1.0, 0.0);
    slider.setMouseDragSensitivity (220);
    slider.setVelocityBasedMode (false);
    slider.getProperties().set ("accent", (int) a);
    slider.setTitle (t);
    slider.setWantsKeyboardFocus (true);
    slider.setMouseCursor (juce::MouseCursor::UpDownLeftRightResizeCursor);
    const double def = param.getDefaultValue();
    slider.setDoubleClickReturnValue (true, invert ? 1.0 - def : def);
    slider.textFromValueFunction = [this] (double v) { return valueText ? valueText ((float) (invert ? 1.0 - v : v)) : param.getText ((float) (invert ? 1.0 - v : v), 64); };
    slider.valueFromTextFunction = [this] (const juce::String& txt) { const double n = param.getValueForText (txt); return invert ? 1.0 - n : n; };
    addAndMakeVisible (slider);

    att = std::make_unique<juce::ParameterAttachment> (param, [this] (float v)
    {
        const juce::ScopedValueSetter<bool> sv (ignore, true);
        const double n = param.convertTo0to1 (v);
        slider.setValue (invert ? 1.0 - n : n, juce::dontSendNotification);
        update();
    });
    slider.onDragStart = [this] { att->beginGesture(); };
    slider.onDragEnd = [this] { att->endGesture(); };
    slider.onValueChange = [this]
    {
        if (ignore) return;
        const double n = invert ? 1.0 - slider.getValue() : slider.getValue();
        att->setValueAsPartOfGesture (param.convertFrom0to1 ((float) n));
        update();
        if (onUserChange) onUserChange (title + ", " + slider.getTextFromValue (slider.getValue()));
    };
    att->sendInitialUpdate();
    setTitle (t);
}

Knob::~Knob() = default;

void Knob::setHelp (const juce::String& h)
{
    slider.setTooltip (title + "\n" + h);
    slider.setHelpText (h);
    slider.setDescription (h);
}

void Knob::update() { repaint(); }

void Knob::resized()
{
    auto r = getLocalBounds();
    r.removeFromTop ((int) (17 * Settings::get().textScale()));
    slider.setBounds (r.removeFromTop (dialSize).withSizeKeepingCentre (dialSize, dialSize));
}

void Knob::paint (juce::Graphics& g)
{
    const auto p = Settings::get().palette();
    auto r = getLocalBounds().toFloat();
    g.setColour (p.t2); g.setFont (font (11.5f));
    g.drawText (title, r.removeFromTop (17 * Settings::get().textScale()), juce::Justification::centredTop);
    r.removeFromTop ((float) dialSize + 6);
    g.setColour (p.t1); g.setFont (font (valueSize, true));
    g.drawText (slider.getTextFromValue (slider.getValue()), r.removeFromTop (valueSize * Settings::get().textScale() + 4), juce::Justification::centredTop);
    if (Settings::get().focusRing() && slider.hasKeyboardFocus (false))
    {
        g.setColour (p.focus);
        g.drawRoundedRectangle (slider.getBounds().toFloat().expanded (4), 12.0f, 2.0f);
    }
}

// ---------------------------------------------------------------- meter
void Meter::push (float pk)
{
    level = juce::jmax (pk, level * 0.86f);
    if (pk >= hold) { hold = pk; holdCount = 40; } else if (--holdCount < 0) hold *= 0.95f;
    repaint();
}

void Meter::paint (juce::Graphics& g)
{
    const auto p = Settings::get().palette();
    auto r = getLocalBounds().toFloat();
    g.setColour (p.wellColour()); g.fillRoundedRectangle (r, 9.0f);
    auto in = r.reduced (5);
    auto toY = [&] (float lin) { const float db = juce::Decibels::gainToDecibels (lin, -60.0f); return in.getBottom() - (db + 60.0f) / 63.0f * in.getHeight(); };
    const float bw = (in.getWidth() - 4) / 2;
    for (int i = 0; i < 2; ++i)
    {
        auto bar = juce::Rectangle<float> (in.getX() + (float) i * (bw + 4), in.getY(), bw, in.getHeight());
        juce::ColourGradient cg (p[teal], 0, in.getBottom(), p[rose], 0, in.getY(), false);
        cg.addColour (0.7, p[lime]); cg.addColour (0.88, p[orange]);
        g.setGradientFill (cg);
        const float y = toY (level * (i == 0 ? 1.0f : 0.96f));
        g.fillRoundedRectangle (bar.withTop (y), 3.0f);
        g.setColour (p.t1.withAlpha (0.7f)); g.fillRect (bar.getX(), toY (hold), bw, 1.5f);
    }
}

std::unique_ptr<juce::AccessibilityHandler> Meter::createAccessibilityHandler()
{
    struct V : juce::AccessibilityValueInterface
    {
        explicit V (Meter& m) : meter (m) {}
        bool isReadOnly() const override { return true; }
        double getCurrentValue() const override { return meter.levelDb(); }
        void setValue (double) override {}
        juce::String getCurrentValueAsString() const override { return juce::String (juce::roundToInt (meter.levelDb())) + " decibels"; }
        void setValueAsString (const juce::String&) override {}
        AccessibleValueRange getRange() const override { return { { -60.0, 3.0 }, 1.0 }; }
        Meter& meter;
    };
    return std::make_unique<juce::AccessibilityHandler> (*this, juce::AccessibilityRole::progressBar,
        juce::AccessibilityActions(), juce::AccessibilityHandler::Interfaces { std::make_unique<V> (*this) });
}

// ---------------------------------------------------------------- segmented group
juce::TextButton& SegGroup::add (const juce::String& text, const juce::String& sub)
{
    auto b = std::make_unique<juce::TextButton> (text);
    setStyle (*b, "seg", lime);
    if (sub.isNotEmpty()) b->getProperties().set ("sub", sub);
    b->setClickingTogglesState (false);
    b->setTitle (sub.isNotEmpty() ? text + ", " + sub : text);
    addAndMakeVisible (*b);
    buttons.push_back (std::move (b));
    return *buttons.back();
}

void SegGroup::resized()
{
    auto r = getLocalBounds().reduced (4);
    const int n = (int) buttons.size();
    if (n == 0) return;
    const int w = (r.getWidth() - 4 * (n - 1)) / n;
    for (auto& b : buttons) { b->setBounds (r.removeFromLeft (w)); r.removeFromLeft (4); }
}

void SegGroup::paint (juce::Graphics& g)
{
    const auto p = Settings::get().palette();
    g.setColour (p.wellColour()); g.fillRoundedRectangle (getLocalBounds().toFloat(), 11.0f);
    if (p.highContrast) { g.setColour (p.ink.withAlpha (0.6f)); g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 11.0f, 1.0f); }
}
} // namespace ui
