#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace aeryther {
// Deterministic, portable rules used by the plugin and regression tests.
struct Defaults {
    static constexpr float unityGainDecibels = 0.0f;
    static constexpr float neutralPitchSemitones = 0.0f;
    static constexpr float neutralFineCents = 0.0f;
    static constexpr float neutralDetune = 0.0f;
    static constexpr float neutralModDepth = 0.0f;
    static constexpr float convolutionMix = 1.0f;
    static constexpr float reverbMix = 1.0f;
    static constexpr float reverbDecay = 1.0f;
    static constexpr bool loopPlay = false;
};

struct Routing {
    bool filter = false, delay = false, conv = true, direct = false;
};

// Direct is an independent dry tap. The three effect flags form one serial
// processed branch in the order FILTER -> DELAY -> CONV; this is deliberately
// not three separately mixed effect sends. With all flags off, output is silent.
struct Paths {
    bool dry=false,filtered=false,delayed=false,convolved=false;
    bool hasProcessedPath() const noexcept {
        return filtered || delayed || convolved;
    }
    bool audible() const noexcept {
        return dry || hasProcessedPath();
    }
};
inline Paths getPaths(Routing p) noexcept {
    return {p.direct,p.filter,p.delay,p.conv};
}
inline bool midiTriggersSample(bool loopPlay) noexcept { return !loopPlay; }
inline bool midiTriggersWave(bool) noexcept { return true; }
inline bool midiTriggersSub(bool) noexcept { return true; }
inline bool sampleLoops(bool loopPlay, bool hostPlaying) noexcept { return loopPlay && hostPlaying; }
enum class SamplePlaybackMode { Silent, HostLoop, Midi };
inline SamplePlaybackMode samplePlaybackMode(bool loopPlay, bool hostPlaying) noexcept {
    return loopPlay ? (hostPlaying ? SamplePlaybackMode::HostLoop : SamplePlaybackMode::Silent)
                    : SamplePlaybackMode::Midi;
}
inline float wetDry(float dry, float wet, float mix) noexcept {
    const auto m = std::clamp(mix, 0.0f, 1.0f);
    return dry * (1.0f - m) + wet * m;
}
struct ReverbGains { float dry,wet; };
// Equal-power mix plus 6.85dB tail gain compensation. 0% is exactly dry;
// 100% is genuinely wet-only, preserving reverb-only sound design.
inline ReverbGains reverbGains(float mix) noexcept {
    constexpr float halfPi=1.57079632679489661923f;
    const float m=std::clamp(mix,0.0f,1.0f);
    return {std::cos(halfPi*m),2.2f*std::sin(halfPi*m)};
}
// Original playback tempo and transposition are independent. The grain
// anchor follows tempo; this ratio only changes pitch WITHIN each grain.
inline double samplerPitchRatio(double semitones) noexcept {
    return std::exp2(std::clamp(semitones,-48.0,48.0)/12.0);
}
// Steady grain boundaries prevent clicks when the CROSSFADE knob moves.
// Higher CROSSFADE produces a broader, gently overlapping Hann window.
inline float samplerGrainWeight(double age,double grainLength,float crossfade) noexcept {
    if(grainLength<=0 || age<=0 || age>=grainLength)return 0.f;
    constexpr double pi=3.14159265358979323846;
    const double hann=0.5-0.5*std::cos(2*pi*age/grainLength);
    const double width=std::clamp(double(crossfade),0.0,0.5);
    return float(std::pow(hann, 8.0-14.4*width));
}
// Number of quarter-note beats per delay subdivision.
inline double delayDivisionBeats(int index) noexcept {
    constexpr double beats[]{4.0,2.0,1.0,0.5,0.25,0.125,
                              1.5,0.75,0.375,2.0/3.0,1.0/3.0,1.0/6.0};
    return beats[std::clamp(index,0,11)];
}
// Glitch-free tempo changes: keep two *fixed* delay taps and smoothly
// crossfade between them instead of jumping the ring-buffer read head.
// No allocations or pitch bending in the realtime thread.
struct DelayTimeCrossfade {
    int fromSamples=0,toSamples=0,remaining=0,total=0;
    void reset() noexcept {fromSamples=toSamples=remaining=total=0;}
    void request(int requested,int fadeSamples) noexcept {
        requested=std::max(1,requested);
        if(fromSamples==0){fromSamples=toSamples=requested;return;}
        // Finish the active crossfade before starting a new one. This avoids
        // discontinuities when TIME moves repeatedly during a knob gesture.
        if(remaining>0 || requested==fromSamples)return;
        toSamples=requested;
        total=std::max(2,fadeSamples);
        remaining=total;
    }
    bool isCrossfading() const noexcept {return remaining>0;}
    float blend() const noexcept {
        if(remaining<=0)return 0.f;
        const float t=1.f-float(remaining)/float(total);
        return t*t*(3.f-2.f*t); // smoothstep: zero slope at both boundaries
    }
    void advance() noexcept {
        if(remaining>0 && --remaining==0)fromSamples=toSamples;
    }
};
inline double tempoDelaySeconds(double bpm,int division) noexcept {
    const double safeBpm=std::clamp(bpm,30.0,300.0);
    return 60.0*delayDivisionBeats(division)/safeBpm;
}
// Only these two stage limits can be selected: the pitch-shifted feedback
// never creates content above the selected octave ceiling.
inline int shimmerOctaveCeiling(int selected) noexcept {
    return std::clamp(selected,2,3);
}
// Rate modulation is expressed in octaves. This works for free-running
// frequencies and tempo-synced note divisions alike. ±2 octaves => 1/4..4x.
inline double lfoRateMultiplier(double octaves) noexcept {
    return std::exp2(std::clamp(octaves,-2.0,2.0));
}
// Stateless deterministic sample-and-hold. The value changes only when the
// (transport or free-running) LFO completes a cycle; no global RNG on audio thread.
inline float lfoRandomForCycle(int64_t cycle,int index) noexcept {
    uint64_t v=static_cast<uint64_t>(cycle) ^
        (0x9e3779b97f4a7c15ULL * static_cast<uint64_t>(index+1));
    v^=v>>30; v*=0xbf58476d1ce4e5b9ULL;
    v^=v>>27; v*=0x94d049bb133111ebULL;
    v^=v>>31;
    return float((double(v>>32)/4294967295.0)*2.0-1.0);
}
inline float shimmerGrainWindow(float phase) noexcept {
    constexpr float pi=3.14159265358979323846f;
    const float s=std::sin(pi*phase);
    return s*s;
}
inline double wrapped(double position, double length) noexcept {
    if (length <= 0.0) return 0.0;
    const auto result = std::fmod(position, length);
    return result < 0 ? result + length : result;
}
inline double sampleReadPosition(double beatPosition, double sourceBpm, double sourceRate,
                                 double start, double end, int direction) noexcept {
    const auto length = end - start;
    if (length <= 0.0 || sourceBpm <= 0.0 || sourceRate <= 0.0) return start;
    const auto playedSamples = beatPosition * 60.0 / sourceBpm * sourceRate;
    if (direction == 1) return end - wrapped(playedSamples, length); // reverse
    if (direction == 2) { // ping-pong: no repeats of abrupt endpoint jumps
        const auto t = wrapped(playedSamples, 2.0 * length);
        return start + (t <= length ? t : 2.0 * length - t);
    }
    return start + wrapped(playedSamples, length); // forward
}
} // namespace aeryther
