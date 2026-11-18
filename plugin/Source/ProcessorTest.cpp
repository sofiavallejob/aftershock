// Aftershock, a reverb made from earthquakes. (C) 2026 Sofia Vallejo Budziszewski, AGPL-3.0.
// Free to port, by humans. Bring your own ears. See PORTING.md.

// aftershock_test: runs the real plugin processor without a host and checks its output.
#include "PluginProcessor.h"

static float rmsDb (const juce::AudioBuffer<float>& b, int ch, int start, int num)
{
    return juce::Decibels::gainToDecibels (b.getRMSLevel (ch, start, num), -200.0f);
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    int failures = 0;
    auto check = [&] (bool ok, const juce::String& what) { std::cout << (ok ? "  ok    " : "  FAIL  ") << what << "\n"; if (! ok) ++failures; };

    AftershockProcessor proc;
    const double sr = 48000; const int block = 256;
    proc.setPlayConfigDetails (2, 2, sr, block);
    proc.prepareToPlay (sr, block);

    auto setParam = [&] (const char* id, float v) { auto* p = proc.apvts.getParameter (id); p->setValueNotifyingHost (p->convertTo0to1 (v)); };
    auto waitForIR = [&] (int previous) { for (int i = 0; i < 400 && proc.viewVersion.load() == previous; ++i) juce::Thread::sleep (10); juce::Thread::sleep (300); };

    std::cout << "pack: " << proc.pack.events.size() << " events\n";
    check (proc.pack.events.size() >= 30, "pack loaded");
    std::cout << "default event: " << proc.pack.events[(size_t) proc.currentEventIndex()].name << "\n";

    const auto t0 = juce::Time::getMillisecondCounterHiRes();
    waitForIR (0);
    std::cout << "first IR after " << juce::roundToInt (juce::Time::getMillisecondCounterHiRes() - t0) << " ms\n";
    check (proc.viewVersion.load() > 0, "impulse response rendered");
    check (proc.getLatencySamples() == 0, "zero latency (" + juce::String (proc.getLatencySamples()) + " samples)");

    // impulse in, wet only: output must contain a tail of about the reverb length
    setParam ("dry", 0); setParam ("wet", 1); setParam ("early", 0); setParam ("mod", 0); setParam ("duck", 0);
    const int seconds = 5, total = (int) sr * seconds;
    juce::AudioBuffer<float> out (2, total); out.clear();
    juce::MidiBuffer midi;
    juce::AudioBuffer<float> io (2, block);
    double cpuMs = 0;
    for (int pos = 0; pos < total; pos += block)
    {
        io.clear();
        if (pos == 0) { io.setSample (0, 0, 1.0f); io.setSample (1, 0, 1.0f); }
        const auto c0 = juce::Time::getMillisecondCounterHiRes();
        proc.processBlock (io, midi);
        cpuMs += juce::Time::getMillisecondCounterHiRes() - c0;
        for (int ch = 0; ch < 2; ++ch) out.copyFrom (ch, pos, io, ch, 0, juce::jmin (block, total - pos));
    }
    bool finite = true;
    for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < total; ++i) finite &= std::isfinite (out.getSample (ch, i));
    check (finite, "output finite");
    const float early = rmsDb (out, 0, (int) (0.05 * sr), (int) (0.5 * sr));
    const float late = rmsDb (out, 0, (int) (3.5 * sr), (int) (1.0 * sr));
    std::cout << "  tail: " << early << " dB in 0.05-0.55 s, " << late << " dB in 3.5-4.5 s (length 2.8 s)\n";
    check (early > -60 && early < 0, "wet tail present and not too loud");
    check (late < early - 30, "tail ends after the reverb length");
    const double realtimeMs = 1000.0 * seconds;
    std::cout << "  processing " << juce::String (cpuMs, 1) << " ms for " << seconds << " s of audio ("
              << juce::String (100.0 * cpuMs / realtimeMs, 2) << " % of one core)\n";
    check (cpuMs < realtimeMs * 0.25, "CPU well below real time");

    // dry only must pass the input unchanged
    setParam ("dry", 1); setParam ("wet", 0);
    juce::Thread::sleep (50);
    for (int i = 0; i < 40; ++i) { io.clear(); proc.processBlock (io, midi); }   // let gains settle
    juce::AudioBuffer<float> sine (2, block);
    for (int i = 0; i < block; ++i) sine.setSample (0, i, std::sin (i * 0.1f)), sine.setSample (1, i, std::sin (i * 0.1f));
    auto copy = sine; proc.processBlock (copy, midi);
    float maxErr = 0; for (int i = 0; i < block; ++i) maxErr = juce::jmax (maxErr, std::abs (copy.getSample (0, i) - sine.getSample (0, i)));
    check (maxErr < 1e-4f, "dry path is transparent");

    // switching earthquake re-renders, and different settings give different IRs
    auto irStats = [&]
    {
        auto v = proc.getView(); auto& b = v.ir;
        juce::dsp::FFT fft (15); std::vector<float> d (1 << 16, 0.0f);
        for (int i = 0; i < juce::jmin (b.getNumSamples(), 1 << 15); ++i) d[(size_t) i] = b.getSample (0, i);
        fft.performFrequencyOnlyForwardTransform (d.data());
        double num = 0, den = 0; for (int k = 1; k < (1 << 14); ++k) { num += d[(size_t) k] * k * sr / (1 << 15); den += d[(size_t) k]; }
        return num / den;
    };
    setParam ("spectrum", 1.0f); setParam ("station", 0);
    std::vector<std::pair<juce::String, double>> cents;
    for (auto id : { "petrinja2020", "kilauea2018", "botswana2017", "noto2024" })
        for (int i = 0; i < (int) proc.pack.events.size(); ++i)
            if (proc.pack.events[(size_t) i].id == id)
            {
                const int v = proc.viewVersion.load(); setParam ("event", (float) i); waitForIR (v);
                cents.push_back ({ id, irStats() });
                std::cout << "  " << id << " near: IR centroid " << juce::roundToInt (cents.back().second) << " Hz\n";
            }
    double lo = 1e9, hi = 0; for (auto& c : cents) { lo = juce::jmin (lo, c.second); hi = juce::jmax (hi, c.second); }
    check (hi / lo > 1.2, "different earthquakes give audibly different spectra");

    // state round trip
    juce::MemoryBlock state; proc.getStateInformation (state);
    AftershockProcessor other; other.setStateInformation (state.getData(), (int) state.getSize());
    check (other.currentEventIndex() == proc.currentEventIndex(), "state restores the same earthquake");

    proc.releaseResources();
    std::cout << (failures == 0 ? "all checks passed\n" : "SOME CHECKS FAILED\n");
    return failures;
}
