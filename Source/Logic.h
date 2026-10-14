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

struct Paths { bool dry = false, filtered = false, delayed = false, convolved = false; };
inline Paths getPaths(Routing p) noexcept {
    if (p.direct) return {true, false, false, false};
    return {!p.conv, p.filter, p.delay, p.conv};
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
