#pragma once
#include <JuceHeader.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

// Original eight-line modulated FDN reverb for the Aeryther beta.
// Serial input diffusion, fractional delay taps and per-line high-frequency
// damping produce a wider, slowly moving tail without adding any UI controls.
class AerytherSpaciousReverb {
    struct Line {
        std::vector<float> samples;
        int write = 0;
        int delay = 1;
        float damped = 0.f;
        float depth = 0.f;
        double phase = 0.0, increment = 0.0;
    };
    struct Allpass {
        std::vector<float> samples;
        int write = 0;
        float tick(float input) noexcept {
            const float delayed = samples[size_t(write)];
            const float x = input - .68f * delayed;
            samples[size_t(write)] = x;
            if (++write >= int(samples.size())) write = 0;
            return delayed + .68f * x;
        }
    };
    std::array<Line,8> lines;
    std::array<Allpass,6> diffusion;
    double sr = 44100.0;
public:
    void prepare(double sampleRate) {
        sr = sampleRate;
        constexpr double seconds[8] = {.0413,.0479,.0537,.0611,.0697,.0793,.0917,.1051};
        constexpr double modulation[8] = {.103,.137,.081,.159,.112,.073,.127,.093};
        constexpr double diffuserLength[6] = {.0091,.0127,.0181,.0107,.0157,.0211};
        for(int k=0;k<8;++k) {
            auto& l=lines[size_t(k)];
            l.delay=juce::jmax(4,int(seconds[k]*sr));
            l.depth=float(sr*(.00075 + .00014*(k%4)));
            l.samples.assign(size_t(l.delay + int(2*l.depth)+8),0.f);
            l.write=0;l.damped=0;
            l.phase=(2.0*juce::MathConstants<double>::pi*k)/8.0;
            l.increment=2.0*juce::MathConstants<double>::pi*modulation[k]/sr;
        }
        for(int k=0;k<6;++k){
            auto& d=diffusion[size_t(k)];
            d.samples.assign(size_t(juce::jmax(2,int(sr*diffuserLength[k]))),0.f);
            d.write=0;
        }
    }
    void process(juce::AudioBuffer<float>& audio,float decay) noexcept {
        const float rt60=juce::jlimit(.5f,60.f,decay);
        const float lowPass = 1.f-std::exp(-2.f*juce::MathConstants<float>::pi*6800.f/float(sr));
        for(int i=0;i<audio.getNumSamples();++i) {
            float l=audio.getSample(0,i),r=audio.getSample(1,i);
            for(int k=0;k<3;++k) {
                l=diffusion[size_t(k)].tick(l);
                r=diffusion[size_t(k+3)].tick(r);
            }
            float y[8]{};
            for(int k=0;k<8;++k) {
                auto& d=lines[size_t(k)];
                const int size=int(d.samples.size());
                const float variable=float(d.delay)+d.depth*std::sin(d.phase);
                float read=float(d.write)-variable;
                if(read<0.f) read+=float(size);
                const int p0=int(read)%size,p1=(p0+1)%size;
                const float alpha=read-std::floor(read);
                y[k]=d.samples[size_t(p0)]*(1.f-alpha)+d.samples[size_t(p1)]*alpha;
                d.phase+=d.increment;
                if(d.phase>=2.0*juce::MathConstants<double>::pi)
                    d.phase-=2.0*juce::MathConstants<double>::pi;
            }
            // Orthonormal Hadamard feedback matrix.
            float feedback[8]{};
            for(int k=0;k<8;++k) {
                float sum=0.f;
                for(int j=0;j<8;++j){
                    unsigned bits=unsigned(k&j),odd=0u;
                    while(bits){odd^=(bits&1u);bits>>=1u;}
                    sum+=(odd?-y[j]:y[j]);
                }
                feedback[k]=sum*.353553390593f;
            }
            for(int k=0;k<8;++k) {
                auto& d=lines[size_t(k)];
                d.damped+=lowPass*(feedback[k]-d.damped);
                const float g=std::exp(-6.9077553f*(float(d.delay)/float(sr))/rt60);
                d.samples[size_t(d.write)]=((k&1)?r:l)*.21f+std::tanh(g*d.damped);
                if(++d.write>=int(d.samples.size()))d.write=0;
            }
            audio.setSample(0,i,(y[0]+y[2]-y[4]+y[6]-y[1]-y[7])*.23f);
            audio.setSample(1,i,(y[1]+y[3]-y[5]+y[7]-y[0]-y[6])*.23f);
        }
    }
};
