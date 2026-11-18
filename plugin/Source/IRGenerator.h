// Aftershock, a reverb made from earthquakes. (C) 2026 Sofia Vallejo Budziszewski, AGPL-3.0.
// Free to port, by humans. Bring your own ears. See PORTING.md.

#pragma once
#include "EventPack.h"
#include <juce_audio_basics/juce_audio_basics.h>

namespace aftershock
{
enum class WindowStart { full = 0, p, s, surface };
enum class Channels { northEast = 0, vertical, all };

struct IRParams
{
    WindowStart window = WindowStart::full;
    Channels channels = Channels::northEast;
    float lengthS = 3.0f;      // reverb length the window is compressed into
    float damping = 0.2f;      // extra high-band decay on top of the data
    float spectrum = 0.5f;     // 0 = every band equally loud, 1 = band levels as recorded
    float decayContrast = 1.0f;// 0 = all bands decay alike, 1 = as recorded, >1 exaggerated
    float grit = 0.1f;         // blend of the raw resampled seismogram
    bool reverse = false;
    juce::uint32 seed = 1;
};

struct IRResult
{
    juce::AudioBuffer<float> ir;          // stereo, unit energy per channel
    double sampleRate = 48000;
    float windowStart = 0, windowEnd = 0; // seconds after origin
    float compression = 1;                // ground seconds per audio second
    std::vector<float> bandDecayDbPerS;   // measured inside the window, per band (0 if none)
};

// Turns a seismogram into a stereo impulse response.
// Ten seismic bands (0.1-8 Hz, log spaced) each give an envelope that keeps its own
// decay; the envelope drives band-passed noise in a matching audio band
// (120 Hz-12 kHz, log onto log). Optional blend with the raw resampled trace.
IRResult generateIR (const Trace&, const IRParams&, double sampleRate);

// Window bounds for a given start mode, in seconds after origin.
std::pair<float, float> windowFor (const Trace&, WindowStart);
} // namespace aftershock
