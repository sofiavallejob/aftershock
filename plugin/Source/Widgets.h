// Aftershock, a reverb made from earthquakes. (C) 2026 Sofia Vallejo Budziszewski, AGPL-3.0.
// Free to port, by humans. Bring your own ears. See PORTING.md.

#pragma once
#include "Theme.h"
#include <juce_audio_processors/juce_audio_processors.h>

namespace ui
{
class Look : public juce::LookAndFeel_V4
{
    FontLease fontLease;   // must be constructed before refresh() asks for the font
public:
    Look();
    void refresh();   // re-read the palette after a settings change

    void drawRotarySlider (juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool, bool) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool, bool) override;
    void drawComboBox (juce::Graphics&, int, int, bool, int, int, int, int, juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override { return font (12.0f); }
    juce::Font getPopupMenuFont() override { return font (13.0f); }
    juce::Font getTextButtonFont (juce::TextButton&, int) override { return font (12.0f); }
    juce::Font getLabelFont (juce::Label& l) override { return font (l.getFont().getHeight() / Settings::get().textScale()); }
    void drawPopupMenuBackground (juce::Graphics&, int, int) override;
    juce::Rectangle<int> getTooltipBounds (const juce::String&, juce::Point<int>, juce::Rectangle<int>) override;
    void drawTooltip (juce::Graphics&, const juce::String&, int, int) override;
    void fillTextEditorBackground (juce::Graphics&, int, int, juce::TextEditor&) override;
    void drawTextEditorOutline (juce::Graphics&, int, int, juce::TextEditor&) override;
    void drawScrollbar (juce::Graphics&, juce::ScrollBar&, int, int, int, int, bool, int, int, bool, bool) override;
    void drawFocusOutline (juce::Graphics&, juce::Component&);
    void drawLinearSlider (juce::Graphics&, int, int, int, int, float, float, float, juce::Slider::SliderStyle, juce::Slider&) override;
};

// Button styles are set with the "style" property: "accent" + an Accent index, "outline", "plain", "seg"
void setStyle (juce::Button&, const juce::String& style, Accent a = lime);

// A rotary control bound to one parameter. The slider runs over the parameter's
// normalised range, optionally inverted, with its own value text.
class Knob : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState&, const juce::String& paramId, const juce::String& title, Accent, bool invert = false);
    ~Knob() override;
    void setValueText (std::function<juce::String (float normalised)> f) { valueText = std::move (f); update(); }
    void setHelp (const juce::String& h);
    void setSizes (int dial, float valueFont) { dialSize = dial; valueSize = valueFont; resized(); }
    void resized() override;
    void paint (juce::Graphics&) override;
    juce::Slider slider;
    std::function<void (const juce::String&)> onUserChange;   // spoken text after a change
private:
    void update();
    juce::RangedAudioParameter& param;
    std::unique_ptr<juce::ParameterAttachment> att;
    juce::String title;
    Accent accent;
    bool invert;
    int dialSize = 70;
    float valueSize = 20.0f;
    std::function<juce::String (float)> valueText;
    bool ignore = false;
};

// Peak meter with two bars and slow fall-back
class Meter : public juce::Component
{
public:
    Meter (const juce::String& name) { setTitle (name); setAccessible (true); }
    void push (float peak);
    void paint (juce::Graphics&) override;
    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;
    float levelDb() const { return juce::Decibels::gainToDecibels (level, -60.0f); }
private:
    float level = 0, hold = 0;
    int holdCount = 0;
};

// Draws a "seg" group background behind a set of radio buttons
class SegGroup : public juce::Component
{
public:
    SegGroup (const juce::String& name) { setTitle (name); setInterceptsMouseClicks (false, true); }
    void paint (juce::Graphics&) override;
    void resized() override;
    std::vector<std::unique_ptr<juce::TextButton>> buttons;
    juce::TextButton& add (const juce::String& text, const juce::String& sub = {});
    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override
    { return std::make_unique<juce::AccessibilityHandler> (*this, juce::AccessibilityRole::group); }
};
} // namespace ui
