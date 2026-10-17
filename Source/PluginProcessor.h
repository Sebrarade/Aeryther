#pragma once
#include <JuceHeader.h>
#include "Logic.h"
#include "AerytherSpaciousReverb.h"
#include "AerytherConvolverEQ.h"
#include <array>
#include <atomic>
#include <memory>
#include <vector>

class AerytherAudioProcessor final : public juce::AudioProcessor {
public:
    AerytherAudioProcessor();
    ~AerytherAudioProcessor() override = default;
    void prepareToPlay(double sampleRate, int maximumExpectedSamplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override {
#if defined(AERYTHER_FX)
        return "Aeryther FX";
#else
        return "Aeryther";
#endif
    }
    bool acceptsMidi() const override {
#if defined(AERYTHER_FX)
        return false;
#else
        return true;
#endif
    }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 120.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "Init"; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;
    bool isBusesLayoutSupported(const BusesLayout&) const override;

    bool loadSample(const juce::File& file,bool resetLoopRange=true);
    void clearSample();
    bool hasSample() const noexcept {return bool(std::atomic_load(&sample));}
    uint64_t sampleContentRevision() const noexcept {
        return sampleRevision.load(std::memory_order_relaxed);
    }
    juce::String sampleFileName() const;
    std::vector<float> waveformPeaks(int columns) const;
    float sampleReadhead() const noexcept {return playbackFraction.load(std::memory_order_relaxed);}
    bool sampleHeadActive() const noexcept {return playbackActive.load(std::memory_order_relaxed);}
    juce::AudioProcessorValueTreeState state;
    static juce::AudioProcessorValueTreeState::ParameterLayout makeLayout();

private:
    struct SampleData {
        juce::AudioBuffer<float> samples;
        double rate = 44100.0;
        juce::String path;
    };
    struct SamplerGrain {
        struct Grain { double source=0.0, pitchRatio=1.0; bool active=false; };
        Grain current, previous;
        double sourcePosition=0.0;
        int clock=0;
        void reset(double position=0.0) noexcept {
            current={}; previous={};sourcePosition=position;clock=0;
        }
    };
    struct SynthVoice {
        SamplerGrain samplerGrain;
        bool active = false; int note = -1;
        juce::ADSR envelope;
        std::array<double, 8> phases {};
        double samplePosition = 0.0;
        double glidePitch = -1.0;
    };
    struct DelayLine {
        // Four discrete echo levels: fundamental, +12, +24 and +36 semitones.
        // Each level has its own feedback loop; the highest enabled level
        // recirculates WITHOUT further pitch shifting, so pitch cannot run away.
        std::array<std::array<std::vector<float>,2>,4> octaveRings;
        std::array<std::array<std::vector<float>,2>,3> grainHistory;
        int cursor=0, grainCursor=0;
        double grainPhase=0.0;
        float shimmerSmoothed=0.0f;
        aeryther::DelayTimeCrossfade timeCrossfade;
        void prepare(int maximumSamples,double sampleRate);
        void process(juce::AudioBuffer<float>& buffer,float seconds,float feedback,
                     double sampleRate,bool pingPong,float shimmer,int maxOctaves);
    };
    struct LongReverb {
        struct Line { std::vector<float> samples; int head = 0; float damped = 0; };
        std::array<Line, 4> lines;
        double sampleRate = 44100.0;
        void prepare(double newSampleRate);
        void process(juce::AudioBuffer<float>& audio, float decaySeconds);
    };
    struct LayerFilter {
        std::array<juce::dsp::StateVariableTPTFilter<float>, 2> svf;
        std::array<std::vector<float>, 2> comb;
        std::array<int, 2> cursor {};
        void prepare(double rate, int maximumBlock);
        void process(juce::AudioBuffer<float>& buffer, int kind, float cutoff, float resonance,
                     float drive, float mix, double rate);
    };

    std::shared_ptr<const SampleData> sample;
    std::atomic<uint64_t> sampleRevision{0};
    mutable juce::CriticalSection fileNameMutex;
    juce::String loadedSamplePath;
    std::array<SynthVoice, 24> waveVoices, sampleVoices;
    SamplerGrain loopSamplerGrain;
    const SampleData* activeSamplerData=nullptr;
    int64_t expectedSamplerFrame=-1;
    juce::SmoothedValue<float,juce::ValueSmoothingTypes::Linear> smoothStretch,smoothPitch,smoothCrossfade,smoothReverbMix;
    struct SampleControl { float stretch=1.f,pitch=0.f,crossfade=0.08f; };
    std::vector<SampleControl> sampleControls;
    std::array<bool, 128> heldNotes {};
    std::array<double, 8> subPhases {};
    std::array<DelayLine, 3> layerDelays;
    std::array<LayerFilter, 3> layerFilters;
    std::array<juce::AudioBuffer<float>, 3> layerAudio;
    juce::AudioBuffer<float> convInput, processed, convWet, wetScratch;
    // Four original hall modes, each with NORMAL and REVERSE impulse responses.
    // Two-stage non-uniform FFT: zero latency for the early IR while the
    // giant 34-54s tail is partitioned efficiently in 4096-sample blocks.
    // Preserves the complete original impulse responses, including REVERSE.
    std::array<juce::dsp::Convolution,8> convolutions{
        juce::dsp::Convolution{juce::dsp::Convolution::NonUniform{4096}},
        juce::dsp::Convolution{juce::dsp::Convolution::NonUniform{4096}},
        juce::dsp::Convolution{juce::dsp::Convolution::NonUniform{4096}},
        juce::dsp::Convolution{juce::dsp::Convolution::NonUniform{4096}},
        juce::dsp::Convolution{juce::dsp::Convolution::NonUniform{4096}},
        juce::dsp::Convolution{juce::dsp::Convolution::NonUniform{4096}},
        juce::dsp::Convolution{juce::dsp::Convolution::NonUniform{4096}},
        juce::dsp::Convolution{juce::dsp::Convolution::NonUniform{4096}}};
    AerytherSpaciousReverb reverb;
    AerytherConvolverEQ convolverEQ;
    juce::ADSR filterEnvelope, amplitudeModEnvelope;
    float currentFilterEnv = 0.0f, currentAmpModEnv = 0.0f;
    double currentSampleRate = 44100.0;
    int blockCapacity = 0;
    double subCurrentNote = -1;
    float subEnvelopeLevel = 0.0f; // sample-accurate anti-click fade for note-on/off
    double lastWaveNote = -1.0;
    double lfoPhases[2] {0.0, 0.0};
    // Cross-modulation uses PREVIOUS block's two outputs to break circular
    // dependencies. Both synced and free LFOs keep independent cycle counters.
    float previousLfoValues[2] {0.f,0.f};
    int64_t lfoFreeCycles[2] {0,0};
    double lfoSyncOffsets[2] {0.0,0.0};
    double lfoPreviousPpq[2] {0.0,0.0};
    bool lfoSyncWasOn[2] {false,false};
    uint64_t transportFrameCounter = 0;
    std::atomic<float> playbackFraction{0.f};
    std::atomic<bool> playbackActive{false};

    float get(const char* id) const;
    bool enabled(const char* id) const { return get(id) > 0.5f; }
    float modulated(const char* destination, float original, float lfo1, float lfo2) const;
    void renderOscillators(juce::AudioBuffer<float>& wave, juce::AudioBuffer<float>& sub,
                           int start, int length, float lfo1, float lfo2);
    void renderSampler(juce::AudioBuffer<float>& dest, int start, int length,
                       bool loopPlaying, double hostPpq, double hostBpm,
                       int64_t hostSampleStart, double sourceBpm, float lfo1, float lfo2);
    void applyRouting(juce::AudioBuffer<float>& output, int samples, float lfo1, float lfo2,
                      double hostBpm);
    void handleMidi(const juce::MidiMessage& m);
    void makeImpulseResponses(double sampleRate, int maxBlock);
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AerytherAudioProcessor)
};
