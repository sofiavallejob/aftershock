// Aftershock, a reverb made from earthquakes. (C) 2026 Sofia Vallejo Budziszewski, AGPL-3.0.
// Free to port, by humans. Bring your own ears. See PORTING.md.

#include "Panels.h"

namespace ui
{
// ---------------------------------------------------------------- overlay
Overlay::Overlay (const juce::String& t, int w, int h) : heading (t), cw (w), ch (h)
{
    setStyle (closeButton, "plain");
    closeButton.setTitle ("Close " + t);
    closeButton.onClick = [this] { close(); };
    addAndMakeVisible (closeButton);
    setWantsKeyboardFocus (true);
    setTitle (t);
}

juce::Rectangle<int> Overlay::card() const { return getLocalBounds().withSizeKeepingCentre (cw, ch); }

void Overlay::paint (juce::Graphics& g)
{
    const auto p = Settings::get().palette();
    g.fillAll (juce::Colour (0xff05040c).withAlpha (p.bg.getPerceivedBrightness() > 0.5f ? 0.35f : 0.62f));
    auto c = card().toFloat();
    juce::Path shape; shape.addRoundedRectangle (c, 16.0f);
    juce::DropShadow (juce::Colours::black.withAlpha (0.5f), 50, { 0, 20 }).drawForPath (g, shape);
    g.setColour (p.card); g.fillPath (shape);
    if (p.tint > 0)
    {
        juce::ColourGradient cg (p[lav].withAlpha (0.16f * p.tint), 0, c.getY(), p[lav].withAlpha (0.0f), 0, c.getY() + c.getHeight() * 0.7f, false);
        cg.addColour (0.3, p[lav].withAlpha (0.06f * p.tint));
        g.setGradientFill (cg); g.fillPath (shape);
    }
    g.setColour (p.ink.withAlpha (p.highContrast ? 0.7f : 0.12f)); g.strokePath (shape, juce::PathStrokeType (1.0f));
    if (showHeading)
    {
        g.setColour (p.t1); g.setFont (font (22.0f, true));
        g.drawText (heading, card().reduced (24, 20).removeFromTop (30), juce::Justification::centredLeft);
    }
}

void Overlay::resized()
{
    auto c = card().reduced (24, 20);
    closeButton.setBounds (c.getRight() - 28, c.getY() + 1, 28, 28);
    if (showHeading) c.removeFromTop (44);
    layout (c);
}

void Overlay::mouseDown (const juce::MouseEvent& e) { if (! card().contains (e.getPosition())) close(); }

bool Overlay::keyPressed (const juce::KeyPress& k)
{
    if (k == juce::KeyPress::escapeKey) { close(); return true; }
    return false;
}

void Overlay::visibilityChanged()
{
    if (isVisible()) juce::MessageManager::callAsync ([sp = juce::Component::SafePointer<Overlay> (this)] { if (sp != nullptr) sp->grabKeyboardFocus(); });
}

std::unique_ptr<juce::AccessibilityHandler> Overlay::createAccessibilityHandler()
{
    return std::make_unique<juce::AccessibilityHandler> (*this, juce::AccessibilityRole::dialogWindow);
}

// ---------------------------------------------------------------- settings
SettingsPanel::SettingsPanel() : Overlay ("Settings", 780, 600)
{
    auto& s = Settings::get();
    auto seg = [this] (SegGroup& gp, juce::StringArray items, juce::String key, std::function<juce::var (int)> value)
    {
        for (int i = 0; i < items.size(); ++i)
        {
            auto& b = gp.add (items[i]);
            b.onClick = [key, value, i] { Settings::get().set (key, value (i)); };
        }
        addAndMakeVisible (gp);
    };
    seg (theme, { "Dark", "Light", "Match system" }, "theme", [] (int i) { return i; });
    seg (size, { "75 %", "100 %", "125 %", "150 %", "200 %" }, "scale", [] (int i) { const double v[] = { 0.75, 1.0, 1.25, 1.5, 2.0 }; return v[i]; });
    seg (text, { "Default", "Large", "Extra large" }, "textSize", [] (int i) { return i; });
    seg (vision, { "Default", "Red-green safe", "Blue-yellow safe", "Monochrome" }, "vision", [] (int i) { return i; });
    for (auto& b : vision.buttons) b->getProperties().set ("sub", juce::String (juce::CharPointer_UTF8 ("\xc2\xa0")));

    auto addRow = [this] (const juce::String& key, const juce::String& label, const juce::String& hint, bool def)
    {
        Row r { std::make_unique<juce::ToggleButton>(), key, label, hint, def };
        r.button->getProperties().set ("switchRight", true);
        r.button->setTitle (label);
        r.button->setDescription (hint);
        r.button->onClick = [k = key, b = r.button.get()] { Settings::get().set (k, b->getToggleState()); };
        addAndMakeVisible (*r.button);
        rows.push_back (std::move (r));
    };
    addRow ("help", "Help on hover", "Explains each control in a bubble", true);
    addRow ("openLast", "Open with last preset", "New instances start where you left off", true);
    addRow ("highContrast", "High contrast", "Black background, solid outlines, no fades", false);
    addRow ("reduceMotion", "Reduce motion", "Stops the globe from turning", false);
    addRow ("announce", "Announce changes", "Screen readers speak new values as you turn a knob", true);
    addRow ("focusRing", "Keyboard focus outline", "Tab moves between controls, arrows change values", true);

    for (auto* b : { &reset, &done, &folder }) { setStyle (*b, b == &done ? "accent" : "plain", lime); addAndMakeVisible (*b); }
    done.onClick = [this] { close(); };
    folder.onClick = [] { PresetManager::userFolder().revealToUser(); };
    reset.onClick = []
    {
        auto& st = Settings::get();
        for (auto k : { "theme", "scale", "textSize", "vision", "highContrast", "reduceMotion", "announce", "focusRing", "help", "openLast" })
            st.remove (k);
    };
    s.addChangeListener (this);
    sync();
}

SettingsPanel::~SettingsPanel() { Settings::get().removeChangeListener (this); }

void SettingsPanel::sync()
{
    auto& s = Settings::get();
    auto mark = [] (SegGroup& gp, int on) { for (int i = 0; i < (int) gp.buttons.size(); ++i) gp.buttons[(size_t) i]->setToggleState (i == on, juce::dontSendNotification); gp.repaint(); };
    mark (theme, s.theme());
    const double sc = s.scale(); int si = 1; const double v[] = { 0.75, 1.0, 1.25, 1.5, 2.0 };
    for (int i = 0; i < 5; ++i) if (std::abs (v[i] - sc) < 0.01) si = i;
    mark (size, si);
    mark (text, s.textSize());
    mark (vision, s.vision());
    for (auto& r : rows)
        r.button->setToggleState (s.getBool (r.key, r.def), juce::dontSendNotification);
    repaint();
}

void SettingsPanel::layout (juce::Rectangle<int> c)
{
    labels.clear();
    auto foot = c.removeFromBottom (36);
    reset.setBounds (foot.removeFromLeft (150).withSizeKeepingCentre (150, 30));
    done.setBounds (foot.removeFromRight (90).withSizeKeepingCentre (90, 30));
    folder.setBounds (foot.removeFromRight (170).withSizeKeepingCentre (160, 30));
    c.removeFromBottom (14);
    auto left = c.removeFromLeft ((c.getWidth() - 28) / 2); c.removeFromLeft (28);
    auto right = c;

    auto caption = [this] (juce::Rectangle<int>& col, const juce::String& t) { labels.push_back ({ col.removeFromTop (20), t }); };
    labels.push_back ({ left.removeFromTop (18), "#APPEARANCE" }); left.removeFromTop (8);
    caption (left, "Theme"); theme.setBounds (left.removeFromTop (34)); left.removeFromTop (12);
    caption (left, "Window size"); size.setBounds (left.removeFromTop (34)); left.removeFromTop (12);
    caption (left, "Text size"); text.setBounds (left.removeFromTop (34)); left.removeFromTop (18);
    labels.push_back ({ left.removeFromTop (18), "#GENERAL" }); left.removeFromTop (6);
    for (int i = 0; i < 2; ++i) { auto r = left.removeFromTop (44); rows[(size_t) i].button->setBounds (r.removeFromRight (40).withSizeKeepingCentre (40, 24)); labels.push_back ({ r, "!" + rows[(size_t) i].label + "|" + rows[(size_t) i].hint }); }

    labels.push_back ({ right.removeFromTop (18), "#ACCESSIBILITY" }); right.removeFromTop (8);
    caption (right, "Colour vision");
    visionArea = right.removeFromTop (70);
    vision.setBounds (visionArea);
    right.removeFromTop (10);
    for (int i = 2; i < (int) rows.size(); ++i) { auto r = right.removeFromTop (44); rows[(size_t) i].button->setBounds (r.removeFromRight (40).withSizeKeepingCentre (40, 24)); labels.push_back ({ r, "!" + rows[(size_t) i].label + "|" + rows[(size_t) i].hint }); }
    right.removeFromTop (6);
    preview = right.removeFromTop (58);
}

void SettingsPanel::paint (juce::Graphics& g)
{
    Overlay::paint (g);
    const auto p = Settings::get().palette();
    for (auto& [r, t] : labels)
    {
        if (t.startsWith ("#"))
        {
            const Accent a = t == "#APPEARANCE" ? lav : t == "#GENERAL" ? teal : lime;
            drawGroupTitle (g, r.toFloat(), a, t.substring (1));
        }
        else if (t.startsWith ("!"))
        {
            auto rr = r.toFloat();
            g.setColour (p.t1); g.setFont (font (12.5f)); g.drawText (t.substring (1).upToFirstOccurrenceOf ("|", false, false), rr.removeFromTop (rr.getHeight() * 0.5f), juce::Justification::bottomLeft);
            g.setColour (p.t3); g.setFont (font (10.5f)); g.drawText (t.fromFirstOccurrenceOf ("|", false, false), rr.withTrimmedTop (3), juce::Justification::topLeft);
        }
        else { g.setColour (p.t1); g.setFont (font (12.5f)); g.drawText (t, r, juce::Justification::centredLeft); }
    }
    paint2 (g);
}

void SettingsPanel::paintOverChildren (juce::Graphics& g)
{
    // swatches under the colour vision names
    const auto saved = Settings::get().vision();
    juce::ignoreUnused (saved);
    static const juce::uint32 sets[4][6] = {
        { 0xe4f58c, 0x68c7d2, 0xc798ee, 0xeda967, 0xe8899b, 0x77b99a },
        { 0xf0e442, 0x56b4e9, 0xcc79a7, 0xe69f00, 0xd55e00, 0x0072b2 },
        { 0xffffff, 0xe8899b, 0x4fb3a9, 0xd0d0d0, 0xb03a48, 0x2f8f87 },
        { 0xffffff, 0xd4d4d4, 0xa8a8a8, 0x7c7c7c, 0xbdbdbd, 0xeeeeee } };
    for (int i = 0; i < (int) vision.buttons.size(); ++i)
    {
        auto b = vision.buttons[(size_t) i]->getBounds().translated (vision.getX(), vision.getY()).toFloat();
        auto sw = b.removeFromBottom (18).reduced (10, 4);
        const float w = (sw.getWidth() - 10) / 6;
        for (int k = 0; k < 6; ++k)
        {
            g.setColour (juce::Colour (0xff000000 | sets[i][k]));
            g.fillRoundedRectangle (sw.getX() + (float) k * (w + 2), sw.getY(), w, sw.getHeight(), 2.0f);
        }
    }
}

void SettingsPanel::paint2 (juce::Graphics& g)
{
    const auto p = Settings::get().palette();
    // what a screen reader says
    g.setColour (p.wellColour()); g.fillRoundedRectangle (preview.toFloat(), 10.0f);
    auto pr = preview.reduced (12, 8);
    g.setColour (p.t3); g.setFont (font (10.0f).withExtraKerningFactor (0.12f)); g.drawText ("VOICEOVER READS", pr.removeFromTop (14), juce::Justification::centredLeft);
    g.setColour (p.t1); g.setFont (font (12.0f));
    g.drawFittedText (juce::String (juce::CharPointer_UTF8 ("\xe2\x80\x9cLength, 2.8 seconds, adjustable. Swipe up or down to change.\xe2\x80\x9d")), pr, juce::Justification::centredLeft, 2);
}

// ---------------------------------------------------------------- about
static const char* howItWorks[][2] = {
    { "1. Real recordings",
      "Aftershock holds 35 earthquakes chosen to cover nine tectonic settings: subduction interfaces, intermediate-depth and deep slabs, continental collision, strike-slip faults, "
      "the Mediterranean crust, the ocean floor, old stable cratons and volcanoes. For each one, up to three permanent broadband seismometers recorded it: a near station (under about 330 km), "
      "a regional one (3 to 15 degrees away) and a far one (20 to 60 degrees). The waveforms come from the FDSN data centres EarthScope, ORFEUS and GEOFON, downloaded with ObsPy. "
      "Origin time, place, depth and magnitude come from the USGS ComCat catalogue." },
    { "2. Cleaning",
      "Each recording has the instrument's response removed (0.02 to 9.5 Hz), so what is left is true ground velocity in metres per second. It is rotated to vertical, north and east, "
      "low-passed at 8 Hz and resampled to 20 Hz. Recordings that would mislead are rejected: clipped sensors, sensors whose resting level jumps during the shaking, energy arriving before the "
      "P wave, gaps, or a signal-to-noise ratio below 8. Near stations whose seismometer clipped fall back to the accelerometer at the same site." },
    { "3. Measurements",
      "P and S arrival times come from the iasp91 Earth model. The recording is split into ten frequency bands from 0.1 to 8 Hz. For each band the build measures how much energy it carries "
      "and how fast it fades (a seismic RT60, fitted like T20 in room acoustics). The eight strongest bursts between the P and S waves are kept as the early arrivals." },
    { "4. Time",
      "Minutes of ground motion become a few seconds of sound. The window you choose (the whole recording, or from the P, S or surface waves) is squeezed into the Length you set: "
      "typically 100 to 700 times faster. The globe and seismogram show which part is used." },
    { "5. Frequency",
      "Each of the ten seismic bands drives band-passed noise in a matching audio band, from 120 Hz to 12 kHz, spaced the same way (log onto log). So the reverb inherits each band's onset, "
      "peaks and decay: low frequencies still fade slowest and high ones fastest, as in the Earth. The north component becomes the left channel and the east component the right." },
    { "6. Your controls",
      "Brightness (Spectrum in the advanced view) sets how much of the recording's band levels you hear: fully dark keeps them as recorded, which makes far stations very dark. "
      "Decay contrast scales how differently the bands fade: 1 is the data, 0 makes them fade alike, above 1 exaggerates the difference. "
      "Character (Grit) blends in the raw seismogram played faster, which is plain audification. Damping adds extra high-frequency decay." },
    { "7. The engine",
      "The impulse response is rendered in the background and convolved with zero-latency partitioned convolution. Around it: pre-delay, the early arrivals from the real P-to-S bursts, "
      "a 24 dB/oct low cut, a tilt EQ, slow modulation, mid/side width, freeze and ducking." },
    { "8. What is measured, and what is not",
      "On all 80 recordings, the reverb gets darker with distance (median centroid 2.9 kHz near, 1.6 kHz regional, 1.1 kHz far), volcanic recordings give much shorter tails and deep-slab and "
      "craton paths the longest. These groups are small, so they are reasons for a listening test, not results. The full method is in docs/SCIENCE.md in the repository." } };

AboutPanel::AboutPanel() : Overlay ("About", 520, 520)
{
    showHeading = false;
    for (auto* b : { &github, &licences, &manual, &how, &back }) { setStyle (*b, b == &github ? "accent" : "plain", lav); b->setTitle (b->getButtonText()); addAndMakeVisible (*b); }
    github.onClick = [] { juce::URL ("https://github.com/sofiavallejob/aftershock").launchInDefaultBrowser(); };
    licences.onClick = [] { juce::URL ("https://github.com/sofiavallejob/aftershock/blob/main/LICENSE").launchInDefaultBrowser(); };
    manual.onClick = [] { juce::URL ("https://github.com/sofiavallejob/aftershock/releases/latest/download/Aftershock-Manual.pdf").launchInDefaultBrowser(); };
    how.onClick = [this] { showPage (1); };
    back.onClick = [this] { showPage (0); };
    website.setFont (font (12.0f), false, juce::Justification::centredLeft);
    website.setColour (juce::HyperlinkButton::textColourId, Settings::get().palette()[lav]);
    website.setTitle ("Website");
    addAndMakeVisible (website);
    scroller.setViewedComponent (&howText, false);
    scroller.setScrollBarsShown (true, false);
    addChildComponent (scroller);
    showPage (0);
}

void AboutPanel::showPage (int pg)
{
    page = pg;
    for (auto* c : std::initializer_list<juce::Component*> { &github, &licences, &manual, &how, &website }) c->setVisible (page == 0);
    back.setVisible (page == 1);
    scroller.setVisible (page == 1);
    cw = page == 0 ? 520 : 640; ch = page == 0 ? 404 : 600;
    resized();
    repaint();
}

void AboutPanel::Text::paint (juce::Graphics& g) { layout.draw (g, getLocalBounds().toFloat()); }

void AboutPanel::layout (juce::Rectangle<int> c)
{
    if (page == 0)
    {
        auto foot = c.removeFromBottom (32);
        for (auto* b : { &github, &licences, &manual, &how })
        {
            const int w = juce::roundToInt (juce::GlyphArrangement::getStringWidth (font (12.0f), b->getButtonText())) + 28;
            b->setBounds (foot.removeFromLeft (w)); foot.removeFromLeft (8);
        }
        textArea = c;
        juce::AttributedString d;
        d.append ("Aftershock is a reverb made from real earthquake data. Every impulse response is a seismogram recorded by a real station, squeezed in time so the way the Earth rings and fades becomes your reverb tail.", font (12.0f));
        d.setLineSpacing (4.0f);
        juce::TextLayout tl; tl.createLayout (d, (float) c.getWidth());
        const int y = c.getY() + 40 + 10 + 5 + 14 + 18 + 8 + (int) tl.getHeight() + 4 + 14 + 1 + 14 + 18 + 4;
        website.setBounds (c.getX() + 62, y, 280, 18);
    }
    else
    {
        back.setBounds (c.getX(), c.getBottom() - 30, 90, 30);
        auto t = c.withTrimmedTop (46).withTrimmedBottom (44);
        scroller.setBounds (t);
        const auto p = Settings::get().palette();
        juce::AttributedString s;
        for (auto& sec : howItWorks)
        {
            s.append (juce::String (sec[0]) + "\n", font (14.0f, true), p.limeText);
            s.append (juce::String (sec[1]) + "\n\n", font (12.0f), p.t2);
        }
        s.setLineSpacing (3.0f);
        howText.layout.createLayout (s, (float) t.getWidth() - 14);
        howText.setSize (t.getWidth() - 14, (int) std::ceil (howText.layout.getHeight()) + 10);
    }
}

void AboutPanel::paint (juce::Graphics& g)
{
    Overlay::paint (g);
    const auto p = Settings::get().palette();
    auto c = card().reduced (24, 20);
    if (page == 1)
    {
        g.setColour (p.t1); g.setFont (font (22.0f, true));
        g.drawText ("How it works", c.removeFromTop (30), juce::Justification::centredLeft);
        g.setColour (p.t3); g.setFont (font (11.0f));
        g.drawText ("From a seismometer to your reverb tail", c.removeFromTop (16), juce::Justification::centredLeft);
        return;
    }
    g.setColour (p.t1); g.setFont (font (32.0f, true).withExtraKerningFactor (0.08f));
    g.drawText ("AFTERSHOCK", c.removeFromTop (40), juce::Justification::centredLeft);
    c.removeFromTop (10);
    auto stripe = c.removeFromTop (5).toFloat();
    const Accent cols[] = { teal, orange, lav, rose, yellow, peri, green, grey, pink };
    const float w = (stripe.getWidth() - 8 * 5) / 9;
    for (int i = 0; i < 9; ++i) { g.setColour (p[cols[i]]); g.fillRoundedRectangle (stripe.getX() + (float) i * (w + 5), stripe.getY(), w, 5, 2.5f); }
    c.removeFromTop (14);
    g.setColour (p[teal]); g.setFont (font (12.0f)); g.drawText ("Version " JucePlugin_VersionString, c.removeFromTop (18), juce::Justification::centredLeft);
    c.removeFromTop (8);
    juce::AttributedString d;
    d.append ("Aftershock is a reverb made from real earthquake data. Every impulse response is a seismogram recorded by a real station, squeezed in time so the way the Earth rings and fades becomes your reverb tail.", font (12.0f), p.t2);
    d.setLineSpacing (4.0f);
    juce::TextLayout tl; tl.createLayout (d, (float) c.getWidth());
    tl.draw (g, c.removeFromTop ((int) tl.getHeight() + 4).toFloat());
    c.removeFromTop (14);
    g.setColour (p.border()); g.fillRect (c.removeFromTop (1)); c.removeFromTop (14);
    g.setColour (p.limeText); g.setFont (font (12.5f)); g.drawText ("Made by Sofia Vallejo Budziszewski", c.removeFromTop (18), juce::Justification::centredLeft);
    c.removeFromTop (4);
    g.setColour (p.t3); g.setFont (font (12.0f)); g.drawText ("Website", c.removeFromTop (18), juce::Justification::centredLeft);
    c.removeFromTop (14);
    g.setColour (p.t3); g.setFont (font (11.0f));
    for (auto l : { "Seismograms: EarthScope, ORFEUS and GEOFON (FDSN networks)", "Earthquake origins: USGS ComCat  ·  Coastlines: Natural Earth",
                    "Built with JUCE  ·  Font: Pixelify Sans (SIL Open Font License)" })
        g.drawText (juce::CharPointer_UTF8 (l), c.removeFromTop (18), juce::Justification::centredLeft);
}

// ---------------------------------------------------------------- preset browser
struct PresetRow : juce::Component
{
    PresetRow (PresetBrowser& b) : owner (b)
    {
        setStyle (star, "ghost");
        star.onClick = [this] { if (juce::isPositiveAndBelow (row, (int) owner.shown.size())) owner.proc.presets.toggleFavourite (owner.shown[(size_t) row]); };
        addAndMakeVisible (star);
        setInterceptsMouseClicks (false, true);
    }
    void set (int r, bool sel)
    {
        row = r; selected = sel;
        if (juce::isPositiveAndBelow (row, (int) owner.shown.size()))
        {
            const auto& pr = owner.shown[(size_t) row];
            const bool fav = owner.proc.presets.isFavourite (pr);
            star.setButtonText (fav ? juce::String (juce::CharPointer_UTF8 ("\xe2\x98\x85")) : juce::String (juce::CharPointer_UTF8 ("\xe2\x98\x86")));
            star.setTitle (fav ? "Remove " + pr.name + " from favourites" : "Add " + pr.name + " to favourites");
            star.getProperties().set ("textColour", (int) yellow);
        }
        repaint();
    }
    void resized() override { star.setBounds (getLocalBounds().removeFromRight (40).reduced (4, 8)); }
    void paint (juce::Graphics& g) override
    {
        const auto p = Settings::get().palette();
        if (! juce::isPositiveAndBelow (row, (int) owner.shown.size())) return;
        const auto& pr = owner.shown[(size_t) row];
        auto r = getLocalBounds().toFloat().reduced (0, 2);
        const bool current = pr.name == owner.proc.presets.currentName();
        if (selected || current)
        {
            g.setColour (p.ink.withAlpha (0.06f)); g.fillRoundedRectangle (r, 10);
            g.setColour (p[lime].withAlpha (0.5f)); g.drawRoundedRectangle (r.reduced (0.5f), 10, 1);
        }
        r = r.reduced (12, 6);
        g.setColour (current ? p.limeText : p.t1); g.setFont (font (13.0f));
        g.drawText (pr.name, r.removeFromTop (r.getHeight() * 0.55f), juce::Justification::bottomLeft);
        const int ci = owner.proc.presets.categories().indexOf (pr.category);
        const Accent cols[] = { pink, orange, green, lav, teal, rose, yellow, peri };
        g.setColour (p[cols[juce::jlimit (0, 7, ci < 0 ? 7 : ci)]]); g.fillEllipse (r.getX(), r.getY() + 5, 6, 6);
        g.setColour (p.t3); g.setFont (font (10.5f));
        auto meta = owner.proc.presets.describe (pr);
        if (! pr.factory) meta = "user  " + meta;
        g.drawText (meta, r.withTrimmedLeft (12), juce::Justification::topLeft);
    }
    PresetBrowser& owner;
    juce::TextButton star;
    int row = -1;
    bool selected = false;
};

PresetBrowser::PresetBrowser (AftershockProcessor& p) : Overlay ("Presets", 700, 520), proc (p)
{
    showHeading = false;
    search.setTextToShowWhenEmpty ("Search " + juce::String ((int) proc.presets.all().size()) + " presets, earthquakes, places", Settings::get().palette().t3);
    search.setFont (font (12.5f));
    search.setIndents (12, 8);
    search.setTitle ("Search presets");
    search.addListener (this);
    addAndMakeVisible (search);
    for (juce::String t : { juce::String ("Factory"), juce::String ("User"), juce::String (juce::CharPointer_UTF8 ("\xe2\x98\x85 Favourites")) })
    {
        auto& b = tabs.add (juce::String (t));
        const int idx = (int) tabs.buttons.size() - 1;
        b.onClick = [this, idx] { tab = idx; for (int i = 0; i < (int) tabs.buttons.size(); ++i) tabs.buttons[(size_t) i]->setToggleState (i == tab, juce::dontSendNotification); refresh(); };
    }
    tabs.buttons[0]->setToggleState (true, juce::dontSendNotification);
    addAndMakeVisible (tabs);

    juce::StringArray cats { "All" }; cats.addArray (proc.presets.categories());
    for (auto& c : cats)
    {
        auto b = std::make_unique<juce::TextButton> (c);
        setStyle (*b, "seg", pink);
        b->getProperties().set ("just", (int) juce::Justification::centredLeft);
        b->onClick = [this, c] { category = c; refresh(); };
        addAndMakeVisible (*b);
        catButtons.push_back (std::move (b));
    }
    list.setRowHeight (52);
    list.setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
    list.setTitle ("Presets");
    addAndMakeVisible (list);

    for (auto* b : { &saveAs, &importB, &folderB, &deleteB }) { setStyle (*b, b == &saveAs ? "accent" : "plain", lav); addAndMakeVisible (*b); }
    saveAs.onClick = [this] { openSaveForm(); };
    importB.onClick = [this] { importPreset(); };
    folderB.onClick = [] { PresetManager::userFolder().revealToUser(); };
    deleteB.onClick = [this]
    {
        if (juce::isPositiveAndBelow (selected, (int) shown.size())) { auto r = proc.presets.deleteUser (shown[(size_t) selected]); if (r.failed()) error.setText (r.getErrorMessage(), juce::dontSendNotification); }
    };

    for (auto* e : { &nameEd, &aboutEd }) { e->setFont (font (12.5f)); e->setIndents (10, 7); addChildComponent (*e); }
    nameEd.setTitle ("Preset name"); nameEd.setTextToShowWhenEmpty ("Name", Settings::get().palette().t3);
    aboutEd.setTitle ("Description"); aboutEd.setTextToShowWhenEmpty ("What is it good for? (optional)", Settings::get().palette().t3);
    aboutEd.setMultiLine (true, true);
    catBox.addItemList (proc.presets.categories(), 1);
    catBox.setTitle ("Category");
    addChildComponent (catBox);
    for (auto* b : { &confirm, &cancel }) { setStyle (*b, b == &confirm ? "accent" : "plain", lime); addChildComponent (*b); }
    confirm.onClick = [this]
    {
        auto r = proc.presets.saveUser (nameEd.getText(), catBox.getText(), aboutEd.getText());
        if (r.failed()) { error.setText (r.getErrorMessage(), juce::dontSendNotification); return; }
        saving = false; tab = 1; for (int i = 0; i < 3; ++i) tabs.buttons[(size_t) i]->setToggleState (i == 1, juce::dontSendNotification);
        category = "All"; refresh(); resized();
    };
    cancel.onClick = [this] { saving = false; resized(); refresh(); };
    error.setFont (font (11.0f));
    error.setColour (juce::Label::textColourId, Settings::get().palette()[rose]);
    addAndMakeVisible (error);
    refresh();
}

void PresetBrowser::openSaveForm()
{
    saving = true;
    nameEd.setText (proc.presets.currentName() == "Init" ? juce::String() : proc.presets.currentName() + " (mine)");
    aboutEd.clear();
    const int ci = proc.presets.categories().indexOf (proc.presets.currentCategory());
    catBox.setSelectedItemIndex (ci >= 0 ? ci : proc.presets.categories().size() - 1, juce::dontSendNotification);
    error.setText ({}, juce::dontSendNotification);
    resized(); repaint();
    nameEd.grabKeyboardFocus();
}

void PresetBrowser::importPreset()
{
    chooser = std::make_unique<juce::FileChooser> ("Import an Aftershock preset", juce::File::getSpecialLocation (juce::File::userHomeDirectory), "*.aftershock");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this] (const juce::FileChooser& fc)
    {
        if (fc.getResult() == juce::File()) return;
        auto r = proc.presets.importFile (fc.getResult());
        error.setText (r.failed() ? r.getErrorMessage() : juce::String(), juce::dontSendNotification);
        tab = 1; refresh();
    });
}

void PresetBrowser::refresh()
{
    shown.clear();
    const auto q = search.getText().trim().toLowerCase();
    for (auto& pr : proc.presets.all())
    {
        if (tab == 0 && ! pr.factory) continue;
        if (tab == 1 && pr.factory) continue;
        if (tab == 2 && ! proc.presets.isFavourite (pr)) continue;
        if (category != "All" && pr.category != category) continue;
        if (q.isNotEmpty())
        {
            const auto hay = (pr.name + " " + pr.category + " " + pr.about + " " + proc.presets.describe (pr)).toLowerCase();
            bool all = true;
            for (auto w : juce::StringArray::fromTokens (q, " ", "")) all &= hay.contains (w);
            if (! all) continue;
        }
        shown.push_back (pr);
    }
    selected = -1;
    for (int i = 0; i < (int) shown.size(); ++i) if (shown[(size_t) i].name == proc.presets.currentName()) selected = i;
    for (auto& b : catButtons) b->setToggleState (b->getButtonText() == category, juce::dontSendNotification);
    deleteB.setEnabled (juce::isPositiveAndBelow (selected, (int) shown.size()) && ! shown[(size_t) selected].factory);
    list.updateContent();
    list.repaint();
    repaint();
}

juce::Component* PresetBrowser::refreshComponentForRow (int row, bool sel, juce::Component* existing)
{
    auto* r = dynamic_cast<PresetRow*> (existing);
    if (r == nullptr) { delete existing; r = new PresetRow (*this); }
    r->set (row, sel || row == selected);
    return r;
}

void PresetBrowser::loadRow (int row)
{
    if (! juce::isPositiveAndBelow (row, (int) shown.size())) return;
    proc.presets.load (shown[(size_t) row]);
    selected = row;
    deleteB.setEnabled (! shown[(size_t) row].factory);
    list.updateContent(); list.repaint(); repaint();
}

void PresetBrowser::listBoxItemClicked (int row, const juce::MouseEvent&) { loadRow (row); }
void PresetBrowser::returnKeyPressed (int row) { loadRow (row); }

void PresetBrowser::layout (juce::Rectangle<int> c)
{
    auto top = c.removeFromTop (34);
    closeButton.setBounds (top.removeFromRight (30).withSizeKeepingCentre (28, 28)); top.removeFromRight (10);
    tabs.setBounds (top.removeFromRight (290)); top.removeFromRight (10);
    search.setBounds (top);
    c.removeFromTop (12);
    footArea = c.removeFromBottom (64);
    auto f = footArea.reduced (0, 14);
    for (auto* b : { &deleteB, &folderB, &importB, &saveAs }) { b->setBounds (f.removeFromRight (b == &saveAs ? 92 : 72)); f.removeFromRight (8); }
    error.setBounds (footArea.removeFromBottom (14).translated (0, 12));
    c.removeFromBottom (4);
    sideArea = c.removeFromLeft (180);
    auto s = sideArea.reduced (0, 6);
    for (auto& b : catButtons) b->setBounds (s.removeFromTop (30).reduced (0, 1));
    c.removeFromLeft (14);
    listArea = c;

    for (auto* comp : std::initializer_list<juce::Component*> { &nameEd, &aboutEd, &catBox, &confirm, &cancel }) comp->setVisible (saving);
    list.setVisible (! saving);
    if (saving)
    {
        auto r = listArea.reduced (6, 30);
        nameEd.setBounds (r.removeFromTop (34)); r.removeFromTop (10);
        catBox.setBounds (r.removeFromTop (30)); r.removeFromTop (10);
        aboutEd.setBounds (r.removeFromTop (90)); r.removeFromTop (14);
        auto br = r.removeFromTop (32);
        confirm.setBounds (br.removeFromRight (90)); br.removeFromRight (8); cancel.setBounds (br.removeFromRight (90));
    }
    else list.setBounds (listArea.withTrimmedTop (22));
}

void PresetBrowser::paint (juce::Graphics& g)
{
    Overlay::paint (g);
    const auto p = Settings::get().palette();
    g.setColour (p.border());
    g.fillRect (sideArea.getRight() + 6, sideArea.getY(), 1, sideArea.getHeight());
    g.fillRect (card().getX(), footArea.getY() - 2, card().getWidth(), 1);
    const Accent cols[] = { grey, pink, orange, green, lav, teal, rose, yellow, peri };
    for (int i = 0; i < (int) catButtons.size(); ++i)
    {
        auto b = catButtons[(size_t) i]->getBounds().toFloat();
        int n = 0;
        for (auto& pr : proc.presets.all()) n += (i == 0 || pr.category == catButtons[(size_t) i]->getButtonText()) ? 1 : 0;
        g.setColour (p.t3); g.setFont (font (11.0f));
        g.drawText (juce::String (n), b.withTrimmedRight (10), juce::Justification::centredRight);
        g.setColour (p[cols[juce::jmin (8, i)]]); g.fillEllipse (b.getX() - 2, b.getCentreY() - 3, 6, 6);
    }
    if (! saving)
    {
        drawGroupTitle (g, listArea.removeFromTop (18).toFloat(), pink, (category == "All" ? juce::String ("ALL PRESETS") : category.toUpperCase()),
                        juce::String ((int) shown.size()) + (shown.size() == 1 ? " PRESET" : " PRESETS"));
        if (shown.empty())
        {
            g.setColour (p.t3); g.setFont (font (12.0f));
            g.drawText (tab == 2 ? juce::String (juce::CharPointer_UTF8 ("No favourites yet: click the \xe2\x98\x86 next to a preset.")) : juce::String (juce::CharPointer_UTF8 ("Nothing here yet.")), listArea, juce::Justification::centred);
        }
    }
    else drawGroupTitle (g, listArea.removeFromTop (18).toFloat(), lav, "SAVE CURRENT SETTINGS AS A PRESET");
    // description of the selected preset
    auto d = footArea.reduced (0, 10).withTrimmedRight (360);
    if (juce::isPositiveAndBelow (selected, (int) shown.size()) && ! saving)
    {
        g.setColour (p.t2); g.setFont (font (11.5f));
        g.drawFittedText (shown[(size_t) selected].about, d, juce::Justification::centredLeft, 3, 1.0f);
    }
}
} // namespace ui
