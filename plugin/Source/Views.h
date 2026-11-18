// Aftershock, a reverb made from earthquakes. (C) 2026 Sofia Vallejo Budziszewski, AGPL-3.0.
// Free to port, by humans. Bring your own ears. See PORTING.md.

#pragma once
#include "Theme.h"
#include "PluginProcessor.h"

namespace ui
{
// Selection shared by the data views
struct Selection
{
    int event = 0, station = 0;
    float w0 = 0, w1 = 0, compression = 1;
};

// Land as dots and polygons, loaded once from the bundled coastline
struct Land
{
    static const Land& get();
    std::vector<std::vector<juce::Point<float>>> rings;   // lon, lat
    std::vector<juce::Point<float>> dots;                // lon, lat on a ~4 degree grid
};

class DataView : public juce::Component
{
public:
    explicit DataView (AftershockProcessor& p) : proc (p) {}
    virtual void setSelection (const Selection& s) { sel = s; selectionChanged(); repaint(); }
    virtual void selectionChanged() {}
    std::function<void (int eventIndex)> onPickEvent;
protected:
    const aftershock::Event* event() const;
    const aftershock::Station* station() const;
    AftershockProcessor& proc;
    Selection sel;
};

class GlobeView : public DataView, private juce::Timer
{
public:
    explicit GlobeView (AftershockProcessor&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void visibilityChanged() override { updateTimer(); }
    void selectionChanged() override;
    void updateTimer();
    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;
private:
    void timerCallback() override;
    bool project (double lat, double lon, juce::Point<float>& out, float& depth) const;
    int hit (juce::Point<float>) const;
    double lon0 = 15.0, tilt = 18.0, dragLon = 0;
    bool dragging = false, moved = false;
    int hover = -1;
    double lastTime = 0;
};

class MapView : public DataView
{
public:
    explicit MapView (AftershockProcessor& p) : DataView (p) { setTitle ("Earthquake map"); }
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override { hover = -1; repaint(); }
    void selectionChanged() override;
    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;
private:
    juce::Rectangle<float> plotArea() const;
    juce::Point<float> project (double lat, double lon) const;
    int hit (juce::Point<float>) const;
    int hover = -1;
};

// One horizontal component plus an energy strip, for the simple view
class SignalView : public DataView
{
public:
    explicit SignalView (AftershockProcessor& p) : DataView (p) { setTitle ("Seismogram"); }
    void paint (juce::Graphics&) override;
    void selectionChanged() override;
    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;
    aftershock::Trace trace;
};

// Three components, for the advanced view
class WavesView : public DataView
{
public:
    explicit WavesView (AftershockProcessor& p) : DataView (p) { setTitle ("Ground velocity, three components"); }
    void paint (juce::Graphics&) override;
    void selectionChanged() override;
    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;
    aftershock::Trace trace;
};

class IRView : public juce::Component
{
public:
    IRView() { setTitle ("Impulse response spectrogram"); }
    void setIR (const juce::AudioBuffer<float>&, double rate);
    void recolour();
    void paint (juce::Graphics&) override;
    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;
private:
    std::vector<std::vector<float>> cols;
    juce::Image img;
    float seconds = 0;
};

class FingerprintView : public DataView
{
public:
    explicit FingerprintView (AftershockProcessor& p) : DataView (p) { setTitle ("Coda fingerprint"); }
    void paint (juce::Graphics&) override;
    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;
};

std::unique_ptr<juce::AccessibilityHandler> imageHandler (juce::Component&, std::function<juce::String()> description);
} // namespace ui
