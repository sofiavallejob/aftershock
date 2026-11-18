// Aftershock, a reverb made from earthquakes. (C) 2026 Sofia Vallejo Budziszewski, AGPL-3.0.
// Free to port, by humans. Bring your own ears. See PORTING.md.

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Theme.h"

using namespace aftershock;

namespace
{
const juce::StringArray irParams { "event", "station", "window", "channels", "length", "damping", "spectrum", "contrast", "grit", "reverse" };
}

juce::AudioProcessorValueTreeState::ParameterLayout AftershockProcessor::createLayout (const EventPack& pk)
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> ps;
    auto pct = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (juce::roundToInt (v * 100)) + " %"; });

    const int nEvents = std::max (1, (int) pk.events.size());
    int defaultEvent = 0;   // Petrinja 2020: closest to Graz, recorded at all three distances
    for (int i = 0; i < (int) pk.events.size(); ++i) if (pk.events[(size_t) i].id == "petrinja2020") defaultEvent = i;
    ps.push_back (std::make_unique<AudioParameterInt> (ParameterID { "event", 1 }, "Earthquake", 0, nEvents - 1, defaultEvent,
        AudioParameterIntAttributes().withStringFromValueFunction ([&pk] (int v, int)
        {
            return v >= 0 && v < (int) pk.events.size() ? pk.events[(size_t) v].name + " " + String (pk.events[(size_t) v].time.substring (0, 4)) : String (v);
        })));
    ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "station", 1 }, "Station", stationBandNames(), 0));
    ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "window", 1 }, "Window start", StringArray { "Full", "P wave", "S wave", "Surface waves" }, 0));
    ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "channels", 1 }, "Channels", StringArray { "North | East", "Vertical", "All three" }, 0));
    ps.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "length", 1 }, "Length", NormalisableRange<float> (0.3f, 12.0f, 0.01f, 0.45f), 2.8f,
        AudioParameterFloatAttributes().withLabel ("s").withStringFromValueFunction ([] (float v, int) { return String (v, 2) + " s"; })));
    ps.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "damping", 1 }, "Damping", NormalisableRange<float> (0, 1), 0.15f, pct));
    ps.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "spectrum", 1 }, "Spectrum", NormalisableRange<float> (0, 1), 0.35f, pct));
    ps.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "contrast", 1 }, "Decay contrast", NormalisableRange<float> (0, 3), 1.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (v, 2) + "x"; })));
    ps.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "grit", 1 }, "Grit", NormalisableRange<float> (0, 1), 0.1f, pct));
    ps.push_back (std::make_unique<AudioParameterBool> (ParameterID { "reverse", 1 }, "Reverse", false));

    ps.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "early", 1 }, "Early arrivals", NormalisableRange<float> (0, 1), 0.3f, pct));
    ps.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "spread", 1 }, "Early spread", NormalisableRange<float> (10, 150), 55.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (juce::roundToInt (v)) + " ms"; })));
    ps.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "predelay", 1 }, "Pre-delay", NormalisableRange<float> (0, 250, 0.1f, 0.6f), 12.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (juce::roundToInt (v)) + " ms"; })));
    ps.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "lowcut", 1 }, "Low cut", NormalisableRange<float> (20, 600, 1, 0.4f), 110.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (juce::roundToInt (v)) + " Hz"; })));
    ps.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "tilt", 1 }, "Tilt", NormalisableRange<float> (-1, 1), 0.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return v < -0.02f ? "dark " + String (juce::roundToInt (-v * 100)) : v > 0.02f ? "bright " + String (juce::roundToInt (v * 100)) : String ("flat"); })));
    ps.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "mod", 1 }, "Modulation", NormalisableRange<float> (0, 1), 0.25f, pct));
    ps.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "width", 1 }, "Width", NormalisableRange<float> (0, 2), 1.2f, pct));
    ps.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "duck", 1 }, "Ducking", NormalisableRange<float> (0, 1), 0.0f, pct));
    ps.push_back (std::make_unique<AudioParameterBool> (ParameterID { "freeze", 1 }, "Freeze", false));
    ps.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "dry", 1 }, "Dry", NormalisableRange<float> (0, 1), 1.0f, pct));
    ps.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "wet", 1 }, "Wet", NormalisableRange<float> (0, 1), 0.35f, pct));
    ps.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "output", 1 }, "Output gain", NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f,
        AudioParameterFloatAttributes().withLabel ("dB").withStringFromValueFunction ([] (float v, int) { return (v > 0.04f ? "+" : "") + String (v, 1) + " dB"; })));
    ps.push_back (std::make_unique<AudioParameterBool> (ParameterID { "bypass", 1 }, "Soft bypass", false));
    return { ps.begin(), ps.end() };
}

AftershockProcessor::AftershockProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      juce::Thread ("Aftershock IR renderer"),
      apvts (*this, &undoManager, "AftershockState", createLayout (EventPack::get())),
      pack (EventPack::get()),
      presets (*this)
{
    for (auto& id : irParams)
        apvts.addParameterListener (id, this);
    if (ui::Settings::get().openLast())
        if (auto* last = presets.find (ui::Settings::get().getRaw ("lastPreset", "").toString()))
            presets.load (*last);
    undoManager.clearUndoHistory();
    startThread (juce::Thread::Priority::low);
}

AftershockProcessor::~AftershockProcessor()
{
    for (auto& id : irParams)
        apvts.removeParameterListener (id, this);
    signalThreadShouldExit();
    notify();
    stopThread (4000);
}

bool AftershockProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    const auto in = layouts.getMainInputChannelSet();
    return out == juce::AudioChannelSet::stereo()
        && (in == juce::AudioChannelSet::stereo() || in == juce::AudioChannelSet::mono());
}

int AftershockProcessor::currentEventIndex() const
{
    return juce::jlimit (0, std::max (0, (int) pack.events.size() - 1), (int) p ("event"));
}

int AftershockProcessor::stationIndexFor (int e) const
{
    if (e < 0 || e >= (int) pack.events.size()) return 0;
    const auto& st = pack.events[(size_t) e].stations;
    static const juce::StringArray keys { "near", "regional", "tele" };
    const int want = (int) p ("station");
    int best = 0, bestD = 99;
    for (int i = 0; i < (int) st.size(); ++i)
    {
        const int d = std::abs (keys.indexOf (st[(size_t) i].band) - want);
        if (d < bestD) { bestD = d; best = i; }
    }
    return best;
}

Trace AftershockProcessor::traceFor (int e, int s) const
{
    if (e < 0 || e >= (int) pack.events.size()) return {};
    return pack.readTrace (pack.events[(size_t) e].stations[(size_t) s]);
}

void AftershockProcessor::parameterChanged (const juce::String&, float) { requestIR(); }

void AftershockProcessor::run()
{
    while (! threadShouldExit())
    {
        wait (200);
        if (! irDirty.load() || pack.events.empty()) continue;

        // debounce: wait until parameters have been still for 80 ms
        do { irDirty = false; wait (80); } while (irDirty.load() && ! threadShouldExit());
        if (threadShouldExit()) break;

        rendering = true;
        const int e = currentEventIndex(), s = stationIndexFor (e);
        const auto& station = pack.events[(size_t) e].stations[(size_t) s];
        const auto tr = pack.readTrace (station);

        IRParams ip;
        ip.window = (WindowStart) (int) p ("window");
        ip.channels = (Channels) (int) p ("channels");
        ip.lengthS = p ("length");
        ip.damping = p ("damping");
        ip.spectrum = p ("spectrum");
        ip.decayContrast = p ("contrast");
        ip.grit = p ("grit");
        ip.reverse = p ("reverse") > 0.5f;
        ip.seed = (juce::uint32) (e * 31 + s + 1);

        auto res = generateIR (tr, ip, sampleRateAtomic.load());

        // early reflections
        TapSet ts;
        double norm = 0;
        for (auto& t : station.taps)
        {
            if (ts.n >= 8) break;
            ts.pos[(size_t) ts.n] = t.position;
            ts.gainL[(size_t) ts.n] = t.gain * std::sqrt (1.0f - juce::jlimit (0.0f, 1.0f, t.pan));
            ts.gainR[(size_t) ts.n] = t.gain * std::sqrt (juce::jlimit (0.0f, 1.0f, t.pan));
            norm += (double) t.gain * t.gain;
            ++ts.n;
        }
        if (norm > 0)
            for (int i = 0; i < ts.n; ++i) { ts.gainL[(size_t) i] /= (float) std::sqrt (norm); ts.gainR[(size_t) i] /= (float) std::sqrt (norm); }
        { const juce::SpinLock::ScopedLockType l (tapLock); pendingTaps = ts; tapsChanged = true; }

        {
            const juce::SpinLock::ScopedLockType l (viewLock);
            view.eventIndex = e; view.stationIndex = s;
            view.windowStart = res.windowStart; view.windowEnd = res.windowEnd; view.compression = res.compression;
            view.bandDecay = res.bandDecayDbPerS;
            view.ir.makeCopyOf (res.ir);
            view.irRate = res.sampleRate;
        }

        convolution.loadImpulseResponse (std::move (res.ir), res.sampleRate,
                                         juce::dsp::Convolution::Stereo::yes,
                                         juce::dsp::Convolution::Trim::no,
                                         juce::dsp::Convolution::Normalise::no);
        rendering = false;
        ++viewVersion;
    }
}

void AftershockProcessor::prepareToPlay (double sampleRate, int block)
{
    sr = sampleRate;
    const bool rateChanged = std::abs (sampleRateAtomic.load() - sampleRate) > 1;
    sampleRateAtomic = sampleRate;

    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) block, 2 };
    convolution.prepare (spec);
    erLine.prepare ({ sampleRate, (juce::uint32) block, 1 });
    preDelay.prepare (spec);
    modLine.prepare (spec);
    erLine.setMaximumDelayInSamples ((int) (0.4 * sampleRate));
    preDelay.setMaximumDelayInSamples ((int) (0.4 * sampleRate));
    modLine.setMaximumDelayInSamples ((int) (0.05 * sampleRate));
    for (auto& f : lowCut) f.reset();
    for (auto& f : tiltLow) f.reset();
    for (auto& f : tiltHigh) f.reset();
    lastLowCut = -1; lastTilt = 999;

    freezeLen = (int) (1.5 * sampleRate);
    freezeBuf.setSize (2, freezeLen);
    freezeBuf.clear();
    freezeWrite = 0; freezeRead = 0; freezeMix = 0;

    for (auto* s : { &dryGain, &wetGain, &erGain, &widthS, &freezeTarget, &outGain, &bypassMix })
        s->reset (sampleRate, 0.03);
    outGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (p ("output")));
    bypassMix.setCurrentAndTargetValue (p ("bypass") > 0.5f ? 0.0f : 1.0f);
    dryGain.setCurrentAndTargetValue (p ("dry"));
    wetGain.setCurrentAndTargetValue (p ("wet"));
    erGain.setCurrentAndTargetValue (p ("early"));
    widthS.setCurrentAndTargetValue (p ("width"));
    freezeTarget.reset (sampleRate, 0.25);

    wet.setSize (2, block);
    dryCopy.setSize (2, block);
    setLatencySamples (convolution.getLatency());
    if (rateChanged || irDirty) requestIR();
}

void AftershockProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int inCh = getTotalNumInputChannels();
    if (inCh == 1) buffer.copyFrom (1, 0, buffer, 0, 0, n);
    if (wet.getNumSamples() < n) { wet.setSize (2, n, false, false, true); dryCopy.setSize (2, n, false, false, true); }

    if (tapsChanged.exchange (false))
    {
        const juce::SpinLock::ScopedTryLockType l (tapLock);
        if (l.isLocked()) taps = pendingTaps; else tapsChanged = true;
    }

    // filter coefficients only when their controls move
    const float lc = p ("lowcut"), tilt = p ("tilt");
    if (std::abs (lc - lastLowCut) > 0.5f)
    {
        auto c = juce::dsp::IIR::Coefficients<float>::makeHighPass (sr, lc, 0.7071f);
        for (auto& f : lowCut) f.coefficients = c;
        lastLowCut = lc;
    }
    if (std::abs (tilt - lastTilt) > 0.005f)
    {
        const float g = juce::Decibels::decibelsToGain (tilt * 6.0f);
        auto lo = juce::dsp::IIR::Coefficients<float>::makeLowShelf (sr, 600.0, 0.7071f, 1.0f / g);
        auto hi = juce::dsp::IIR::Coefficients<float>::makeHighShelf (sr, 2500.0, 0.7071f, g);
        for (auto& f : tiltLow) f.coefficients = lo;
        for (auto& f : tiltHigh) f.coefficients = hi;
        lastTilt = tilt;
    }

    dryGain.setTargetValue (p ("dry"));
    wetGain.setTargetValue (p ("wet"));
    erGain.setTargetValue (p ("early"));
    widthS.setTargetValue (p ("width"));
    freezeTarget.setTargetValue (p ("freeze") > 0.5f ? 1.0f : 0.0f);
    outGain.setTargetValue (juce::Decibels::decibelsToGain (p ("output")));
    bypassMix.setTargetValue (p ("bypass") > 0.5f ? 0.0f : 1.0f);
    {
        float pk = 0;
        for (int ch = 0; ch < 2; ++ch) pk = juce::jmax (pk, buffer.getMagnitude (ch, 0, n));
        inMeter = juce::jmax (inMeter.load(), pk);
    }

    for (int ch = 0; ch < 2; ++ch)
    {
        dryCopy.copyFrom (ch, 0, buffer, ch, 0, n);
        wet.copyFrom (ch, 0, buffer, ch, 0, n);
    }

    // convolution tail
    {
        auto block = juce::dsp::AudioBlock<float> (wet).getSubBlock (0, (size_t) n);
        convolution.process (juce::dsp::ProcessContextReplacing<float> (block));
    }

    const float preSamples = p ("predelay") * 0.001f * (float) sr;
    const float spreadSamples = p ("spread") * 0.001f * (float) sr;
    const float modDepth = p ("mod") * 0.0025f * (float) sr;   // up to 2.5 ms
    const float duckAmt = p ("duck");
    const float atk = std::exp (-1.0f / (0.005f * (float) sr)), rel = std::exp (-1.0f / (0.25f * (float) sr));
    const double lfoInc = juce::MathConstants<double>::twoPi * 0.21 / sr;

    auto* wl = wet.getWritePointer (0);
    auto* wr = wet.getWritePointer (1);
    auto* dl = dryCopy.getReadPointer (0);
    auto* dr = dryCopy.getReadPointer (1);
    auto* outL = buffer.getWritePointer (0);
    auto* outR = buffer.getWritePointer (1);

    for (int i = 0; i < n; ++i)
    {
        const float mono = 0.5f * (dl[i] + dr[i]);

        // pre-delay on the tail
        preDelay.pushSample (0, wl[i]); preDelay.pushSample (1, wr[i]);
        float l = preDelay.popSample (0, preSamples), r = preDelay.popSample (1, preSamples);

        // early arrivals
        erLine.pushSample (0, mono);
        const float eg = erGain.getNextValue();
        if (eg > 0.0001f)
            for (int t = 0; t < taps.n; ++t)
            {
                const float s = erLine.popSample (0, 1.0f + taps.pos[(size_t) t] * spreadSamples, false);
                l += eg * taps.gainL[(size_t) t] * s;
                r += eg * taps.gainR[(size_t) t] * s;
            }
        erLine.popSample (0, 0.0f, true);   // advance once per sample

        // low cut (24 dB/oct) and tilt
        l = lowCut[1].processSample (lowCut[0].processSample (l));
        r = lowCut[3].processSample (lowCut[2].processSample (r));
        l = tiltHigh[0].processSample (tiltLow[0].processSample (l));
        r = tiltHigh[1].processSample (tiltLow[1].processSample (r));

        // slow modulation: two LFOs a quarter cycle apart
        lfoPhase += lfoInc; if (lfoPhase > juce::MathConstants<double>::twoPi) lfoPhase -= juce::MathConstants<double>::twoPi;
        modLine.pushSample (0, l); modLine.pushSample (1, r);
        const float base = 0.004f * (float) sr;
        l = modLine.popSample (0, base + modDepth * (float) std::sin (lfoPhase));
        r = modLine.popSample (1, base + modDepth * (float) std::cos (lfoPhase * 1.31));

        // width (mid/side)
        const float w = widthS.getNextValue();
        const float m = 0.5f * (l + r), s = 0.5f * (l - r) * w;
        l = m + s; r = m - s;

        // freeze
        const float fz = freezeTarget.getNextValue();
        if (fz < 0.999f)
        {
            freezeBuf.setSample (0, freezeWrite, l);
            freezeBuf.setSample (1, freezeWrite, r);
            freezeWrite = (freezeWrite + 1) % freezeLen;
        }
        if (fz > 0.001f)
        {
            freezeRead += 1.0; if (freezeRead >= freezeLen) freezeRead -= freezeLen;
            const double pa = freezeRead, pb = std::fmod (freezeRead + freezeLen * 0.5, (double) freezeLen);
            const float wa = 0.5f - 0.5f * (float) std::cos (juce::MathConstants<double>::twoPi * pa / freezeLen);
            const float wb = 1.0f - wa;
            const int ia = (freezeWrite + (int) pa) % freezeLen, ib = (freezeWrite + (int) pb) % freezeLen;
            const float fl = wa * freezeBuf.getSample (0, ia) + wb * freezeBuf.getSample (0, ib);
            const float fr = wa * freezeBuf.getSample (1, ia) + wb * freezeBuf.getSample (1, ib);
            l = l * (1.0f - fz) + fl * fz;
            r = r * (1.0f - fz) + fr * fz;
        }

        // ducking: the dry signal pushes the tail down, it blooms in the gaps
        const float a = std::abs (mono);
        duckEnv = a > duckEnv ? atk * duckEnv + (1 - atk) * a : rel * duckEnv + (1 - rel) * a;
        const float duck = 1.0f - duckAmt * juce::jlimit (0.0f, 1.0f, duckEnv * 4.0f);

        const float wg = wetGain.getNextValue() * duck, dg = dryGain.getNextValue();
        const float og = outGain.getNextValue(), bm = bypassMix.getNextValue();
        outL[i] = bm * og * (dl[i] * dg + l * wg) + (1.0f - bm) * dl[i];
        outR[i] = bm * og * (dr[i] * dg + r * wg) + (1.0f - bm) * dr[i];
    }
    {
        float pk = 0;
        for (int ch = 0; ch < 2; ++ch) pk = juce::jmax (pk, buffer.getMagnitude (ch, 0, n));
        outMeter = juce::jmax (outMeter.load(), pk);
    }
}

juce::AudioProcessorEditor* AftershockProcessor::createEditor() { return new AftershockEditor (*this); }

void AftershockProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("presetName", presets.currentName(), nullptr);
    state.setProperty ("eventId", pack.events.empty() ? juce::String() : pack.events[(size_t) currentEventIndex()].id, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void AftershockProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto state = juce::ValueTree::fromXml (*xml);
            apvts.replaceState (state);
            // tell the host every value again, so its cache matches (bools a host set to in-between values included)
            for (auto* prm : getParameters())
                if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (prm))
                    r->setValueNotifyingHost (r->convertTo0to1 (r->convertFrom0to1 (r->getValue())));
            // keep the same earthquake even if a newer pack reorders events
            const auto id = state.getProperty ("eventId").toString();
            for (int i = 0; i < (int) pack.events.size(); ++i)
                if (pack.events[(size_t) i].id == id)
                    if (auto* prm = apvts.getParameter ("event"))
                        prm->setValueNotifyingHost (prm->convertTo0to1 ((float) i));
            presets.setCurrentName (state.getProperty ("presetName", "Init").toString());
            requestIR();
        }
}

int AftershockProcessor::getNumPrograms() { return juce::jmax (1, presets.numFactory()); }
int AftershockProcessor::getCurrentProgram() { return juce::jmax (0, presets.currentFactoryIndex()); }
void AftershockProcessor::setCurrentProgram (int i) { presets.loadFactory (i); }
const juce::String AftershockProcessor::getProgramName (int i) { return presets.factoryName (i); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new AftershockProcessor(); }
