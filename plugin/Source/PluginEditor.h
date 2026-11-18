// Aftershock, a reverb made from earthquakes. (C) 2026 Sofia Vallejo Budziszewski, AGPL-3.0.
// Free to port, by humans. Bring your own ears. See PORTING.md.

#pragma once
#include "PluginProcessor.h"
#include "Widgets.h"
#include "Views.h"
#include "Panels.h"

// Everything inside the window, laid out at a fixed base size and scaled as a whole.
class EditorRoot : public juce::Component
{
public:
    EditorRoot (AftershockProcessor&);
    ~EditorRoot() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void setAdvanced (bool);
    bool isAdvanced() const { return advanced; }
    void refreshFromProcessor (bool force);
    void tick();
    void themeChanged();
    void showOverlay (ui::Overlay&);
    bool closeOverlays();
    juce::Point<int> baseSize() const { return advanced ? juce::Point<int> (1200, 970) : juce::Point<int> (1040, 720); }
    std::function<void()> onViewChanged;

private:
    void buildToolbar();
    void layoutToolbar (juce::Rectangle<int>);
    void layoutSimple (juce::Rectangle<int>);
    void layoutAdvanced (juce::Rectangle<int>);
    void setParam (const juce::String& id, float value);
    void switchAB (int slot);
    void announce (const juce::String&);
    ui::Knob& knob (const juce::String& id, const juce::String& title, ui::Accent, const juce::String& help, bool invert = false);
    juce::String eventLine (const aftershock::Event&) const;

    AftershockProcessor& proc;
    ui::Selection sel;
    int shownVersion = -1;

    // toolbar
    juce::TextButton logo { "AFTERSHOCK" }, presetButton, prevPreset { juce::String (juce::CharPointer_UTF8 ("\xe2\x80\xb9")) }, nextPreset { juce::String (juce::CharPointer_UTF8 ("\xe2\x80\xba")) },
        saveButton { "Save" }, loadButton { "Load" }, aButton { "A" }, bButton { "B" }, undoButton { juce::String (juce::CharPointer_UTF8 ("\xe2\x86\xb6")) }, redoButton { juce::String (juce::CharPointer_UTF8 ("\xe2\x86\xb7")) },
        helpButton { "i" }, themeButton { juce::String (juce::CharPointer_UTF8 ("\xe2\x97\x90")) }, settingsButton { juce::String (juce::CharPointer_UTF8 ("\xe2\x9a\x99")) }, viewButton { "Advanced" }, bypassButton { juce::String (juce::CharPointer_UTF8 ("\xe2\x8f\xbb")) };
    std::vector<std::pair<juce::Rectangle<int>, juce::String>> toolbarLabels;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAtt;
    juce::ValueTree abSlots[2];
    int abSlot = 0;

    // data
    ui::GlobeView globe;
    ui::MapView map;
    ui::SignalView signal;
    ui::WavesView waves;
    ui::IRView irView;
    ui::FingerprintView finger;
    ui::SegGroup stations { "Listen from" };
    juce::TextButton prevEvent { juce::String (juce::CharPointer_UTF8 ("\xe2\x80\xb9")) }, nextEvent { juce::String (juce::CharPointer_UTF8 ("\xe2\x80\xba")) }, allEvents;
    juce::ComboBox eventBox;

    // processing
    std::vector<std::unique_ptr<ui::Knob>> knobs;
    std::map<juce::String, ui::Knob*> simpleKnobs, advKnobs;
    juce::ComboBox windowBox, channelBox;
    juce::ToggleButton reverseToggle { "Reverse" }, freezeToggle { "Freeze" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> windowAtt, channelAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> reverseAtt, freezeAtt;
    ui::Meter inMeter { "Input level" }, outMeter { "Output level" };
    juce::Slider gain;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> gainAtt;

    // overlays
    ui::SettingsPanel settingsPanel;
    ui::AboutPanel aboutPanel;
    ui::PresetBrowser presetBrowser;
    std::unique_ptr<juce::FileChooser> chooser;

    // layout rectangles used by paint()
    struct PanelRect { juce::Rectangle<int> r; ui::Accent a, b; float s; juce::String title, note; ui::Accent dot; };
    std::vector<PanelRect> panels;
    std::vector<std::pair<juce::Rectangle<int>, juce::String>> sectionLabels;
    juce::Rectangle<int> eventText, recInfo, footer, groupLines[4], statusArea;
    bool advanced = false;
    float statusPulse = 0;
};

class AftershockEditor : public juce::AudioProcessorEditor, private juce::Timer, private juce::ChangeListener
{
public:
    explicit AftershockEditor (AftershockProcessor&);
    ~AftershockEditor() override;
    void paint (juce::Graphics&) override {}
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    void timerCallback() override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void applyScale();

    AftershockProcessor& proc;
    ui::Look look;
    EditorRoot root;
    std::unique_ptr<juce::TooltipWindow> tips;
    bool inResize = false;
    int undoTicks = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AftershockEditor)
};
