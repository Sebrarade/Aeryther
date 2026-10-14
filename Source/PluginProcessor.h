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

    bool loadSample(const juce::File& file);
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
    struct SynthVoice {
        bool active = false; int note = -1;
        juce::ADSR envelope;
        std::array<double, 8> phases {};
        double samplePosition = 0.0;
        double glidePitch = -1.0;
    };
    struct DelayLine {
        std::array<std::vector<float>, 2> ring;
        std::array<std::vector<float>, 2> pitchRing;
        int cursor = 0, pitchCursor = 0;
        double grainPhase = 0.0;
        float shimmerSmoothed = 0.0f;
        void prepare(int maximumSamples, double sampleRate);
        void process(juce::AudioBuffer<float>& buffer, float seconds, float feedback,
                     double sampleRate, bool pingPong, float shimmer);
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
    mutable juce::CriticalSection fileNameMutex;
    juce::String loadedSamplePath;
    std::array<SynthVoice, 24> waveVoices, sampleVoices;
    std::array<bool, 128> heldNotes {};
    std::array<double, 8> subPhases {};
    std::array<DelayLine, 3> layerDelays;
    std::array<LayerFilter, 3> layerFilters;
    std::array<juce::AudioBuffer<float>, 3> layerAudio;
    juce::AudioBuffer<float> convInput, processed, convWet, wetScratch;
    // Four original hall modes, each with NORMAL and REVERSE impulse responses.
    std::array<juce::dsp::Convolution, 8> convolutions;
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
