#pragma once
#include <JuceHeader.h>
#include <array>
#include <cmath>

// Lightweight stereo three-band EQ for the sound entering/leaving Convolver.
// Transposed Direct Form II biquads; no allocations inside audio processing.
class AerytherConvolverEQ {
    struct Biquad {
        double b0=1,b1=0,b2=0,a1=0,a2=0;
        std::array<double,2> z1{},z2{};
        void set(double nb0,double nb1,double nb2,double na0,double na1,double na2) noexcept {
            b0=nb0/na0;b1=nb1/na0;b2=nb2/na0;a1=na1/na0;a2=na2/na0;
        }
        void reset() noexcept {z1.fill(0);z2.fill(0);}
        float process(float input,int channel) noexcept {
            const double x=input;
            const double y=b0*x+z1[size_t(channel)];
            z1[size_t(channel)]=b1*x-a1*y+z2[size_t(channel)];
            z2[size_t(channel)]=b2*x-a2*y;
            return float(y);
        }
    };
    std::array<Biquad,3> bands;
    double sampleRate=44100.0;
    static constexpr double pi=3.14159265358979323846;
    static constexpr double sqrt2=1.4142135623730950488;
    static void shelf(Biquad& b,double sr,double freq,double db,bool high) noexcept {
        const double A=std::pow(10.0,db/40.0);
        const double w=2*pi*juce::jmin(freq,sr*.42)/sr;
        const double cs=std::cos(w),sn=std::sin(w);
        const double alpha=sn/sqrt2; // shelf slope ~= 1
        const double beta=2*std::sqrt(A)*alpha;
        const double ap=A+1,am=A-1;
        if(high) {
            b.set(A*(ap+am*cs+beta),-2*A*(am+ap*cs),A*(ap+am*cs-beta),
                  ap-am*cs+beta,2*(am-ap*cs),ap-am*cs-beta);
        } else {
            b.set(A*(ap-am*cs+beta),2*A*(am-ap*cs),A*(ap-am*cs-beta),
                  ap+am*cs+beta,-2*(am+ap*cs),ap+am*cs-beta);
        }
    }
    static void peaking(Biquad& b,double sr,double freq,double db) noexcept {
        const double A=std::pow(10.0,db/40.0);
        const double w=2*pi*juce::jmin(freq,sr*.42)/sr;
        const double alpha=std::sin(w)/(2*0.8);
        const double cs=std::cos(w);
        b.set(1+alpha*A,-2*cs,1-alpha*A,1+alpha/A,-2*cs,1-alpha/A);
    }
public:
    void prepare(double sr) noexcept {
        sampleRate=juce::jmax(8000.0,sr);
        for(auto& b:bands)b.reset();
        configure(0,0,0);
    }
    void configure(float lows,float mids,float highs) noexcept {
        // Three broad, audible bands rather than narrow corrections:
        // 220Hz low shelf, 1.4kHz mid presence, 4.5kHz high shelf.
        shelf(bands[0],sampleRate,220.,lows,false);
        peaking(bands[1],sampleRate,1400.,mids);
        shelf(bands[2],sampleRate,4500.,highs,true);
    }
    void process(juce::AudioBuffer<float>& audio) noexcept {
        for(int ch=0;ch<juce::jmin(2,audio.getNumChannels());++ch) {
            auto* dst=audio.getWritePointer(ch);
            for(int i=0;i<audio.getNumSamples();++i) {
                float v=dst[i];
                for(auto& f:bands)v=f.process(v,ch);
                dst[i]=v;
            }
        }
    }
};
