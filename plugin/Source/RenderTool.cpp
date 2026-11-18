// Aftershock, a reverb made from earthquakes. (C) 2026 Sofia Vallejo Budziszewski, AGPL-3.0.
// Free to port, by humans. Bring your own ears. See PORTING.md.

// aftershock_render: renders impulse responses from the bundled pack to WAV files,
// without a DAW. Used for testing and for building listening-test stimuli.
//
//   aftershock_render --list
//   aftershock_render OUTDIR [--length 3] [--window full|p|s|surface] [--spectrum 0.35]
//                     [--contrast 1] [--grit 0.1] [--damping 0.15] [--rate 48000] [--event ID]
#include <juce_audio_formats/juce_audio_formats.h>
#include "EventPack.h"
#include "IRGenerator.h"

using namespace aftershock;

int main (int argc, char* argv[])
{
    juce::StringArray args;
    for (int i = 1; i < argc; ++i) args.add (argv[i]);
    const auto& pk = EventPack::get();
    if (! pk.isLoaded()) { std::cerr << "pack failed to load\n"; return 1; }

    auto opt = [&] (const juce::String& k, const juce::String& def) { const int i = args.indexOf (k); return i >= 0 && i + 1 < args.size() ? args[i + 1] : def; };

    if (args.contains ("--list"))
    {
        for (auto& e : pk.events)
        {
            std::cout << e.id << "\t" << e.name << "\tM" << e.mag << "\t" << pk.settingLabel (e.setting) << "\n";
            for (auto& s : e.stations)
            {
                std::cout << "    " << s.band << "\t" << s.label() << "\t" << (int) s.distKm << " km\tRT60:";
                for (auto& b : s.bands) std::cout << " " << (b.rt60 > 0 ? juce::String (juce::roundToInt (b.rt60)) : juce::String ("-"));
                std::cout << "\n";
            }
        }
        return 0;
    }
    if (args.isEmpty()) { std::cerr << "usage: aftershock_render OUTDIR [options] | --list\n"; return 1; }

    juce::File out (juce::File::getCurrentWorkingDirectory().getChildFile (args[0]));
    out.createDirectory();
    IRParams p;
    p.lengthS = opt ("--length", "3").getFloatValue();
    p.spectrum = opt ("--spectrum", "0.35").getFloatValue();
    p.decayContrast = opt ("--contrast", "1").getFloatValue();
    p.grit = opt ("--grit", "0.1").getFloatValue();
    p.damping = opt ("--damping", "0.15").getFloatValue();
    const auto w = opt ("--window", "full");
    p.window = w == "p" ? WindowStart::p : w == "s" ? WindowStart::s : w == "surface" ? WindowStart::surface : WindowStart::full;
    const double rate = opt ("--rate", "48000").getDoubleValue();
    const auto only = opt ("--event", "");

    juce::WavAudioFormat wav;
    for (auto& e : pk.events)
    {
        if (only.isNotEmpty() && e.id != only) continue;
        for (auto& s : e.stations)
        {
            const auto t0 = juce::Time::getMillisecondCounterHiRes();
            auto tr = pk.readTrace (s);
            auto res = generateIR (tr, p, rate);
            const auto ms = juce::Time::getMillisecondCounterHiRes() - t0;
            auto f = out.getChildFile (e.id + "_" + s.band + "_" + s.net + "." + s.sta + ".wav");
            f.deleteFile();
            std::unique_ptr<juce::FileOutputStream> os (f.createOutputStream().release());
            std::unique_ptr<juce::AudioFormatWriter> wr (wav.createWriterFor (os.get(), rate, 2, 32, {}, 0));
            if (wr) { os.release(); wr->writeFromAudioSampleBuffer (res.ir, 0, res.ir.getNumSamples()); }
            float pk_ = res.ir.getMagnitude (0, res.ir.getNumSamples());
            std::cout << f.getFileName() << "\tk=" << juce::roundToInt (res.compression) << "x\tpeak=" << pk_
                      << "\trender=" << juce::roundToInt (ms) << " ms\n";
        }
    }
    return 0;
}
