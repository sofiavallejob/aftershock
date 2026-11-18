// Aftershock, a reverb made from earthquakes. (C) 2026 Sofia Vallejo Budziszewski, AGPL-3.0.
// Free to port, by humans. Bring your own ears. See PORTING.md.

#include "PluginEditor.h"

using namespace ui;

namespace
{
const juce::StringArray stationKeys { "near", "regional", "tele" };

juce::String grit (float n)
{
    return n < 0.15f ? "Smooth" : n < 0.4f ? "Textured" : n < 0.75f ? "Raw" : "Pure data";
}

void setTip (juce::Component& c, const juce::String& title, const juce::String& help)
{
    if (auto* t = dynamic_cast<juce::SettableTooltipClient*> (&c)) t->setTooltip (title + "\n" + help);
    c.setTitle (title);
    c.setDescription (help);
    c.setHelpText (help);
}
}

// ================================================================= root
EditorRoot::EditorRoot (AftershockProcessor& p)
    : proc (p), globe (p), map (p), signal (p), waves (p), finger (p), presetBrowser (p)
{
    setTitle ("Aftershock");
    buildToolbar();

    // ---- data views
    for (auto* v : std::initializer_list<DataView*> { &globe, &map, &signal, &waves, &finger })
    {
        addChildComponent (*v);
        v->onPickEvent = [this] (int i) { setParam ("event", (float) i); };
    }
    addChildComponent (irView);
    setTip (globe, "Globe", "All 35 earthquakes, coloured by tectonic setting. Click one to use its recording; drag to turn the Earth. The dashed line runs to the station that recorded it.");
    setTip (map, "Earthquake map", "Click any earthquake to use its recording. Colours are tectonic settings; dot size is magnitude.");
    setTip (signal, "Seismogram", "The real ground motion recorded at the station. The lime part, from the line onwards, is squeezed in time to become your reverb tail.");
    setTip (waves, "Ground velocity", "The three components of the recording: vertical (Z), north (N) and east (E). North becomes the left channel, east the right.");
    setTip (irView, "Impulse response", "The reverb this recording makes with your settings, as a spectrogram: time to the right, pitch upwards.");
    setTip (finger, "Coda fingerprint", "How much energy each seismic band carries (top) and how slowly it fades (seismic RT60, bottom). Lighter dots are rougher measurements.");

    const char* names[] = { "Near", "Regional", "Far" };
    for (int i = 0; i < 3; ++i)
    {
        auto& b = stations.add (names[i], "-");
        b.onClick = [this, i] { setParam ("station", (float) i); };
        b.setTooltip (juce::String (names[i]) + "\n" + juce::String (i == 0 ? "A station close to the earthquake: bright, busy early arrivals."
                                                                     : i == 1 ? "A station a few hundred to 1,600 km away: darker, smoother."
                                                                              : "A station on another continent: the Earth has eaten the highs. Dark and long."));
    }
    addChildComponent (stations);

    for (auto* b : { &prevEvent, &nextEvent, &allEvents }) { setStyle (*b, "plain"); b->getProperties().set ("radius", 16.0); addChildComponent (*b); }
    prevEvent.onClick = [this] { const int n = (int) proc.pack.events.size(); setParam ("event", (float) ((proc.currentEventIndex() + n - 1) % n)); };
    nextEvent.onClick = [this] { const int n = (int) proc.pack.events.size(); setParam ("event", (float) ((proc.currentEventIndex() + 1) % n)); };
    setTip (prevEvent, "Previous earthquake", "Step to the previous earthquake.");
    setTip (nextEvent, "Next earthquake", "Step to the next earthquake.");
    allEvents.setButtonText ("All " + juce::String ((int) proc.pack.events.size()) + juce::String (juce::CharPointer_UTF8 (" \xe2\x96\xbe")));
    setTip (allEvents, "All earthquakes", "Pick any earthquake from the list, grouped by tectonic setting.");
    allEvents.onClick = [this]
    {
        juce::PopupMenu m;
        for (auto& [key, label] : proc.pack.settings)
        {
            juce::PopupMenu sub;
            for (int i = 0; i < (int) proc.pack.events.size(); ++i)
                if (proc.pack.events[(size_t) i].setting == key)
                    sub.addItem (i + 1, eventLine (proc.pack.events[(size_t) i]), true, i == proc.currentEventIndex());
            m.addSubMenu (label, sub);
        }
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&allEvents), [this] (int r) { if (r > 0) setParam ("event", (float) (r - 1)); });
    };

    {
        int id = 1;
        for (auto& [key, label] : proc.pack.settings)
        {
            eventBox.addSectionHeading (label);
            for (int i = 0; i < (int) proc.pack.events.size(); ++i)
                if (proc.pack.events[(size_t) i].setting == key) eventBox.addItem (eventLine (proc.pack.events[(size_t) i]), i + 1);
            ++id;
        }
        eventBox.onChange = [this] { if (eventBox.getSelectedId() > 0) setParam ("event", (float) (eventBox.getSelectedId() - 1)); };
        setTip (eventBox, "Earthquake", "Choose the earthquake whose recording becomes the reverb.");
        addChildComponent (eventBox);
    }

    // ---- knobs: simple view
    simpleKnobs["length"]   = &knob ("length", "Length", lime, "How long the tail is. The chosen part of the recording is squeezed into this time.");
    simpleKnobs["predelay"] = &knob ("predelay", "Pre-delay", orange, "A gap before the tail starts. Keeps vocals and drums clear of their reverb.");
    auto& bright = knob ("spectrum", "Brightness", teal, "How much of the earthquake's real spectrum you hear. Left keeps the recording as it is: far stations sound dark, because the Earth eats high frequencies. Right balances all bands.", true);
    bright.setValueText ([] (float n) { return juce::String (juce::roundToInt ((1.0f - n) * 100.0f)) + " %"; });
    simpleKnobs["spectrum"] = &bright;
    auto& ch = knob ("grit", "Character", lav, "Smooth uses only the measured band envelopes. Turning it up blends in the raw seismogram played faster: rough, gritty, real ground motion.");
    ch.setValueText ([] (float n) { return grit (n); });
    simpleKnobs["grit"] = &ch;
    simpleKnobs["width"] = &knob ("width", "Width", peri, "Stereo width of the tail. North ground motion is the left channel, east the right.");
    simpleKnobs["wet"]   = &knob ("wet", "Dry / Wet", pink, "How loud the reverb is next to your dry sound.");

    // ---- knobs: advanced view
    advKnobs["length"]   = &knob ("length", "Length", lime, "How long the tail is. The chosen part of the recording is squeezed into this time.");
    advKnobs["damping"]  = &knob ("damping", "Damping", teal, "Extra high-frequency decay on top of the data, like a softer room.");
    advKnobs["spectrum"] = &knob ("spectrum", "Spectrum", peri, "How much of the recording's band levels survive. 100 % is the data as recorded: far stations are very dark. 0 % makes every band equally loud.");
    advKnobs["contrast"] = &knob ("contrast", "Contrast", lav, "How differently the bands decay. 1x is the data, 0 makes them decay alike, above 1 exaggerates the difference between regions.");
    advKnobs["grit"]     = &knob ("grit", "Grit", orange, "Blends in the raw seismogram played faster: plain audification. Pitch and length are tied.");
    advKnobs["early"]    = &knob ("early", "Early arrivals", teal, "Level of the early reflections, built from the real bursts between the P and S waves.");
    advKnobs["spread"]   = &knob ("spread", "Early spread", teal, "How far apart in time the early arrivals are spread.");
    advKnobs["lowcut"]   = &knob ("lowcut", "Low cut", teal, "Removes low end from the tail only (24 dB per octave).");
    advKnobs["tilt"]     = &knob ("tilt", "Tilt", teal, "Tilts the tail darker or brighter around 1 kHz.");
    advKnobs["mod"]      = &knob ("mod", "Modulation", lav, "Slow pitch movement in the tail, for width and shimmer.");
    advKnobs["width"]    = &knob ("width", "Width", peri, "Stereo width of the tail.");
    advKnobs["duck"]     = &knob ("duck", "Ducking", pink, "Pushes the tail down while you play and lets it bloom in the gaps.");
    advKnobs["predelay"] = &knob ("predelay", "Pre-delay", orange, "A gap before the tail starts.");
    advKnobs["dry"]      = &knob ("dry", "Dry", grey, "Level of your dry signal.");
    advKnobs["wet"]      = &knob ("wet", "Wet", pink, "Level of the reverb.");
    advKnobs["grit"]->setValueText (nullptr);

    windowBox.addItemList ({ "Full recording", "From the P wave", "From the S wave", "Surface waves" }, 1);
    channelBox.addItemList ({ "North | East", "Vertical", "All three" }, 1);
    windowAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "window", windowBox);
    channelAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "channels", channelBox);
    setTip (windowBox, "Window", "Which part of the recording becomes the reverb: all of it, or starting at the P, S or surface waves.");
    setTip (channelBox, "Channels", "Which ground motion feeds left and right: north and east, the vertical, or all three mixed.");
    reverseAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "reverse", reverseToggle);
    freezeAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "freeze", freezeToggle);
    setTip (reverseToggle, "Reverse", "Plays the impulse response backwards: the tail swells into each note.");
    setTip (freezeToggle, "Freeze", "Holds the current tail forever. Play something, then freeze it.");
    for (auto* c : std::initializer_list<juce::Component*> { &windowBox, &channelBox, &reverseToggle, &freezeToggle }) addChildComponent (*c);

    gain.setSliderStyle (juce::Slider::LinearBar);
    gain.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    gainAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, "output", gain);
    gain.setDoubleClickReturnValue (true, 0.0);
    gain.getProperties().set ("gain", true);
    setTip (gain, "Output gain", "Overall output level. Drag sideways, double-click for 0 dB.");
    gain.setWantsKeyboardFocus (true);
    addAndMakeVisible (gain);
    setTip (inMeter, "Input level", "Level going into Aftershock.");
    setTip (outMeter, "Output level", "Level coming out, after dry, wet and output gain.");
    addAndMakeVisible (inMeter); addAndMakeVisible (outMeter);

    for (auto* o : std::initializer_list<Overlay*> { &settingsPanel, &aboutPanel, &presetBrowser }) addChildComponent (*o);
    // MSVC needs the safe pointer made outside the inner lambda
    proc.presets.onChange = [self = juce::Component::SafePointer<EditorRoot> (this)]
    {
        juce::MessageManager::callAsync ([self] { if (self != nullptr) { self->refreshFromProcessor (true); self->presetBrowser.refresh(); } });
    };

    abSlots[0] = proc.apvts.copyState();
    setAdvanced (Settings::get().advanced());
    refreshFromProcessor (true);
}

EditorRoot::~EditorRoot() { proc.presets.onChange = nullptr; }

Knob& EditorRoot::knob (const juce::String& id, const juce::String& title, Accent a, const juce::String& help, bool invert)
{
    knobs.push_back (std::make_unique<Knob> (proc.apvts, id, title, a, invert));
    auto& k = *knobs.back();
    k.setHelp (help);
    k.onUserChange = [this] (const juce::String& t) { announce (t); };
    addChildComponent (k);
    return k;
}

juce::String EditorRoot::eventLine (const aftershock::Event& e) const
{
    return e.name + "  M" + juce::String (e.mag, 1) + "  " + e.time.substring (0, 4);
}

void EditorRoot::announce (const juce::String& t)
{
    if (Settings::get().announce())
        juce::AccessibilityHandler::postAnnouncement (t, juce::AccessibilityHandler::AnnouncementPriority::medium);
}

void EditorRoot::setParam (const juce::String& id, float v)
{
    if (auto* p = proc.apvts.getParameter (id))
    {
        proc.undoManager.beginNewTransaction();
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (v));
        p->endChangeGesture();
    }
}

// ---------------------------------------------------------------- toolbar
void EditorRoot::buildToolbar()
{
    setStyle (logo, "ghost");
    logo.getProperties().set ("fontSize", 30.0);
    logo.getProperties().set ("just", (int) juce::Justification::centredLeft);
    setTip (logo, "About Aftershock", "Version, credits and how it works.");
    logo.onClick = [this] { aboutPanel.showPage (0); showOverlay (aboutPanel); };

    presetButton.getProperties().set ("just", (int) juce::Justification::centredLeft);
    presetButton.onClick = [this] { presetBrowser.refresh(); showOverlay (presetBrowser); };
    setTip (presetButton, "Preset", "Open the preset browser: 28 factory presets for vocals, drums, guitar, synths, mixing and sound design, plus your own.");
    prevPreset.onClick = [this] { proc.presets.step (-1); };
    nextPreset.onClick = [this] { proc.presets.step (1); };
    setTip (prevPreset, "Previous preset", "Load the previous preset.");
    setTip (nextPreset, "Next preset", "Load the next preset.");
    setStyle (saveButton, "accent", lav);
    saveButton.onClick = [this] { presetBrowser.refresh(); showOverlay (presetBrowser); presetBrowser.openSaveForm(); };
    setTip (saveButton, "Save preset", "Save the current settings as your own preset.");
    loadButton.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Load an Aftershock preset", PresetManager::userFolder(), "*.aftershock");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this] (const juce::FileChooser& fc)
        {
            const auto f = fc.getResult();
            if (f == juce::File()) return;
            Preset pr;
            if (PresetManager::fromJson (juce::JSON::parse (f), pr)) { pr.factory = false; pr.file = f; proc.presets.load (pr); }
        });
    };
    setTip (loadButton, "Load preset file", "Load a .aftershock preset file from anywhere on your computer.");

    for (auto* b : { &aButton, &bButton }) setStyle (*b, "toggle", orange);
    aButton.onClick = [this] { switchAB (0); };
    bButton.onClick = [this] { switchAB (1); };
    setTip (aButton, "Setting A", "Compare two settings. Changes you make go to the slot that is lit.");
    setTip (bButton, "Setting B", "Compare two settings. The first time you press B it starts as a copy of A.");
    undoButton.onClick = [this] { proc.undoManager.undo(); };
    redoButton.onClick = [this] { proc.undoManager.redo(); };
    setTip (undoButton, "Undo", "Undo the last change (Cmd or Ctrl + Z).");
    setTip (redoButton, "Redo", "Redo (Shift + Cmd or Ctrl + Z).");

    setStyle (helpButton, "toggle", teal);
    helpButton.onClick = [] { Settings::get().set ("help", ! Settings::get().help()); };
    setTip (helpButton, "Help on hover", "Shows a bubble like this one when you hover over a control. Click to turn it off.");
    themeButton.onClick = [] { auto& s = Settings::get(); s.set ("theme", s.isDark() ? (int) Settings::light : (int) Settings::dark); };
    setTip (themeButton, "Light or dark", "Switch between the light and dark look. More options in Settings.");
    settingsButton.onClick = [this] { showOverlay (settingsPanel); };
    setTip (settingsButton, "Settings", "Theme, window size, text size, colour vision, high contrast, reduced motion and screen reader options.");
    setStyle (viewButton, "outline", lime);
    viewButton.onClick = [this] { setAdvanced (! advanced); };
    setTip (viewButton, "Simple or advanced", "Switch between the simple view and the advanced view with every parameter.");

    setStyle (bypassButton, "toggle", lime);
    bypassButton.onClick = [this] { const bool by = proc.apvts.getRawParameterValue ("bypass")->load() > 0.5f; setParam ("bypass", by ? 0.0f : 1.0f); };
    setTip (bypassButton, "Bypass", "Lit means Aftershock is on. Click to hear your sound without it.");

    for (auto* b : { &logo, &presetButton, &prevPreset, &nextPreset, &saveButton, &loadButton, &aButton, &bButton, &undoButton, &redoButton,
                     &helpButton, &themeButton, &settingsButton, &viewButton, &bypassButton })
    {
        if (! b->getProperties().contains ("style")) setStyle (*b, "plain");
        addAndMakeVisible (*b);
    }
}

void EditorRoot::switchAB (int slot)
{
    if (slot == abSlot) return;
    abSlots[abSlot] = proc.apvts.copyState();
    abSlot = slot;
    if (abSlots[slot].isValid()) proc.apvts.replaceState (abSlots[slot].createCopy());
    else abSlots[slot] = proc.apvts.copyState();
    proc.requestIRPublic();
    refreshFromProcessor (true);
    announce (slot == 0 ? "Setting A" : "Setting B");
}

void EditorRoot::layoutToolbar (juce::Rectangle<int> r)
{
    toolbarLabels.clear();
    logo.setBounds (r.removeFromLeft (220).withTrimmedTop (8).withTrimmedLeft (-6));
    auto group = [&] (const juce::String& label, std::initializer_list<std::pair<juce::Button*, int>> items)
    {
        int w = 0; for (auto& [b, bw] : items) w += bw + 4;
        auto g = r.removeFromLeft (w - 4);
        toolbarLabels.push_back ({ g.removeFromTop (14).withWidth (juce::jmax (70, g.getWidth())), label });
        g.removeFromTop (4);
        for (auto& [b, bw] : items) { b->setBounds (g.removeFromLeft (bw).withHeight (26)); g.removeFromLeft (4); }
        r.removeFromLeft (16);
    };
    r = r.withTrimmedTop (4);
    r.removeFromLeft (juce::jmax (0, r.getWidth() - (advanced ? 790 : 790)));
    group ("PRESET", { { &prevPreset, 24 }, { &presetButton, 200 }, { &nextPreset, 24 }, { &saveButton, 50 }, { &loadButton, 50 } });
    group ("COMPARE", { { &aButton, 26 }, { &bButton, 26 } });
    group ("HISTORY", { { &undoButton, 26 }, { &redoButton, 26 } });
    group ("VIEW", { { &helpButton, 26 }, { &themeButton, 26 }, { &settingsButton, 26 }, { &viewButton, 86 } });
    group ("BYPASS", { { &bypassButton, 26 } });
}

// ---------------------------------------------------------------- views
void EditorRoot::setAdvanced (bool a)
{
    advanced = a;
    Settings::get().setQuietly ("advanced", a);
    viewButton.setButtonText (a ? "Simple" : "Advanced");
    globe.setVisible (! a); signal.setVisible (! a); prevEvent.setVisible (! a); nextEvent.setVisible (! a); allEvents.setVisible (! a);
    map.setVisible (a); waves.setVisible (a); irView.setVisible (a); finger.setVisible (a); eventBox.setVisible (a);
    for (auto* c : std::initializer_list<juce::Component*> { &windowBox, &channelBox, &reverseToggle, &freezeToggle }) c->setVisible (a);
    stations.setVisible (true);
    for (auto& k : knobs) k->setVisible (false);
    for (auto& [id, k] : (a ? advKnobs : simpleKnobs)) k->setVisible (true);
    if (getWidth() > 0) resized();
    globe.updateTimer();
    if (onViewChanged) onViewChanged();
}

void EditorRoot::resized()
{
    panels.clear(); sectionLabels.clear();
    auto r = getLocalBounds().reduced (22, 18);
    layoutToolbar (r.removeFromTop (46));
    footer = r.removeFromBottom (16);
    r.removeFromBottom (8);
    if (advanced) layoutAdvanced (r); else layoutSimple (r);
    for (auto* o : std::initializer_list<Overlay*> { &settingsPanel, &aboutPanel, &presetBrowser }) o->setBounds (getLocalBounds());
}

void EditorRoot::layoutSimple (juce::Rectangle<int> r)
{
    r.removeFromTop (12);
    sectionLabels.push_back ({ r.removeFromTop (14), juce::String (juce::CharPointer_UTF8 ("THE RECORDING \xc2\xb7 WHAT THE EARTH DID")) });
    r.removeFromTop (8);
    auto data = r.removeFromTop (338);
    auto left = data.removeFromLeft (300); data.removeFromLeft (14);
    panels.push_back ({ left, peri, peri, 0.14f, {}, {}, peri });
    panels.push_back ({ data, lav, teal, 0.15f, {}, {}, lav });
    auto l = left.reduced (16, 14);
    auto st = l.removeFromBottom (50);
    stations.setBounds (st);
    l.removeFromBottom (24);
    sectionLabels.push_back ({ juce::Rectangle<int> (st.getX(), st.getY() - 20, 200, 14), "LISTEN FROM" });
    globe.setBounds (l.withSizeKeepingCentre (juce::jmin (l.getWidth(), l.getHeight()), juce::jmin (l.getWidth(), l.getHeight())));

    auto d = data.reduced (18, 16);
    auto head = d.removeFromTop (52);
    prevEvent.setBounds (head.removeFromLeft (32).withSizeKeepingCentre (32, 32));
    nextEvent.setBounds (head.removeFromRight (32).withSizeKeepingCentre (32, 32));
    head.removeFromRight (8);
    allEvents.setBounds (head.removeFromRight (78).withSizeKeepingCentre (78, 28));
    eventText = head.reduced (12, 0);
    d.removeFromTop (12);
    signal.setBounds (d);

    r.removeFromTop (14);
    sectionLabels.push_back ({ r.removeFromTop (14), juce::String (juce::CharPointer_UTF8 ("THE REVERB \xc2\xb7 WHAT YOU DO WITH IT")) });
    r.removeFromTop (8);
    auto out = r.removeFromRight (150); r.removeFromRight (14);
    panels.push_back ({ r, lime, teal, 0.10f, {}, {}, lime });
    panels.push_back ({ out, rose, rose, 0.13f, "OUTPUT", {}, rose });

    auto k = r.reduced (10, 14);
    const int gw = k.getWidth() / 3;
    const char* titles[] = { "TIME", "TONE", "SPACE" };
    const Accent dots[] = { lime, teal, peri };
    const juce::String ids[3][2] = { { "length", "predelay" }, { "spectrum", "grit" }, { "width", "wet" } };
    for (int i = 0; i < 3; ++i)
    {
        auto g = k.removeFromLeft (gw).reduced (8, 0);
        groupLines[i] = g;
        auto t = g.removeFromTop (18);
        panels.push_back ({ t, dots[i], dots[i], -1.0f, titles[i], {}, dots[i] });
        g.removeFromTop (14);
        auto row = g.removeFromTop (130);
        const int kw = row.getWidth() / 2;
        for (int j = 0; j < 2; ++j)
        {
            auto* kn = simpleKnobs[ids[i][j]];
            kn->setSizes (70, 20.0f);
            kn->setBounds (row.removeFromLeft (kw).withSizeKeepingCentre (juce::jmin (kw, 120), 130));
        }
    }
    auto o = out.reduced (14, 14);
    o.removeFromTop (26);
    auto gr = o.removeFromBottom (28);
    gain.setBounds (gr.withSizeKeepingCentre (110, 28));
    o.removeFromBottom (12);
    auto ml = o.removeFromBottom (14);
    juce::ignoreUnused (ml);
    auto meters = o;
    const int mw = 30;
    inMeter.setBounds (meters.getCentreX() - mw - 14, meters.getY(), mw, meters.getHeight() - 4);
    outMeter.setBounds (meters.getCentreX() + 14, meters.getY(), mw, meters.getHeight() - 4);
}

void EditorRoot::layoutAdvanced (juce::Rectangle<int> r)
{
    r.removeFromTop (12);
    sectionLabels.push_back ({ r.removeFromTop (14), juce::String (juce::CharPointer_UTF8 ("THE RECORDING \xc2\xb7 WHAT THE EARTH DID")) });
    r.removeFromTop (8);
    auto row1 = r.removeFromTop (340);
    auto rec = row1.removeFromRight (330); row1.removeFromRight (14);
    panels.push_back ({ row1, teal, peri, 0.12f, "EARTHQUAKES", juce::String ((int) proc.pack.events.size()) + juce::String (juce::CharPointer_UTF8 (" EVENTS \xc2\xb7 ")) + juce::String ((int) proc.pack.settings.size()) + " SETTINGS", teal });
    panels.push_back ({ rec, lime, lav, 0.13f, "RECORDING", {}, lime });
    map.setBounds (row1.reduced (16, 14).withTrimmedTop (26));
    auto rc = rec.reduced (16, 14).withTrimmedTop (26);
    eventBox.setBounds (rc.removeFromTop (32)); rc.removeFromTop (12);
    sectionLabels.push_back ({ rc.removeFromTop (14), "LISTEN FROM" }); rc.removeFromTop (6);
    stations.setBounds (rc.removeFromTop (46)); rc.removeFromTop (12);
    recInfo = rc;

    r.removeFromTop (14);
    auto row2 = r.removeFromTop (196);
    auto fp = row2.removeFromRight (290); row2.removeFromRight (14);
    auto ir = row2.removeFromRight (260); row2.removeFromRight (14);
    panels.push_back ({ row2, lav, teal, 0.13f, "GROUND VELOCITY", {}, lav });
    panels.push_back ({ ir, orange, orange, 0.13f, "IMPULSE RESPONSE", {}, orange });
    panels.push_back ({ fp, green, green, 0.13f, "CODA FINGERPRINT", "0.1 TO 8 HZ", green });
    waves.setBounds (row2.reduced (16, 14).withTrimmedTop (24));
    irView.setBounds (ir.reduced (16, 14).withTrimmedTop (24));
    finger.setBounds (fp.reduced (16, 14).withTrimmedTop (24));

    r.removeFromTop (12);
    sectionLabels.push_back ({ r.removeFromTop (14), juce::String (juce::CharPointer_UTF8 ("THE REVERB \xc2\xb7 WHAT YOU DO WITH IT")) });
    r.removeFromTop (8);
    auto out = r.removeFromRight (140); r.removeFromRight (14);
    const int total = r.getWidth() - 28;
    auto shape = r.removeFromLeft (juce::roundToInt (total * 0.37f)); r.removeFromLeft (14);
    auto space = r.removeFromLeft (juce::roundToInt (total * 0.41f)); r.removeFromLeft (14);
    auto mix = r;
    panels.push_back ({ shape, lime, lime, 0.10f, "SHAPE", "HOW THE RECORDING BECOMES AN IR", lime });
    panels.push_back ({ space, peri, teal, 0.11f, "SPACE", {}, peri });
    panels.push_back ({ mix, pink, pink, 0.12f, "MIX", {}, pink });
    panels.push_back ({ out, rose, rose, 0.13f, "OUTPUT", {}, rose });

    auto place = [this] (juce::Rectangle<int> area, std::initializer_list<const char*> ids)
    {
        const int n = (int) ids.size(), w = area.getWidth() / juce::jmax (1, n);
        for (auto id : ids) { auto* k = advKnobs[id]; k->setSizes (50, 14.0f); k->setBounds (area.removeFromLeft (w).withSizeKeepingCentre (juce::jmin (w + 6, 100), 92)); }
    };
    auto s = shape.reduced (14, 14).withTrimmedTop (28);
    auto ctl = s.removeFromTop (28);
    windowBox.setBounds (ctl.removeFromLeft (140)); ctl.removeFromLeft (8);
    channelBox.setBounds (ctl.removeFromLeft (100)); ctl.removeFromLeft (10);
    reverseToggle.setBounds (ctl.removeFromLeft (110));
    s.removeFromTop (16);
    place (s.removeFromTop (92), { "length", "damping", "spectrum", "contrast", "grit" });

    auto sp = space.reduced (14, 14);
    freezeToggle.setBounds (sp.removeFromTop (18).removeFromRight (92).withSizeKeepingCentre (92, 24));
    sp.removeFromTop (10);
    place (sp.removeFromTop (92), { "early", "spread", "lowcut", "tilt" });
    sp.removeFromTop (6);
    auto r2 = sp.removeFromTop (92);
    place (r2.withTrimmedRight (r2.getWidth() / 4), { "mod", "width", "duck" });

    auto m = mix.reduced (8, 14).withTrimmedTop (28);
    m.removeFromTop (16);
    place (m.removeFromTop (92), { "predelay", "dry", "wet" });

    auto o = out.reduced (14, 14).withTrimmedTop (26);
    gain.setBounds (o.removeFromBottom (28).withSizeKeepingCentre (108, 28));
    o.removeFromBottom (26);
    inMeter.setBounds (o.getCentreX() - 40, o.getY(), 28, o.getHeight());
    outMeter.setBounds (o.getCentreX() + 12, o.getY(), 28, o.getHeight());
}

// ---------------------------------------------------------------- paint
void EditorRoot::paint (juce::Graphics& g)
{
    const auto p = Settings::get().palette();
    auto b = getLocalBounds().toFloat();
    g.fillAll (p.bg);
    if (! p.highContrast)
    {
        juce::ColourGradient g1 (p.glow1.withAlpha (0.22f), b.getWidth() * 0.1f, -b.getHeight() * 0.05f, p.glow1.withAlpha (0.0f), b.getWidth() * 0.85f, b.getHeight() * 0.7f, true);
        g1.addColour (0.4, p.glow1.withAlpha (0.08f));
        g.setGradientFill (g1); g.fillRect (b);
        juce::ColourGradient g2 (p.glow2.withAlpha (0.16f), b.getRight() + 40, b.getBottom() + 40, p.glow2.withAlpha (0.0f), b.getWidth() * 0.3f, b.getHeight() * 0.3f, true);
        g2.addColour (0.45, p.glow2.withAlpha (0.05f));
        g.setGradientFill (g2); g.fillRect (b);
    }

    // toolbar captions
    g.setFont (font (9.5f).withExtraKerningFactor (0.15f)); g.setColour (p.t3);
    for (auto& [r, t] : toolbarLabels) g.drawText (t, r, juce::Justification::bottomLeft);
    if (advanced) { g.setFont (font (10.0f).withExtraKerningFactor (0.15f)); g.drawText ("ADVANCED", logo.getBounds().translated (205, 2).withWidth (90), juce::Justification::centredLeft); }

    for (auto& pr : panels)
    {
        if (pr.s < 0) { drawGroupTitle (g, pr.r.toFloat(), pr.dot, pr.title, pr.note); continue; }
        drawPanel (g, pr.r.toFloat(), pr.a, pr.b, pr.s);
        if (pr.title.isNotEmpty()) drawGroupTitle (g, pr.r.reduced (16, 14).removeFromTop (16).toFloat(), pr.dot, pr.title, pr.note);
    }
    if (! advanced)
        for (int i = 1; i < 3; ++i) { g.setColour (p.border()); g.fillRect (groupLines[i].getX() - 8, groupLines[i].getY(), 1, groupLines[i].getHeight() - 10); }

    g.setFont (font (10.0f).withExtraKerningFactor (0.18f)); g.setColour (p.t3);
    for (auto& [r, t] : sectionLabels) g.drawText (t, r, juce::Justification::centredLeft);

    const int e = sel.event;
    if (juce::isPositiveAndBelow (e, (int) proc.pack.events.size()))
    {
        const auto& ev = proc.pack.events[(size_t) e];
        const auto date = juce::Time (ev.time.substring (0, 4).getIntValue(), ev.time.substring (5, 7).getIntValue() - 1, ev.time.substring (8, 10).getIntValue(),
                                      ev.time.substring (11, 13).getIntValue(), ev.time.substring (14, 16).getIntValue(), 0, 0, false);
        const juce::String meta = "M" + juce::String (ev.mag, 1) + juce::String (juce::CharPointer_UTF8 (" \xc2\xb7 ")) + date.formatted ("%d %b %Y")
                                  + juce::String (juce::CharPointer_UTF8 (" \xc2\xb7 ")) + juce::String (juce::roundToInt (ev.depthKm)) + " km deep";
        const auto setting = proc.pack.settingLabel (ev.setting);
        const auto acc = p[settingAccent (ev.setting)];
        auto chip = [&] (juce::Rectangle<float> at)
        {
            const float w = juce::GlyphArrangement::getStringWidth (font (11.0f), setting) + 26;
            auto c = at.withWidth (w);
            g.setColour (acc.withAlpha (0.18f)); g.fillRoundedRectangle (c, c.getHeight() * 0.5f);
            g.setColour (acc); g.fillEllipse (c.getX() + 9, c.getCentreY() - 3, 6, 6);
            g.setColour (p.t2); g.setFont (font (11.0f)); g.drawText (setting, c.withTrimmedLeft (19), juce::Justification::centredLeft);
            return w;
        };
        if (! advanced)
        {
            auto t = eventText.toFloat();
            g.setColour (p.t1); g.setFont (font (24.0f, true));
            g.drawText (ev.name, t.removeFromTop (30), juce::Justification::centredLeft);
            auto line = t.withTrimmedTop (2).removeFromTop (20);
            g.setColour (p.t3); g.setFont (font (11.0f));
            const float mw = juce::GlyphArrangement::getStringWidth (font (11.0f), meta) + 12;
            g.drawText (meta, line.removeFromLeft (mw), juce::Justification::centredLeft);
            chip (line);
        }
        else
        {
            auto t = recInfo.toFloat();
            chip (t.removeFromTop (20));
            t.removeFromTop (6);
            g.setColour (p.t1); g.setFont (font (22.0f, true)); g.drawText (ev.name, t.removeFromTop (26), juce::Justification::centredLeft);
            g.setColour (p.t3); g.setFont (font (11.0f));
            g.drawText (meta + juce::String (juce::CharPointer_UTF8 (" \xc2\xb7 ")) + date.formatted ("%H:%M") + " UTC", t.removeFromTop (16), juce::Justification::centredLeft);
            t.removeFromTop (10);
            if (juce::isPositiveAndBelow (sel.station, (int) ev.stations.size()))
            {
                const auto& s = ev.stations[(size_t) sel.station];
                t = t.withHeight (juce::jmin (t.getHeight(), 66.0f));
                g.setColour (p.wellColour()); g.fillRoundedRectangle (t, 10.0f);
                auto in = t.reduced (12, 8);
                const float lh = in.getHeight() / 3.0f;
                g.setColour (p.t1); g.setFont (font (11.5f));
                g.drawText (s.label() + juce::String (juce::CharPointer_UTF8 (" \xc2\xb7 ")) + s.channel + "Z / " + s.channel + "N / " + s.channel + "E", in.removeFromTop (lh), juce::Justification::centredLeft);
                g.setColour (p.t2); g.setFont (font (11.0f));
                g.drawText (juce::String (juce::roundToInt (s.distKm)) + juce::String (juce::CharPointer_UTF8 (" km \xc2\xb7 ")) + (s.band == "tele" ? juce::String ("far") : s.band)
                            + (s.channel == "HN" ? juce::String (juce::CharPointer_UTF8 (" \xc2\xb7 accelerometer")) : juce::String (juce::CharPointer_UTF8 (" \xc2\xb7 broadband 0.1 to 8 Hz"))), in.removeFromTop (lh), juce::Justification::centredLeft);
                g.setColour (p.t3);
                g.drawText ("SNR " + juce::String (juce::roundToInt (s.snr)) + juce::String (juce::CharPointer_UTF8 (" \xc2\xb7 ")) + s.dataCentre + juce::String (juce::CharPointer_UTF8 (" \xc2\xb7 ")) + juce::String (juce::roundToInt (sel.compression)) + juce::String (juce::CharPointer_UTF8 ("\xc3\x97 faster")), in.removeFromTop (lh), juce::Justification::centredLeft);
            }
        }
    }

    // footer
    auto f = footer.toFloat();
    g.setFont (font (10.5f));
    g.setColour (p.t3);
    if (Settings::get().help())
    {
        g.drawText (juce::String (juce::CharPointer_UTF8 ("Help is on: hover any control \xc2\xb7 click i to turn it off \xc2\xb7 double-click a knob to reset")), f, juce::Justification::centredLeft);
    }
    else g.drawText ("Drag knobs up or down, double-click to reset, hold Shift for fine steps", f, juce::Justification::centredLeft);
    const bool busy = proc.rendering.load();
    const juce::String right = juce::String (juce::CharPointer_UTF8 ("data: EarthScope \xc2\xb7 ORFEUS \xc2\xb7 GEOFON \xc2\xb7 v")) + JucePlugin_VersionString;
    const float rw = juce::GlyphArrangement::getStringWidth (font (10.5f), right);
    g.drawText (right, f, juce::Justification::centredRight);
    auto st = f.withRight (f.getRight() - rw - 18).removeFromRight (90);
    g.setColour (busy ? p[orange] : p[lime]); g.fillEllipse (st.getX(), st.getCentreY() - 3, 6, 6);
    g.setColour (p.t3); g.drawText (busy ? "rendering" : "ready", st.withTrimmedLeft (11), juce::Justification::centredLeft);
}

void EditorRoot::showOverlay (Overlay& o)
{
    closeOverlays();
    o.setBounds (getLocalBounds());
    o.setVisible (true);
    o.toFront (true);
}

bool EditorRoot::closeOverlays()
{
    bool any = false;
    for (auto* o : std::initializer_list<Overlay*> { &settingsPanel, &aboutPanel, &presetBrowser })
        if (o->isVisible()) { o->setVisible (false); any = true; }
    return any;
}

void EditorRoot::themeChanged()
{
    irView.recolour();
    globe.updateTimer();
    helpButton.setToggleState (Settings::get().help(), juce::dontSendNotification);
    for (auto& k : knobs) k->resized();
    resized();
    repaint();
}

// ---------------------------------------------------------------- refresh
void EditorRoot::refreshFromProcessor (bool force)
{
    const int v = proc.viewVersion.load();
    if (force || v != shownVersion)
    {
        shownVersion = v;
        const auto view = proc.getView();
        const int ev = proc.currentEventIndex();
        sel.event = ev;
        sel.station = proc.stationIndexFor (ev);
        if (view.eventIndex == ev) { sel.w0 = view.windowStart; sel.w1 = view.windowEnd; sel.compression = view.compression; }
        for (auto* d : std::initializer_list<DataView*> { &globe, &map, &signal, &waves, &finger }) d->setSelection (sel);
        if (view.ir.getNumSamples() > 0) irView.setIR (view.ir, view.irRate);
        eventBox.setSelectedId (ev + 1, juce::dontSendNotification);

        // station buttons show this earthquake's distances
        const int want = (int) proc.apvts.getRawParameterValue ("station")->load();
        const auto& evt = proc.pack.events[(size_t) ev];
        for (int i = 0; i < 3; ++i)
        {
            auto& b = *stations.buttons[(size_t) i];
            juce::String sub = "none";
            for (auto& s : evt.stations)
                if (s.band == stationKeys[i])
                {
                    const int km = juce::roundToInt (s.distKm);
                    sub = (km >= 1000 ? juce::String (km / 1000) + "," + juce::String (km % 1000).paddedLeft ('0', 3) : juce::String (km)) + " km";
                }
            b.getProperties().set ("sub", sub);
            b.setTitle (b.getButtonText() + " station, " + sub);
            b.setEnabled (sub != "none");
            b.setToggleState (i == want, juce::dontSendNotification);
        }
        stations.repaint();
    }
    // toolbar state
    presetButton.setButtonText (proc.presets.currentCategory().isNotEmpty() ? proc.presets.currentCategory() + juce::String (juce::CharPointer_UTF8 (" \xc2\xb7 ")) + proc.presets.currentName() : proc.presets.currentName());
    presetButton.getProperties().set ("textColour", (int) pink);
    aButton.setToggleState (abSlot == 0, juce::dontSendNotification);
    bButton.setToggleState (abSlot == 1, juce::dontSendNotification);
    undoButton.setEnabled (proc.undoManager.canUndo());
    redoButton.setEnabled (proc.undoManager.canRedo());
    const bool by = proc.apvts.getRawParameterValue ("bypass")->load() > 0.5f;
    bypassButton.setToggleState (! by, juce::dontSendNotification);
    bypassButton.setTitle (by ? "Bypass, Aftershock is off" : "Bypass, Aftershock is on");
    helpButton.setToggleState (Settings::get().help(), juce::dontSendNotification);
    repaint();
}

void EditorRoot::tick()
{
    inMeter.push (proc.inMeter.exchange (0.0f));
    outMeter.push (proc.outMeter.exchange (0.0f));
    const bool busy = proc.rendering.load();
    if (busy != (statusPulse > 0.5f)) { statusPulse = busy ? 1.0f : 0.0f; repaint (footer); }
    if (proc.viewVersion.load() != shownVersion) refreshFromProcessor (false);
    const int ev = proc.currentEventIndex(), st = proc.stationIndexFor (ev);
    if (ev != sel.event || st != sel.station) refreshFromProcessor (true);
}

// ================================================================= editor
AftershockEditor::AftershockEditor (AftershockProcessor& p) : AudioProcessorEditor (p), proc (p), root (p)
{
    juce::LookAndFeel::setDefaultLookAndFeel (&look);
    setLookAndFeel (&look);
    addAndMakeVisible (root);
    root.onViewChanged = [this] { applyScale(); };
    setResizable (true, true);
    Settings::get().addChangeListener (this);
    if (Settings::get().help()) tips = std::make_unique<juce::TooltipWindow> (this, 450);
    setWantsKeyboardFocus (true);
    applyScale();
    startTimerHz (30);
}

AftershockEditor::~AftershockEditor()
{
    Settings::get().removeChangeListener (this);
    setLookAndFeel (nullptr);
    juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
}

void AftershockEditor::applyScale()
{
    const auto base = root.baseSize();
    const double s = Settings::get().scale();
    const juce::ScopedValueSetter<bool> sv (inResize, true);
    if (auto* c = getConstrainer())
    {
        c->setFixedAspectRatio ((double) base.x / base.y);
        c->setSizeLimits (base.x / 2, base.y / 2, base.x * 5 / 2, base.y * 5 / 2);
    }
    setSize (juce::roundToInt (base.x * s), juce::roundToInt (base.y * s));
    resized();
}

void AftershockEditor::resized()
{
    const auto base = root.baseSize();
    const float s = (float) getWidth() / (float) base.x;
    root.setBounds (0, 0, base.x, base.y);
    root.setTransform (juce::AffineTransform::scale (s));
    if (! inResize && std::abs (s - (float) Settings::get().scale()) > 0.01f)
        Settings::get().setQuietly ("scale", s);
}

void AftershockEditor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    look.refresh();
    if (Settings::get().help() && tips == nullptr) tips = std::make_unique<juce::TooltipWindow> (this, 450);
    if (! Settings::get().help()) tips.reset();
    root.themeChanged();
    applyScale();
    sendLookAndFeelChange();
    repaint();
}

bool AftershockEditor::keyPressed (const juce::KeyPress& k)
{
    const auto cmd = juce::ModifierKeys::commandModifier;
    if (k == juce::KeyPress ('z', cmd, 0)) { proc.undoManager.undo(); return true; }
    if (k == juce::KeyPress ('z', cmd | juce::ModifierKeys::shiftModifier, 0) || k == juce::KeyPress ('y', cmd, 0)) { proc.undoManager.redo(); return true; }
    if (k == juce::KeyPress::escapeKey) return root.closeOverlays();
    return false;
}

void AftershockEditor::timerCallback()
{
    root.tick();
    // group changes into undo steps: a new step once the mouse has been up for a moment
    if (juce::ModifierKeys::currentModifiers.isAnyMouseButtonDown()) undoTicks = 0;
    else if (++undoTicks == 12) proc.undoManager.beginNewTransaction();
    if (Settings::get().theme() == Settings::system)
    {
        static bool lastDark = juce::Desktop::getInstance().isDarkModeActive();
        const bool d = juce::Desktop::getInstance().isDarkModeActive();
        if (d != lastDark) { lastDark = d; Settings::get().invalidate(); changeListenerCallback (nullptr); }
    }
}
