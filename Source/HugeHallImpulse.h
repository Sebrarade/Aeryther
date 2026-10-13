#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <random>
#include <vector>

// Original Aeryther IR generator. This is not an impulse response copied from
// Kilohearts, Fruity Convolver, or any recorded commercial library.
namespace aeryther {
struct HugeHallProfile {
    double durationSeconds;
    double preDelaySeconds;
    double bloomSeconds;
    double rt60Seconds;
    float lowWeight;
    float bodyWeight;
    float airWeight;
};
inline constexpr std::array<HugeHallProfile,4> hugeHallProfiles {{
    {34.0, 0.13, 0.85, 30.0, 0.25f,0.61f,0.28f}, // AUUU: soft luminous endless wash
    {42.0, 0.19, 1.30, 38.0, 0.31f,0.47f,0.35f}, // CROSS: wide, brighter diffusion
    {54.0, 0.31, 2.15, 50.0, 0.42f,0.57f,0.23f}, // CATHOLIC: monumental vaulted hall
    {46.0, 0.28, 1.75, 43.0, 0.67f,0.39f,0.06f}  // DARK: distant low-frequency cavern
}};
struct HallAllpass {
    std::vector<float> memory;
    size_t cursor=0;
    float coefficient=0.65f;
    float tick(float value) noexcept {
        const float old=memory[cursor];
        const float out=old-coefficient*value;
        memory[cursor]=value+coefficient*out;
        if(++cursor>=memory.size())cursor=0;
        return out;
    }
};
inline std::vector<float> generateHugeHallImpulse(int mode, int channel, double rate) {
    mode=std::clamp(mode,0,3);
    channel=std::clamp(channel,0,1);
    rate=std::clamp(rate,4000.0,48000.0);
    const auto profile=hugeHallProfiles[size_t(mode)];
    const size_t samples=size_t(std::ceil(profile.durationSeconds*rate));
    std::vector<float> out(samples,0.0f);
    std::minstd_rand rng(uint32_t(0x76A31u+uint32_t(mode)*1709u+
                                 uint32_t(channel)*65537u));
    const auto lpCoeff=[&](double hz) -> float {
        return float(1.0-std::exp(-6.283185307179586*
                        std::min(hz,rate*0.44)/rate));
    };
    const float bassCoeff=lpCoeff(360.0);
    const float bodyCoeff=lpCoeff(2800.0);
    const float airCoeff=lpCoeff(9800.0);
    const float dcCoeff=lpCoeff(25.0);
    float bass=0.f,body=0.f,air=0.f,dc=0.f;
    std::array<HallAllpass,5> diffusors;
    constexpr double delaySeconds[]={.0173,.0291,.0419,.0673,.0971};
    constexpr float feedbacks[]={.66f,.72f,.64f,.70f,.60f};
    for(size_t k=0;k<diffusors.size();++k) {
        const double extra=1.0+0.073*double(channel)+0.031*double(mode);
        const size_t len=size_t(std::max(2.0,std::round(delaySeconds[k]*rate*extra)));
        diffusors[k].memory.assign(len,0.f);
        diffusors[k].coefficient=feedbacks[k];
    }
    const double twoPi=6.283185307179586;
    for(size_t i=0;i<samples;++i) {
        const double time=double(i)/rate;
        const double after=time-profile.preDelaySeconds;
        if(after<=0.0)continue; // no hard early reflection or dry impulse spike
        // Dense multi-band filtered stochastic field -> no audible discrete echo taps.
        const float white=float(double(rng())/2147483647.0*2.0-1.0);
        bass+=bassCoeff*(white-bass);
        body+=bodyCoeff*(white-body);
        air+=airCoeff*(white-air);
        float signal=profile.lowWeight*bass+profile.bodyWeight*body+
                     profile.airWeight*air;
        for(auto& diffuser:diffusors)signal=diffuser.tick(signal);
        dc+=dcCoeff*(signal-dc);
        signal-=dc*.92f;
        // Predelay is followed by a very soft, slowly blooming attack.
        const double u=after/profile.bloomSeconds;
        const double rise=1.0-std::exp(-std::pow(u,1.55));
        const double decay=std::exp(-6.90775527898*after/profile.rt60Seconds);
        const double remain=profile.durationSeconds-time;
        const double end=std::clamp(remain/3.0,0.0,1.0);
        const double taper=end*end*(3.0-2.0*end);
        const double motion=0.90+0.10*std::sin(twoPi*0.077*time+
                          double(mode)*0.8+double(channel)*1.73);
        out[i]=float(rise*decay*taper*motion)*signal;
    }
    return out;
}
} // namespace aeryther
