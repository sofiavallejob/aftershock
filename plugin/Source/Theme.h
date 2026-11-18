// Aftershock, a reverb made from earthquakes. (C) 2026 Sofia Vallejo Budziszewski, AGPL-3.0.
// Free to port, by humans. Bring your own ears. See PORTING.md.

#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

// Colours, fonts and user settings shared by the whole interface.
namespace ui
{
enum Accent { lime = 0, teal, green, peri, grey, lav, pink, rose, yellow, orange, numAccents };

struct Palette
{
    juce::Colour bg, glow1, glow2, ink, well, t1, t2, t3, onAccent, limeText, focus, globe1, globe2, globe3, dot, card;
    float wellAlpha = 0.24f, borderAlpha = 0.08f, tint = 1.0f;
    bool highContrast = false;
    std::array<juce::Colour, numAccents> acc;
    juce::Colour operator[] (Accent a) const { return acc[(size_t) a]; }
    juce::Colour border() const { return ink.withAlpha (borderAlpha); }
    juce::Colour wellColour() const { return well.withAlpha (wellAlpha); }
};

// One accent per tectonic setting, in the pack's setting order.
Accent settingAccent (const juce::String& key);

// User settings, saved in the user's application data folder.
class Settings : public juce::ChangeBroadcaster
{
public:
    static Settings& get();

    enum Theme { dark = 0, light, system };
    enum Vision { normal = 0, redGreen, blueYellow, mono };

    int theme() const        { return props().getIntValue ("theme", dark); }
    double scale() const     { return juce::jlimit (0.5, 2.5, props().getDoubleValue ("scale", 1.0)); }
    int textSize() const     { return props().getIntValue ("textSize", 0); }       // 0, 1, 2
    int vision() const       { return props().getIntValue ("vision", normal); }
    bool highContrast() const{ return props().getBoolValue ("highContrast", false); }
    bool reduceMotion() const;
    bool announce() const    { return props().getBoolValue ("announce", true); }
    bool focusRing() const   { return props().getBoolValue ("focusRing", true); }
    bool help() const        { return props().getBoolValue ("help", true); }
    bool openLast() const    { return props().getBoolValue ("openLast", true); }
    bool fastPreview() const { return props().getBoolValue ("fastPreview", false); }
    bool advanced() const    { return props().getBoolValue ("advanced", false); }

    void set (const juce::String& key, const juce::var& v);
    void setQuietly (const juce::String& key, const juce::var& v) { file->setValue (key, v); file->saveIfNeeded(); }
    void remove (const juce::String& key) { file->removeValue (key); file->saveIfNeeded(); cacheValid = false; sendChangeMessage(); }
    bool getBool (const juce::String& key, bool def) const { return props().getBoolValue (key, def); }
    juce::var getRaw (const juce::String& key, const juce::var& def) const { return props().getValue (key, def.toString()); }

    bool isDark() const;
    Palette palette() const;
    float textScale() const { return 1.0f + 0.15f * (float) textSize(); }

private:
    Settings();
    juce::PropertiesFile& props() const { return *file; }
    std::unique_ptr<juce::PropertiesFile> file;
    mutable Palette cache;
    mutable bool cacheValid = false, cacheDark = false;
    Palette build() const;
public:
    void invalidate() { cacheValid = false; }
};

juce::Font font (float size, bool bold = false);
juce::Typeface::Ptr pixelTypeface();

// A rounded panel with a soft colour fade from the top.
void drawPanel (juce::Graphics&, juce::Rectangle<float>, Accent top, Accent bottom, float strength = 0.14f);
// Small coloured dot + spaced capitals, used for group titles.
void drawGroupTitle (juce::Graphics&, juce::Rectangle<float>, Accent, const juce::String& title, const juce::String& note = {});
} // namespace ui
