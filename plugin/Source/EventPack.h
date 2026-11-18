// Aftershock, a reverb made from earthquakes. (C) 2026 Sofia Vallejo Budziszewski, AGPL-3.0.
// Free to port, by humans. Bring your own ears. See PORTING.md.

#pragma once
#include <juce_core/juce_core.h>
#include <array>
#include <vector>

// The earthquake pack: real broadband seismograms plus the measurements the
// build pipeline made on them (see pipeline/build_pack.py).
namespace aftershock
{
struct BandInfo
{
    float hz = 0.0f;
    float rt60 = -1.0f;      // seismic RT60 in seconds of ground time, -1 if not measurable
    float fitDb = 0.0f;      // decay range the RT60 was fitted over (20, 10 or 5 dB)
    float energyDb = 0.0f;   // band energy relative to the strongest band
    float qApparent = -1.0f;
};

struct Tap { float position, gain, pan; };   // position: 0..1 across the P..S window

struct Station
{
    juce::String net, sta, channel, dataCentre, band;
    double lat = 0, lon = 0, distKm = 0, distDeg = 0;
    float P = 0, S = -1, R = -1, codaEnd = 0, snr = 0, t0 = -30, fs = 20;
    int n = 0;
    juce::int64 offset = 0;
    double scale = 1.0;
    std::vector<BandInfo> bands;
    std::vector<Tap> taps;
    juce::String label() const { return net + "." + sta; }
};

struct Event
{
    juce::String id, name, setting, time, magType, usgsId;
    double lat = 0, lon = 0, depthKm = 0, mag = 0;
    std::vector<Station> stations;
};

struct Trace
{
    std::array<std::vector<float>, 3> zne;   // ground velocity, m/s
    float fs = 20, t0 = -30, P = 0, S = -1, R = -1;
    float duration() const { return zne[0].empty() ? 0.0f : (float) zne[0].size() / fs; }
    float endTime() const { return t0 + duration(); }
};

class EventPack
{
public:
    bool load (const void* data, size_t size);   // accepts the raw pack or a gzipped pack
    bool isLoaded() const { return ! events.empty(); }

    std::vector<Event> events;
    std::vector<float> bandsHz;
    std::vector<std::pair<juce::String, juce::String>> settings;   // key, label in display order
    juce::String credits;

    juce::String settingLabel (const juce::String& key) const;
    int settingIndex (const juce::String& key) const;
    Trace readTrace (const Station&) const;

    // bundled pack, loaded once per process
    static const EventPack& get();

private:
    juce::MemoryBlock blob;
};
} // namespace aftershock
