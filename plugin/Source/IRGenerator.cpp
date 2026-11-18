// Aftershock, a reverb made from earthquakes. (C) 2026 Sofia Vallejo Budziszewski, AGPL-3.0.
// Free to port, by humans. Bring your own ears. See PORTING.md.

#include "IRGenerator.h"
#include <cmath>

namespace aftershock
{
namespace
{
constexpr int numBands = 10;
constexpr double pi = 3.14159265358979323846;

inline float seismicBandHz (int b) { return (float) (0.1 * std::pow (80.0, b / (double) (numBands - 1))); }
inline float audioBandHz (int b)   { return (float) (120.0 * std::pow (100.0, b / (double) (numBands - 1))); }

struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    void reset() { z1 = z2 = 0; }
    float process (float x)
    {
        double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return (float) y;
    }
    static Biquad make (double b0, double b1, double b2, double a0, double a1, double a2)
    {
        Biquad q; q.b0 = b0 / a0; q.b1 = b1 / a0; q.b2 = b2 / a0; q.a1 = a1 / a0; q.a2 = a2 / a0; return q;
    }
    static Biquad bandpass (double f, double Q, double fs)
    {
        f = std::min (f, fs * 0.45);
        const double w = 2 * pi * f / fs, a = std::sin (w) / (2 * Q), c = std::cos (w);
        return make (a, 0, -a, 1 + a, -2 * c, 1 - a);
    }
    static Biquad lowpass (double f, double fs)
    {
        f = std::min (f, fs * 0.45);
        const double w = 2 * pi * f / fs, a = std::sin (w) / std::sqrt (2.0), c = std::cos (w);
        return make ((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + a, -2 * c, 1 - a);
    }
    static Biquad highpass (double f, double fs)
    {
        const double w = 2 * pi * f / fs, a = std::sin (w) / std::sqrt (2.0), c = std::cos (w);
        return make ((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + a, -2 * c, 1 - a);
    }
};

// zero-phase filtering (forward + backward), so envelopes don't shift in time
void filtfilt (std::vector<float>& x, Biquad q)
{
    q.reset(); for (auto& v : x) v = q.process (v);
    q.reset(); for (auto it = x.rbegin(); it != x.rend(); ++it) *it = q.process (*it);
}

inline float lerp (const std::vector<float>& a, double pos)
{
    if (pos <= 0) return a.empty() ? 0.0f : a.front();
    const auto i = (size_t) pos;
    if (i + 1 >= a.size()) return a.empty() ? 0.0f : a.back();
    const float f = (float) (pos - (double) i);
    return a[i] + f * (a[i + 1] - a[i]);
}

inline float hermite (const std::vector<float>& a, double pos)
{
    const auto n = (long) a.size();
    const long i = (long) std::floor (pos);
    if (i < 1 || i + 2 >= n) return lerp (a, pos);
    const float t = (float) (pos - (double) i);
    const float y0 = a[(size_t) i - 1], y1 = a[(size_t) i], y2 = a[(size_t) i + 1], y3 = a[(size_t) i + 2];
    const float c1 = 0.5f * (y2 - y0), c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3, c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
    return ((c3 * t + c2) * t + c1) * t + y1;
}

// decay slope (dB per second) from -5 dB to -25 dB below the peak (or a shorter
// range if the envelope does not get that low inside the window)
float decaySlope (const std::vector<float>& envDb, int i0, int i1, float fs, int& peakIndex)
{
    peakIndex = i0;
    for (int i = i0; i < i1; ++i)
        if (envDb[(size_t) i] > envDb[(size_t) peakIndex]) peakIndex = i;
    const float pk = envDb[(size_t) peakIndex];
    for (float lo : { -25.0f, -15.0f, -10.0f })
    {
        int a = -1, b = -1;
        for (int i = peakIndex; i < i1; ++i)
        {
            const float d = envDb[(size_t) i] - pk;
            if (a < 0 && d <= -5.0f) a = i;
            if (d <= lo) { b = i; break; }
        }
        if (a < 0 || b < 0 || b - a < (int) (3 * fs)) continue;
        double sx = 0, sy = 0, sxx = 0, sxy = 0; int n = 0;
        for (int i = a; i < b; ++i)
        {
            const double x = i / (double) fs, y = envDb[(size_t) i];
            sx += x; sy += y; sxx += x * x; sxy += x * y; ++n;
        }
        const double den = n * sxx - sx * sx;
        if (den <= 0) continue;
        const double slope = (n * sxy - sx * sy) / den;
        if (slope < 0) return (float) slope;
    }
    return 0.0f;
}
} // namespace

std::pair<float, float> windowFor (const Trace& tr, WindowStart w)
{
    const float end = tr.endTime();
    float start = std::max (tr.t0, tr.P - 1.0f);
    switch (w)
    {
        case WindowStart::p:       start = tr.P - 0.2f; break;
        case WindowStart::s:       if (tr.S > 0) start = tr.S - 0.3f; break;
        case WindowStart::surface: if (tr.R > 0) start = tr.R - 1.0f; else if (tr.S > 0) start = tr.S - 0.3f; break;
        case WindowStart::full:    break;
    }
    start = juce::jlimit (tr.t0, std::max (tr.t0, end - 5.0f), start);
    return { start, end };
}

IRResult generateIR (const Trace& tr, const IRParams& p, double sr)
{
    IRResult res;
    res.sampleRate = sr;
    if (tr.zne[0].size() < 64) return res;

    const float fs = tr.fs;
    auto [ws, we] = windowFor (tr, p.window);
    res.windowStart = ws; res.windowEnd = we;

    const float L = juce::jlimit (0.2f, 30.0f, p.lengthS);
    const int len = std::max (64, (int) std::lround (L * sr));
    const double k = (we - ws) / L;                       // ground seconds per audio second
    res.compression = (float) k;
    const double i0 = (ws - tr.t0) * fs;                  // seismic sample index at window start
    const double step = k * fs / sr;                      // seismic samples per audio sample
    const int s0 = (int) std::floor (i0), s1 = (int) tr.zne[0].size();

    // which components feed the left and right IR channel
    std::array<std::array<float, 3>, 2> mixW {};
    switch (p.channels)
    {
        case Channels::northEast: mixW = {{ { 0, 1, 0 }, { 0, 0, 1 } }}; break;
        case Channels::vertical:  mixW = {{ { 1, 0, 0 }, { 1, 0, 0 } }}; break;
        case Channels::all:       mixW = {{ { 0.577f, 0.816f, 0 }, { 0.577f, 0, 0.816f } }}; break;
    }

    // envelope smoothing: seismic-time cutoff, limited so the audio-time
    // modulation stays below ~40 Hz (no zipper grain)
    auto smoothHz = [&] (int b) { return std::max (0.002, std::min (0.35 * seismicBandHz (b), 40.0 / k)); };

    // ---- band envelopes in seismic time, per component
    std::array<std::array<std::vector<float>, numBands>, 3> env;
    std::array<bool, 3> used { false, false, false };
    for (int ch = 0; ch < 2; ++ch) for (int c = 0; c < 3; ++c) if (mixW[(size_t) ch][(size_t) c] != 0) used[(size_t) c] = true;

    std::array<double, numBands> bandRms {};
    std::array<float, numBands> slope {};
    std::array<float, numBands> peakT {};

    for (int b = 0; b < numBands; ++b)
    {
        const float fb = seismicBandHz (b);
        std::vector<float> sumE ((size_t) s1, 0.0f);
        for (int c = 0; c < 3; ++c)
        {
            if (! used[(size_t) c]) continue;
            auto x = tr.zne[(size_t) c];
            if (fb < fs * 0.45f) filtfilt (x, Biquad::bandpass (fb, 1.4, fs));
            else std::fill (x.begin(), x.end(), 0.0f);
            for (auto& v : x) v = v * v;
            filtfilt (x, Biquad::lowpass (smoothHz (b), fs));
            for (size_t i = 0; i < x.size(); ++i) { x[i] = std::sqrt (std::max (0.0f, x[i])); sumE[i] += x[i]; }
            env[(size_t) c][(size_t) b] = std::move (x);
        }
        double acc = 0; int n = 0;
        for (int i = std::max (0, s0); i < s1; ++i) { acc += (double) sumE[(size_t) i] * sumE[(size_t) i]; ++n; }
        bandRms[(size_t) b] = n > 0 ? std::sqrt (acc / n) : 0.0;

        std::vector<float> db ((size_t) s1);
        for (int i = 0; i < s1; ++i) db[(size_t) i] = 20.0f * std::log10 (std::max (sumE[(size_t) i], 1e-30f));
        int pk = 0;
        slope[(size_t) b] = decaySlope (db, std::max (0, s0), s1, fs, pk);
        peakT[(size_t) b] = tr.t0 + pk / fs;
    }
    res.bandDecayDbPerS.assign (slope.begin(), slope.end());

    double maxRms = 0;
    for (auto r : bandRms) maxRms = std::max (maxRms, r);
    double meanSlope = 0; int nSlope = 0;
    for (auto s : slope) if (s < 0) { meanSlope += s; ++nSlope; }
    meanSlope = nSlope ? meanSlope / nSlope : 0.0;

    // per band: normalise, then restore (part of) the recorded spectrum
    std::array<float, numBands> bandGain {};
    for (int b = 0; b < numBands; ++b)
    {
        if (bandRms[(size_t) b] <= 0 || maxRms <= 0) { bandGain[(size_t) b] = 0; continue; }
        const double rel = juce::jmax (1.0e-3, bandRms[(size_t) b] / maxRms);   // floor at -60 dB
        bandGain[(size_t) b] = (float) (std::pow (rel, (double) p.spectrum) / bandRms[(size_t) b]);
    }

    // ---- render
    juce::Random rng ((juce::int64) p.seed);
    res.ir.setSize (2, len);
    res.ir.clear();

    for (int ch = 0; ch < 2; ++ch)
    {
        auto* out = res.ir.getWritePointer (ch);
        std::vector<float> smooth ((size_t) len, 0.0f);

        for (int b = 0; b < numBands; ++b)
        {
            if (bandGain[(size_t) b] == 0) continue;
            // carrier: band-passed noise in the matching audio band (2 biquads = 4th order)
            auto q1 = Biquad::bandpass (audioBandHz (b), 1.9, sr), q2 = q1;
            const float dbPerS = (float) ((p.decayContrast - 1.0f) * (slope[(size_t) b] - meanSlope));
            const float tilt = (float) b / (numBands - 1);

            for (int i = 0; i < len; ++i)
            {
                const double pos = i0 + i * step;
                float e = 0;
                for (int c = 0; c < 3; ++c)
                    if (const float wgt = mixW[(size_t) ch][(size_t) c]; wgt != 0)
                        e += wgt * lerp (env[(size_t) c][(size_t) b], pos);

                const float tg = (float) (tr.t0 + pos / fs);                  // ground time
                float extraDb = tg > peakT[(size_t) b] ? dbPerS * (tg - peakT[(size_t) b]) : 0.0f;
                extraDb = juce::jlimit (-80.0f, 12.0f, extraDb);
                const float u = (float) i / (float) len;
                const float g = bandGain[(size_t) b] * std::pow (10.0f, extraDb / 20.0f)
                                * std::exp (-p.damping * tilt * 8.0f * u);
                const float noise = q2.process (q1.process (rng.nextFloat() * 2.0f - 1.0f));
                smooth[(size_t) i] += noise * e * g;
            }
        }

        // raw seismogram, played k times faster
        std::vector<float> grit ((size_t) len, 0.0f);
        if (p.grit > 0.0f)
        {
            for (int i = 0; i < len; ++i)
            {
                const double pos = i0 + i * step;
                float v = 0;
                for (int c = 0; c < 3; ++c)
                    if (const float wgt = mixW[(size_t) ch][(size_t) c]; wgt != 0)
                        v += wgt * hermite (tr.zne[(size_t) c], pos);
                grit[(size_t) i] = v;
            }
            auto h1 = Biquad::highpass (30.0, sr), h2 = h1;
            for (auto& v : grit) v = h2.process (h1.process (v));
            if (p.damping > 0)
            {
                auto lp = Biquad::lowpass (1500.0, sr);
                for (int i = 0; i < len; ++i)
                {
                    const float m = std::exp (-p.damping * 6.0f * (float) i / (float) len);
                    const float l = lp.process (grit[(size_t) i]);
                    grit[(size_t) i] = grit[(size_t) i] * m + l * (1.0f - m);
                }
            }
        }

        // level-match the two paths before blending
        auto rmsOf = [] (const std::vector<float>& v) { double a = 0; for (auto x : v) a += (double) x * x; return std::sqrt (a / std::max<size_t> (1, v.size())); };
        const double rs = rmsOf (smooth), rg = rmsOf (grit);
        const float gs = rs > 0 ? (float) (1.0 / rs) : 0.0f, gg = rg > 0 ? (float) (1.0 / rg) : 0.0f;
        for (int i = 0; i < len; ++i)
            out[i] = (1.0f - p.grit) * smooth[(size_t) i] * gs + p.grit * grit[(size_t) i] * gg;
    }

    // fades, optional reverse, unit energy per channel
    const int fadeIn = std::min (len / 4, (int) (0.002 * sr)), fadeOut = len / 10;
    for (int ch = 0; ch < 2; ++ch)
    {
        auto* d = res.ir.getWritePointer (ch);
        for (int i = 0; i < fadeIn; ++i) d[i] *= (float) i / (float) fadeIn;
        for (int i = 0; i < fadeOut; ++i) d[len - 1 - i] *= (float) i / (float) fadeOut;
    }
    if (p.reverse) res.ir.reverse (0, len);

    double energy = 0;
    for (int ch = 0; ch < 2; ++ch) { auto* d = res.ir.getReadPointer (ch); for (int i = 0; i < len; ++i) energy += (double) d[i] * d[i]; }
    if (energy > 0) res.ir.applyGain ((float) (1.0 / std::sqrt (energy / 2.0)));
    return res;
}
} // namespace aftershock
