// Aftershock, a reverb made from earthquakes. (C) 2026 Sofia Vallejo Budziszewski, AGPL-3.0.
// Free to port, by humans. Bring your own ears. See PORTING.md.

// aftershock_demo: runs a dry WAV through the real plugin with a factory preset and
// writes the result, with room for the tail. Used to make the sound examples.
//
//   aftershock_demo "Preset name" in.wav out.wav [--tail 4] [--wet 0.4] [--event id] [--station near]
#include "PluginProcessor.h"
#include <juce_audio_formats/juce_audio_formats.h>

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI init;
    juce::StringArray a; for (int i = 1; i < argc; ++i) a.add (argv[i]);
    if (a.size() < 3) { std::cerr << "usage: aftershock_demo PRESET in.wav out.wav [--tail s] [--wet x] [--event id] [--station near|regional|tele]\n"; return 1; }
    auto opt = [&] (const juce::String& k) { const int i = a.indexOf (k); return i >= 0 && i + 1 < a.size() ? a[i + 1] : juce::String(); };

    juce::AudioFormatManager fm; fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> rd (fm.createReaderFor (juce::File (a[1])));
    if (! rd) { std::cerr << "cannot read " << a[1] << "\n"; return 1; }
    const double sr = rd->sampleRate; const int block = 512;

    AftershockProcessor proc;
    proc.setPlayConfigDetails (2, 2, sr, block);
    proc.prepareToPlay (sr, block);
    bool found = false;
    for (auto& p : proc.presets.all()) if (p.name == a[0]) { auto pr = p; if (opt ("--event").isNotEmpty()) pr.eventId = opt ("--event"); if (opt ("--station").isNotEmpty()) pr.station = opt ("--station"); proc.presets.load (pr); found = true; }
    if (! found) { std::cerr << "no preset " << a[0] << "\n"; return 1; }
    if (opt ("--wet").isNotEmpty()) if (auto* w = proc.apvts.getParameter ("wet")) w->setValueNotifyingHost (w->convertTo0to1 (opt ("--wet").getFloatValue()));
    // wait until the background renderer has settled on this preset's IR
    int last = proc.viewVersion.load(), stable = 0;
    for (int i = 0; i < 3000 && stable < 60; ++i)
    {
        juce::Thread::sleep (10);
        const int v = proc.viewVersion.load();
        if (v != last || proc.rendering.load()) { last = v; stable = 0; } else ++stable;
    }

    const float tail = opt ("--tail").isNotEmpty() ? opt ("--tail").getFloatValue() : proc.apvts.getRawParameterValue ("length")->load() + 1.0f;
    const int n = (int) rd->lengthInSamples, total = n + (int) (tail * sr);
    juce::AudioBuffer<float> in (2, n); rd->read (&in, 0, n, 0, true, true);
    if (rd->numChannels == 1) in.copyFrom (1, 0, in, 0, 0, n);
    juce::AudioBuffer<float> out (2, total), io (2, block); juce::MidiBuffer midi;
    // warm up silently so smoothing settles
    // run silence so the convolution engine swaps in the new IR and smoothing settles
    for (int i = 0; i < (int) (3.0 * sr / block); ++i) { io.clear(); proc.processBlock (io, midi); if (i % 20 == 0) juce::Thread::sleep (5); }
    for (int pos = 0; pos < total; pos += block)
    {
        const int len = juce::jmin (block, total - pos);
        io.clear();
        for (int c = 0; c < 2; ++c) if (pos < n) io.copyFrom (c, 0, in, c, pos, juce::jmin (len, n - pos));
        proc.processBlock (io, midi);
        for (int c = 0; c < 2; ++c) out.copyFrom (c, pos, io, c, 0, len);
    }
    juce::File f (a[2]); f.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> os (f.createOutputStream().release());
    std::unique_ptr<juce::AudioFormatWriter> wr (wav.createWriterFor (os.get(), sr, 2, 32, {}, 0));
    if (! wr) return 1;
    os.release();
    wr->writeFromAudioSampleBuffer (out, 0, total);
    std::cout << a[0] << " -> " << f.getFileName() << "  peak " << juce::Decibels::gainToDecibels (out.getMagnitude (0, total)) << " dB\n";
    return 0;
}
