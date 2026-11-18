// Aftershock, a reverb made from earthquakes. (C) 2026 Sofia Vallejo Budziszewski, AGPL-3.0.
// Free to port, by humans. Bring your own ears. See PORTING.md.

#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "EventPack.h"
#include "IRGenerator.h"
#include "PresetManager.h"

class AftershockProcessor : public juce::AudioProcessor,
                            private juce::AudioProcessorValueTreeState::Listener,
                            private juce::Thread
{
public:
    AftershockProcessor();
    ~AftershockProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Aftershock"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 12.0; }
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int) override;
    const juce::String getProgramName (int) override;

    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::UndoManager undoManager;
    juce::AudioProcessorValueTreeState apvts;
    const aftershock::EventPack& pack;
    PresetManager presets;
    std::atomic<float> inMeter { 0 }, outMeter { 0 };   // peak since the editor last read it
    void requestIRPublic() { requestIR(); }

    // what the editor shows
    struct View
    {
        int eventIndex = 0, stationIndex = 0;
        float windowStart = 0, windowEnd = 0, compression = 1;
        std::vector<float> bandDecay;
        juce::AudioBuffer<float> ir;
        double irRate = 48000;
    };
    View getView() const { const juce::SpinLock::ScopedLockType l (viewLock); return view; }
    std::atomic<int> viewVersion { 0 };
    std::atomic<bool> rendering { false };

    int currentEventIndex() const;
    int stationIndexFor (int eventIndex) const;
    aftershock::Trace traceFor (int eventIndex, int stationIndex) const;

    static juce::StringArray stationBandNames() { return { "Near", "Regional", "Teleseismic" }; }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout (const aftershock::EventPack&);
    void parameterChanged (const juce::String&, float) override;
    void run() override;                // background IR renderer
    void requestIR() { irDirty = true; notify(); }

    float p (const char* id) const { return apvts.getRawParameterValue (id)->load(); }

    juce::dsp::Convolution convolution { juce::dsp::Convolution::NonUniform { 512 } };
    std::atomic<bool> irDirty { true };
    std::atomic<double> sampleRateAtomic { 48000 };

    // early reflections from the station's real P-to-S arrivals
    struct TapSet { std::array<float, 8> pos {}, gainL {}, gainR {}; int n = 0; };
    TapSet taps, pendingTaps;
    juce::SpinLock tapLock;
    std::atomic<bool> tapsChanged { false };

    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> erLine { 1 << 16 };
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> preDelay { 1 << 16 };
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Lagrange3rd> modLine { 1 << 12 };
    std::array<juce::dsp::IIR::Filter<float>, 4> lowCut;      // 2 per channel, 12 dB/oct each = 24 dB/oct
    std::array<juce::dsp::IIR::Filter<float>, 2> tiltLow, tiltHigh;
    float lastLowCut = -1, lastTilt = 999;
    double lfoPhase = 0;
    float duckEnv = 0;

    // freeze: loop the last 1.5 s of the wet signal with two crossfading readers
    juce::AudioBuffer<float> freezeBuf;
    int freezeWrite = 0, freezeLen = 0;
    double freezeRead = 0;
    float freezeMix = 0;

    juce::SmoothedValue<float> dryGain, wetGain, erGain, widthS, freezeTarget, outGain, bypassMix;

    juce::AudioBuffer<float> wet, dryCopy;
    double sr = 48000;

    mutable juce::SpinLock viewLock;
    View view;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AftershockProcessor)
};
