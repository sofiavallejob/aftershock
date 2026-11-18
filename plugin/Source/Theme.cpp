// Aftershock, a reverb made from earthquakes. (C) 2026 Sofia Vallejo Budziszewski, AGPL-3.0.
// Free to port, by humans. Bring your own ears. See PORTING.md.

#include "Theme.h"
#include "BinaryData.h"

namespace ui
{
static juce::Colour C (juce::uint32 rgb) { return juce::Colour (0xff000000 | rgb); }

Accent settingAccent (const juce::String& key)
{
    if (key == "interface")  return teal;
    if (key == "intraslab")  return orange;
    if (key == "deep")       return lav;
    if (key == "collision")  return rose;
    if (key == "strikeslip") return yellow;
    if (key == "europe")     return peri;
    if (key == "oceanic")    return green;
    if (key == "craton")     return grey;
    if (key == "volcanic")   return pink;
    return grey;
}

// Owned by JUCE and deleted when JUCE shuts down, not by a static destructor: on Windows a static
// destructor runs while the DLL unloads (under the loader lock), and the settings file's timer
// and the change broadcaster can deadlock there.
struct SettingsHolder : juce::DeletedAtShutdown
{
    ~SettingsHolder() override { clearSingletonInstance(); }
    Settings settings;
    juce::Typeface::Ptr pixel;
    JUCE_DECLARE_SINGLETON_INLINE (SettingsHolder, false)
};

Settings& Settings::get() { return SettingsHolder::getInstance()->settings; }

Settings::Settings()
{
    juce::PropertiesFile::Options o;
    o.applicationName = "Aftershock";
    o.filenameSuffix = ".settings";
    o.folderName = "Aftershock";
    o.osxLibrarySubFolder = "Application Support";
    o.storageFormat = juce::PropertiesFile::storeAsXML;
    o.millisecondsBeforeSaving = -1;   // no background timer; set() saves straight away
    file = std::make_unique<juce::PropertiesFile> (o);
}

void Settings::set (const juce::String& key, const juce::var& v)
{
    file->setValue (key, v);
    file->saveIfNeeded();
    cacheValid = false;
    sendChangeMessage();
}

bool Settings::reduceMotion() const
{
    return props().getBoolValue ("reduceMotion", false);
}

bool Settings::isDark() const
{
    if (highContrast()) return true;
    const int t = theme();
    if (t == system) return juce::Desktop::getInstance().isDarkModeActive();
    return t == dark;
}

Palette Settings::palette() const
{
    if (! cacheValid) { cache = build(); cacheValid = true; }
    return cache;
}

Palette Settings::build() const
{
    Palette p;
    if (highContrast())
    {
        p.bg = C (0x000000); p.glow1 = p.glow2 = C (0x000000); p.ink = C (0xffffff); p.well = C (0xffffff);
        p.wellAlpha = 0.07f; p.borderAlpha = 0.6f; p.tint = 0.0f; p.highContrast = true;
        p.t1 = C (0xffffff); p.t2 = C (0xffffff); p.t3 = C (0xd8d8d8); p.onAccent = C (0x000000);
        p.globe1 = C (0x1a1a1a); p.globe2 = C (0x0d0d0d); p.globe3 = C (0x000000); p.dot = juce::Colours::white.withAlpha (0.8f);
        p.card = C (0x000000); p.focus = C (0xffffff);
        p.acc = { C (0xf0e442), C (0x56b4e9), C (0x2ec4a0), C (0x7fb8ff), C (0xd6d6d6), C (0xe39ccb), C (0xe39ccb), C (0xff7a3d), C (0xf0e442), C (0xe69f00) };
    }
    else if (isDark())
    {
        p.bg = C (0x12101b); p.glow1 = C (0x966ee6); p.glow2 = C (0x6ebec8); p.ink = C (0xffffff); p.well = C (0x000000);
        p.wellAlpha = 0.24f; p.borderAlpha = 0.08f; p.tint = 1.0f;
        p.t1 = C (0xf1effa); p.t2 = C (0xb9b6cc); p.t3 = C (0x8d8aa3); p.onAccent = C (0x17151f);
        p.globe1 = C (0x2a2c48); p.globe2 = C (0x191a2c); p.globe3 = C (0x0e0e18); p.dot = C (0xd2d7f0).withAlpha (0.55f);
        p.card = C (0x1a1726); p.focus = C (0xe4f58c);
        p.acc = { C (0xe4f58c), C (0x68c7d2), C (0x77b99a), C (0x8ba9ec), C (0xb7c1ce), C (0xc798ee), C (0xdb93c0), C (0xe8899b), C (0xe9cb72), C (0xeda967) };
    }
    else
    {
        p.bg = C (0xf5f2fa); p.glow1 = C (0xbea0f5); p.glow2 = C (0x78c8d2); p.ink = C (0x2d2355); p.well = C (0xffffff);
        p.wellAlpha = 0.7f; p.borderAlpha = 0.10f; p.tint = 0.9f;
        p.t1 = C (0x221d33); p.t2 = C (0x4c4663); p.t3 = C (0x6c6683); p.onAccent = C (0x17151f);
        p.globe1 = C (0xf7f8fc); p.globe2 = C (0xe3e6f2); p.globe3 = C (0xc9cde0); p.dot = C (0x465082).withAlpha (0.45f);
        p.card = C (0xfbf9fe); p.focus = C (0x6b46c1);
        p.acc = { C (0xd9ef6e), C (0x3fa8b5), C (0x4f9a77), C (0x6f8fe0), C (0x8d97a8), C (0xa97ee6), C (0xc86fa5), C (0xd9687e), C (0xd4ae3a), C (0xe0904a) };
    }

    // colour vision: replace accents with sets that stay distinct
    switch (vision())
    {
        case redGreen:   // Okabe-Ito
            p.acc = { C (0xf0e442), C (0x56b4e9), C (0x009e73), C (0x0072b2), C (0xbbbbbb), C (0xcc79a7), C (0xcc79a7), C (0xd55e00), C (0xf0e442), C (0xe69f00) };
            if (p.bg.getPerceivedBrightness() < 0.5f) p.acc[(size_t) peri] = C (0x5aa9e6);
            break;
        case blueYellow:
            p.acc = { C (0xf2f2f2), C (0x4fb3a9), C (0x2f8f87), C (0x7fd1c8), C (0xbdbdbd), C (0xe8899b), C (0xf2a6b3), C (0xb03a48), C (0xdedede), C (0xd9687e) };
            if (p.bg.getPerceivedBrightness() > 0.5f) p.acc[(size_t) lime] = p.acc[(size_t) yellow] = C (0x4a4a4a);
            break;
        case mono:
        {
            const bool d = p.bg.getPerceivedBrightness() < 0.5f;
            for (int i = 0; i < numAccents; ++i)
                p.acc[(size_t) i] = d ? C (0xffffff).darker (0.12f * (float) (i % 4)) : C (0x222222).brighter (0.25f * (float) (i % 4));
            break;
        }
        default: break;
    }
    p.limeText = p.bg.getPerceivedBrightness() > 0.5f ? (vision() == normal && ! p.highContrast ? C (0x5f7110) : p.t1) : p.acc[(size_t) lime];
    return p;
}

juce::Typeface::Ptr pixelTypeface()
{
    auto* h = SettingsHolder::getInstance();
    if (h->pixel == nullptr) h->pixel = juce::Typeface::createSystemTypefaceFor (BinaryData::PixelifySans_ttf, BinaryData::PixelifySans_ttfSize);
    return h->pixel;
}

juce::Font font (float size, bool bold)
{
    juce::Font f (juce::FontOptions().withTypeface (pixelTypeface()).withHeight (size * Settings::get().textScale()));
    if (bold) f.setHorizontalScale (1.03f);
    return f;
}

void drawPanel (juce::Graphics& g, juce::Rectangle<float> r, Accent top, Accent bottom, float s)
{
    const auto p = Settings::get().palette();
    const float k = s * p.tint;
    juce::Path shape; shape.addRoundedRectangle (r, 16.0f);

    // soft shadow
    juce::DropShadow (juce::Colours::black.withAlpha (p.highContrast ? 0.0f : 0.18f), 22, { 0, 8 }).drawForPath (g, shape);
    g.setColour (p.bg); g.fillPath (shape);
    if (k > 0.0f)
    {
        // long, smooth fade: many stops so there is no visible band
        juce::ColourGradient cg (p[top].withAlpha (k), r.getX(), r.getY(), p[bottom].withAlpha (0.0f), r.getX(), r.getBottom(), false);
        const float stops[] = { 0.12f, 0.25f, 0.4f, 0.55f, 0.7f, 0.85f };
        for (float t : stops)
        {
            const float a = k * std::pow (1.0f - t, 1.8f);
            cg.addColour (t, p[top].interpolatedWith (p[bottom], t).withAlpha (a));
        }
        g.setGradientFill (cg); g.fillPath (shape);
    }
    g.setColour (p.ink.withAlpha (p.highContrast ? 0.6f : p.borderAlpha)); g.strokePath (shape, juce::PathStrokeType (1.0f));
}

void drawGroupTitle (juce::Graphics& g, juce::Rectangle<float> r, Accent a, const juce::String& title, const juce::String& note)
{
    const auto p = Settings::get().palette();
    g.setColour (p[a]); g.fillEllipse (r.getX(), r.getCentreY() - 3.5f, 7, 7);
    g.setColour (p.t2); g.setFont (font (11.0f).withExtraKerningFactor (0.18f));
    g.drawText (title, r.withTrimmedLeft (14), juce::Justification::centredLeft);
    if (note.isNotEmpty())
    {
        g.setColour (p.t3); g.setFont (font (10.0f).withExtraKerningFactor (0.08f));
        g.drawText (note, r, juce::Justification::centredRight);
    }
}
} // namespace ui
