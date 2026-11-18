// Aftershock, a reverb made from earthquakes. (C) 2026 Sofia Vallejo Budziszewski, AGPL-3.0.
// Free to port, by humans. Bring your own ears. See PORTING.md.

#pragma once
#include "Widgets.h"
#include "PluginProcessor.h"

namespace ui
{
// A modal card over the plugin window. Escape or a click outside closes it.
class Overlay : public juce::Component
{
public:
    Overlay (const juce::String& title, int cardW, int cardH);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress&) override;
    void visibilityChanged() override;
    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;
    std::function<void()> onClose;
    void close() { setVisible (false); if (onClose) onClose(); }
protected:
    juce::Rectangle<int> card() const;
    virtual void layout (juce::Rectangle<int> content) = 0;
    juce::TextButton closeButton { juce::String (juce::CharPointer_UTF8 ("\xe2\x9c\x95")) };
    juce::String heading;
    int cw, ch;
    bool showHeading = true;
};

class SettingsPanel : public Overlay, private juce::ChangeListener
{
public:
    SettingsPanel();
    ~SettingsPanel() override;
    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void paint2 (juce::Graphics&);
private:
    void layout (juce::Rectangle<int>) override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override { sync(); }
    void sync();
    struct Row { std::unique_ptr<juce::ToggleButton> button; juce::String key, label, hint; bool def; };
    SegGroup theme { "Theme" }, size { "Window size" }, text { "Text size" }, vision { "Colour vision" };
    std::vector<Row> rows;
    juce::TextButton reset { "Reset to defaults" }, done { "Done" }, folder { "Open preset folder" };
    std::vector<std::pair<juce::Rectangle<int>, juce::String>> labels;   // painted captions
    juce::Rectangle<int> preview, visionArea;
};

class AboutPanel : public Overlay
{
public:
    AboutPanel();
    void paint (juce::Graphics&) override;
    void showPage (int page);
private:
    void layout (juce::Rectangle<int>) override;
    int page = 0;
    juce::HyperlinkButton website { "sofiavallejob.github.io/sofiavallejo", juce::URL ("https://sofiavallejob.github.io/sofiavallejo/") };
    juce::TextButton github { "Open GitHub" }, licences { "Licences" }, manual { "Manual" }, how { "How it works" }, back { juce::String (juce::CharPointer_UTF8 ("\xe2\x86\x90 Back")) };
    juce::Viewport scroller;
    struct Text : juce::Component { juce::TextLayout layout; void paint (juce::Graphics&) override; } howText;
    juce::Rectangle<int> textArea;
};

class PresetBrowser : public Overlay, private juce::ListBoxModel, private juce::TextEditor::Listener
{
public:
    explicit PresetBrowser (AftershockProcessor&);
    void paint (juce::Graphics&) override;
    void refresh();
    void openSaveForm();
private:
    void layout (juce::Rectangle<int>) override;
    int getNumRows() override { return (int) shown.size(); }
    void paintListBoxItem (int, juce::Graphics&, int, int, bool) override {}
    juce::Component* refreshComponentForRow (int, bool, juce::Component*) override;
    void listBoxItemClicked (int, const juce::MouseEvent&) override;
    void returnKeyPressed (int) override;
    juce::String getNameForRow (int row) override { return juce::isPositiveAndBelow (row, (int) shown.size()) ? shown[(size_t) row].name : juce::String(); }
    void textEditorTextChanged (juce::TextEditor&) override { refresh(); }
    void loadRow (int);
    void importPreset();

    AftershockProcessor& proc;
    juce::TextEditor search;
    SegGroup tabs { "Preset source" };
    std::vector<std::unique_ptr<juce::TextButton>> catButtons;
    juce::ListBox list { "Presets", this };
    juce::TextButton saveAs { juce::String (juce::CharPointer_UTF8 ("Save as\xe2\x80\xa6")) }, importB { "Import" }, folderB { "Folder" }, deleteB { "Delete" };
    juce::String category = "All";
    int tab = 0;   // 0 factory, 1 user, 2 favourites
    std::vector<Preset> shown;
    int selected = -1;
    std::unique_ptr<juce::FileChooser> chooser;

    // save form
    bool saving = false;
    juce::TextEditor nameEd, aboutEd;
    juce::ComboBox catBox;
    juce::TextButton confirm { "Save" }, cancel { "Cancel" };
    juce::Label error;
    juce::Rectangle<int> sideArea, listArea, footArea;
    friend struct PresetRow;
};
} // namespace ui
