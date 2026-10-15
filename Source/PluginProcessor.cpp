#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "HugeHallImpulse.h"
#include <cmath>
#include <random>

namespace {
using AP = juce::AudioParameterFloat;
using BP = juce::AudioParameterBool;
using CP = juce::AudioParameterChoice;
constexpr float pi = juce::MathConstants<float>::pi;
float clampf(float v, float lo, float hi) { return juce::jlimit(lo,hi,v); }
float midiHz(double n) { return float(440.0 * std::pow(2.0,(n - 69.0) / 12.0)); }
float readInterpolated(const juce::AudioBuffer<float>& b, int ch, double pos) {
    if (b.getNumSamples() < 2) return 0.0f;
    const int n=b.getNumSamples();
    pos=juce::jlimit(0.0,double(n-1),pos);
    const int a=juce::jlimit(0,n-1,int(pos));
    const int next=juce::jmin(a+1,n-1);
    return juce::jmap(float(pos-a),b.getSample(juce::jmin(ch,b.getNumChannels()-1),a),
                     b.getSample(juce::jmin(ch,b.getNumChannels()-1),next));
}
double polyBlep(double phase, double step) {
    if (step <= 0.0) return 0.0;
    if (phase < step) { const double t=phase/step; return t+t-t*t-1.0; }
    if (phase > 1.0-step) { const double t=(phase-1.0)/step; return t*t+t+t+1.0; }
    return 0.0;
}
double oscWave(int shape, double phase, double step) {
    const double sine=std::sin(phase*2.0*juce::MathConstants<double>::pi);
    const double saw=2.0*phase-1.0-polyBlep(phase,step);
    const double tri=4.0*std::abs(phase-0.5)-1.0;
    const double square=(phase<0.5?1.0:-1.0)+polyBlep(phase,step)
                        -polyBlep(std::fmod(phase+0.5,1.0),step);
    switch(shape) {
       case 0: return sine;
       case 1: return tri;
       case 2: return saw;
       case 3: return square;
       case 4: return (phase<0.25?1.0:-1.0)+polyBlep(phase,step)
                      -polyBlep(std::fmod(phase+0.75,1.0),step);
       case 5: return std::tanh(1.4*saw)/std::tanh(1.4);
       case 6: {
          double p2=std::fmod(phase+0.26,1.0);
          return 0.5*(saw+2.0*p2-1.0-polyBlep(p2,step));
       }
       case 7: return 0.65*sine+0.35*saw;
       case 8: return 0.75*sine+0.20*std::sin(6.0*juce::MathConstants<double>::pi*phase)
                  +0.08*std::sin(10.0*juce::MathConstants<double>::pi*phase);
       default: return sine;
    }
}
float routedParam(juce::AudioProcessorValueTreeState& s, juce::String id) {
    if (auto* p=s.getRawParameterValue(id)) return p->load(); return 0.0f;
}
}

juce::AudioProcessorValueTreeState::ParameterLayout AerytherAudioProcessor::makeLayout() {
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    auto f=[&](const char* id,const char* name,float lo,float hi,float def,float step=0.001f){
        p.push_back(std::make_unique<AP>(juce::ParameterID{id,1},name,
            juce::NormalisableRange<float>{lo,hi,step},def));};
    auto b=[&](const char* id,const char* name,bool def){p.push_back(std::make_unique<BP>(juce::ParameterID{id,1},name,def));};
    auto c=[&](const char* id,const char* name,juce::StringArray values,int def){p.push_back(std::make_unique<CP>(juce::ParameterID{id,1},name,values,def));};
    b("loopPlay","Loop Play",false); f("sampleBpm","Sample BPM",30,300,120,1);
    c("direction","Direction",{"Forward","Reverse","Ping-Pong"},0);
    f("loopStart","Loop Start",0,0.999,0,0.001); f("loopEnd","Loop End",0.001,1,1,0.001);
    f("root","Root Note",0,127,60,1); f("sampleFine","Sample Fine",-100,100,0,1);
    f("stretch","Stretch",0.25,4,1); f("crossfade","Crossfade",0,0.5,0.08);
    f("sampleLevel","Sample Level",-60,6,0,0.1);
    c("waveShape","Wave",{"Sine","Triangle","Saw","Square","Pulse 25%","Soft Saw","Double Saw","Sine + Saw","Digital"},2);
    f("unison","Unison",1,8,1,1); f("detune","Detune",0,100,0); f("width","Width",0,1,0);
    f("waveOctave","Wave Octave",-3,3,0,1); f("waveFine","Wave Fine",-100,100,0,1);
    f("waveGlide","Portamento",0,3,0,0.001f); f("waveLevel","Wave Level",-60,6,0,0.1);
    c("subShape","Sub Wave",{"Sine","Triangle"},0);
    f("subOctave","Sub Octave",-3,0,-1,1); f("subTune","Sub Tune",-100,100,0,1);
    f("subLevel","Sub Level",-60,6,0,0.1);
    for (auto prefix : {"sample","wave","sub"}) {
        juce::String pr(prefix);
        b((pr+"Filter").toRawUTF8(),(pr+" Filter").toRawUTF8(),false);
        b((pr+"Delay").toRawUTF8(),(pr+" Delay").toRawUTF8(),false);
        b((pr+"Conv").toRawUTF8(),(pr+" Convolver").toRawUTF8(),pr!="sub");
        b((pr+"Direct").toRawUTF8(),(pr+" Direct").toRawUTF8(),pr=="sub");
    }
    for (auto prefix : {"amp","fenv"}) {
        juce::String pr(prefix);
        f((pr+"Attack").toRawUTF8(),(pr+" Attack").toRawUTF8(),0,15,0.0f);
        f((pr+"Decay").toRawUTF8(),(pr+" Decay").toRawUTF8(),0.01f,20,0.01f);
        f((pr+"Sustain").toRawUTF8(),(pr+" Sustain").toRawUTF8(),0,1,1);
        f((pr+"Release").toRawUTF8(),(pr+" Release").toRawUTF8(),0.01f,30,0.15f);
    }
    for(int i=1;i<=2;++i) {
        auto pre="lfo"+juce::String(i);
        f((pre+"Rate").toRawUTF8(),(pre+" Rate").toRawUTF8(),0.002f,12,0.1f);
        f((pre+"Depth").toRawUTF8(),(pre+" Depth").toRawUTF8(),0,1,0);
        c((pre+"Shape").toRawUTF8(),(pre+" Shape").toRawUTF8(),{"Sine","Triangle","Saw","Square","Random"},0);
        b((pre+"Sync").toRawUTF8(),(pre+" Tempo Sync").toRawUTF8(),false);
        c((pre+"Division").toRawUTF8(),(pre+" Division").toRawUTF8(),
          {"4 Bars","2 Bars","1 Bar","1/2","1/4","1/8","1/16","1/32","1/64"},4);
    }
    c("filterType","Filter Type",{"Low Pass","High Pass","Band Pass","Notch","Comb"},0);
    f("cutoff","Cutoff",30,18000,1200); f("resonance","Resonance",0.05f,0.95f,0.15f);
    f("drive","Drive",0,1,0); f("filterMix","Filter Mix",0,1,1);
    f("delayTime","Delay Time",0.04f,1.8f,0.35f);
    f("delayFeedback","Delay Feedback",0,0.94f,0.35f);
    b("pingPong","Ping Pong",true); f("delayMix","Delay Mix",0,1,0.4f);
    b("delaySync","Tempo Sync",false);
    c("delayDivision","Delay Note",{"1/1","1/2","1/4","1/8","1/16","1/32",
                                       "1/4 D","1/8 D","1/16 D","1/4 T","1/8 T","1/16 T"},2);
    f("delayShimmer","Octave Shimmer",0,1,0,0.001f);
    c("shimmerMaxOctaves","Shimmer Ceiling",{"MAX +2 OCT","MAX +3 OCT"},0);
    c("convMode","Convolver",{"AUUU","CROSS","CATHOLIC","DARK"},0);
    b("convReverse","Reverse Impulse",true);
    f("convMix","Convolver Mix",0,1,1);
    f("reverbDecay","Reverb Decay",0.5f,60,60);
    f("reverbTone","Reverb Tone",0,1,0.72f,0.001f);
    f("reverbMix","Reverb Mix",0,1,1);
    b("eqEnabled","EQ Enabled",true);
    // POST by default provides immediate audible feedback when adjusting EQ.
    // PRE still shapes the impulse excitation for evolving reverb colours.
    c("eqPosition","EQ Position",{"PRE CONV","POST CONV"},1);
    f("eqLow","EQ Low dB",-18,18,0,0.1f);
    f("eqMid","EQ Mid dB",-18,18,0,0.1f);
    f("eqHigh","EQ High dB",-18,18,0,0.1f);
    const juce::StringArray sources{"Off","LFO 1","LFO 2","AMP ENV","FILTER ENV"};
    const juce::StringArray targets{
        "Off","Filter Cutoff","Filter Resonance","Filter Drive","Filter Mix",
        "Wave Detune","Wave Width","Wave Pitch","Wave Level",
        "Sub Pitch","Sub Level","Sample Pitch","Sample Stretch","Sample Level",
        "Delay Time","Delay Feedback","Delay Mix",
        "Convolver Mix","Reverb Decay","Reverb Mix","Reverb Tone",
        "EQ Low","EQ Mid","EQ High","LFO 1 Rate","LFO 2 Rate",
        "Wave Octave"
    };
    for(int i=1;i<=8;++i) {
        auto prefix="mod"+juce::String(i);
        c((prefix+"Source").toRawUTF8(),(prefix+" Source").toRawUTF8(),sources,i==1?1:i==2?2:0);
        c((prefix+"Target").toRawUTF8(),(prefix+" Target").toRawUTF8(),targets,0);
        f((prefix+"Amount").toRawUTF8(),(prefix+" Amount").toRawUTF8(),-1,1,0);
    }
    // Append for backwards compatibility: existing sampleFine (+/- 100 cents)
    // remains readable in old project states, but the UI exposes semitones.
    f("sampleTranspose","Sample Pitch (Semitones)",-24.f,24.f,0.f,1.f);
    return {p.begin(),p.end()};
}

AerytherAudioProcessor::AerytherAudioProcessor()
: juce::AudioProcessor(BusesProperties()
#if defined(AERYTHER_FX)
  .withInput("Input",juce::AudioChannelSet::stereo(),true)
#endif
  .withOutput("Output",juce::AudioChannelSet::stereo(),true)),
  state(*this,nullptr,"AerytherState",makeLayout()) {}

bool AerytherAudioProcessor::isBusesLayoutSupported(const BusesLayout& l) const {
#if defined(AERYTHER_FX)
    return l.getMainInputChannelSet()==juce::AudioChannelSet::stereo() &&
           l.getMainOutputChannelSet()==juce::AudioChannelSet::stereo();
#else
    return l.getMainOutputChannelSet()==juce::AudioChannelSet::stereo();
#endif
}

float AerytherAudioProcessor::get(const char* id) const {
    if (auto* v=state.getRawParameterValue(id))return v->load();return 0;
}

void AerytherAudioProcessor::DelayLine::prepare(int maximumSamples,double sr) {
    for(auto& stage:octaveRings)
        for(auto& channel:stage)channel.assign(size_t(maximumSamples),0.f);
    const int pitchCount=juce::jmax(256,int(sr*0.21));
    for(auto& stage:grainHistory)
        for(auto& channel:stage)channel.assign(size_t(pitchCount),0.f);
    cursor=0;grainCursor=0;grainPhase=0.0;shimmerSmoothed=0.f;
    timeCrossfade.reset();
}
void AerytherAudioProcessor::DelayLine::process(juce::AudioBuffer<float>& b,
      float seconds,float feedback,double sr,bool pingPong,float shimmer,int maxOctaves) {
    if(octaveRings[0][0].empty())return;
    const int n=int(octaveRings[0][0].size());
    const int delay=juce::jlimit(1,n-1,int(seconds*sr));
    // TIME is block-rate; the two delay taps persist between audio blocks.
    // 75ms fades are gentle enough for live sweeps without pitching the echoes.
    timeCrossfade.request(delay,juce::jmax(2,int(sr*0.075)));
    const int pn=int(grainHistory[0][0].size());
    const int grain=juce::jlimit(32,pn/2,int(sr*0.08));
    const double phaseStep=1.0/double(grain);
    const float slew=1.f-std::exp(-1.f/float(sr*0.025));
    const float amount=juce::jlimit(0.f,1.f,shimmer);
    const int top=aeryther::shimmerOctaveCeiling(maxOctaves);
    auto pitchRead=[&](int stage,int channel,double position)->float {
        while(position<0.)position+=pn;
        while(position>=pn)position-=pn;
        const int i0=int(position),i1=(i0+1)%pn;
        const float mix=float(position-double(i0));
        const auto& buf=grainHistory[size_t(stage)][size_t(channel)];
        return buf[size_t(i0)]*(1.f-mix)+buf[size_t(i1)]*mix;
    };
    for(int i=0;i<b.getNumSamples();++i) {
        const int readFrom=(cursor-timeCrossfade.fromSamples+n)%n;
        const bool blending=timeCrossfade.isCrossfading();
        const int readTo=blending?(cursor-timeCrossfade.toSamples+n)%n:readFrom;
        const float weight=timeCrossfade.blend();
        shimmerSmoothed+=(amount-shimmerSmoothed)*slew;
        const float shimmerLevel=shimmerSmoothed;
        const float normalFeedback=feedback*(1.f-0.55f*shimmerLevel);
        std::array<std::array<float,2>,4> taps{};
        std::array<std::array<float,2>,3> raised{};
        for(int stage=0;stage<=top;++stage)
            for(int ch=0;ch<2;++ch)
                taps[size_t(stage)][size_t(ch)]=
                    octaveRings[size_t(stage)][size_t(ch)][size_t(readFrom)]*(1.f-weight)
                  + octaveRings[size_t(stage)][size_t(ch)][size_t(readTo)]*weight;

        // Each pitch stage reads ONLY the octave beneath it. Unlike feeding
        // the complete wet sum back into the shifter, already-raised echoes
        // never climb above the selected two/three-octave ceiling.
        if(shimmerLevel>0.00001f) {
            const float p1=float(grainPhase);
            const float p2=std::fmod(p1+.5f,1.f);
            const float w1=aeryther::shimmerGrainWindow(p1);
            const float w2=aeryther::shimmerGrainWindow(p2);
            const double cursorPos=double(grainCursor);
            const double offset1=double(grain)*(1.0-double(p1));
            const double offset2=double(grain)*(1.0-double(p2));
            for(int stage=0;stage<top;++stage) {
                for(int ch=0;ch<2;++ch) {
                    grainHistory[size_t(stage)][size_t(ch)][size_t(grainCursor)]
                        =taps[size_t(stage)][size_t(ch)];
                    raised[size_t(stage)][size_t(ch)]=
                          w1*pitchRead(stage,ch,cursorPos-offset1)
                         +w2*pitchRead(stage,ch,cursorPos-offset2);
                }
            }
            grainPhase+=phaseStep;
            if(grainPhase>=1.0)grainPhase-=1.0;
        } else {
            grainPhase=0.0;
            for(int stage=0;stage<top;++stage)
                for(int ch=0;ch<2;++ch)
                    grainHistory[size_t(stage)][size_t(ch)][size_t(grainCursor)]
                        =taps[size_t(stage)][size_t(ch)];
        }

        const std::array<float,2> input{
            b.getSample(0,i),b.getSample(1,i)};
        std::array<float,2> wet{};
        for(int stage=0;stage<=top;++stage) {
            for(int ch=0;ch<2;++ch) {
                wet[size_t(ch)]+=taps[size_t(stage)][size_t(ch)];
                const int other=pingPong?1-ch:ch;
                const float again=taps[size_t(stage)][size_t(other)];
                // Each octave maintains its own repeats. Stage 0 remains
                // identical to the old delay when SHIMMER=0.
                float value=stage==0?input[size_t(ch)]:0.f;
                value+=again*normalFeedback;
                if(stage>0)
                    value+=raised[size_t(stage-1)][size_t(other)]
                           *feedback*shimmerLevel*.68f;
                octaveRings[size_t(stage)][size_t(ch)][size_t(cursor)]=value;
            }
        }
        b.setSample(0,i,wet[0]);
        b.setSample(1,i,wet[1]);
        timeCrossfade.advance();
        if(++cursor>=n)cursor=0;
        if(++grainCursor>=pn)grainCursor=0;
    }
}

void AerytherAudioProcessor::LongReverb::prepare(double sr) {
    sampleRate=sr;
    // Four mutually-incommensurate delay lines, mixed by an orthonormal
    // 4x4 Hadamard feedback matrix. Decay is interpreted as time to -60 dB.
    const double delays[]={0.0481,0.0563,0.0677,0.0811};
    for(int k=0;k<4;++k){
        auto& line=lines[size_t(k)];
        line.samples.assign(size_t(juce::jmax(128,int(delays[k]*sr))),0.f);
        line.head=0;line.damped=0;
    }
}
void AerytherAudioProcessor::LongReverb::process(juce::AudioBuffer<float>& audio,float decaySeconds) {
    decaySeconds=juce::jlimit(0.5f,60.f,decaySeconds);
    for(int i=0;i<audio.getNumSamples();++i){
        const float l=audio.getSample(0,i),r=audio.getSample(1,i);
        float y[4];
        for(int k=0;k<4;++k){
            auto& d=lines[size_t(k)];y[k]=d.samples[size_t(d.head)];
        }
        const float fb[4]={
            (y[0]+y[1]+y[2]+y[3])*0.5f,
            (y[0]-y[1]+y[2]-y[3])*0.5f,
            (y[0]+y[1]-y[2]-y[3])*0.5f,
            (y[0]-y[1]-y[2]+y[3])*0.5f
        };
        const float injection[4]={l*0.34f,r*0.34f,(l+r)*0.23f,(l-r)*0.23f};
        for(int k=0;k<4;++k){
            auto& d=lines[size_t(k)];
            float lengthSeconds=float(double(d.samples.size())/sampleRate);
            float decayGain=std::exp(-6.9077553f*lengthSeconds/decaySeconds);
            // Slight high-frequency attenuation inside the reverb network.
            d.damped+=0.65f*(fb[k]-d.damped);
            d.samples[size_t(d.head)]=injection[k]+d.damped*decayGain;
            if(++d.head==int(d.samples.size()))d.head=0;
        }
        audio.setSample(0,i,(y[0]+y[2]-y[1]-y[3])*0.45f);
        audio.setSample(1,i,(y[0]-y[2]+y[1]-y[3])*0.45f);
    }
}
void AerytherAudioProcessor::LayerFilter::prepare(double sr,int) {
    for(auto& f:svf) { f.reset();f.prepare({sr,512,1}); }
    for(auto& v:comb)v.assign(size_t(sr/20.0+16.0),0.f);
    cursor={0,0};
}
void AerytherAudioProcessor::LayerFilter::process(juce::AudioBuffer<float>& b,int kind,float cutoff,
                                                    float resonance,float drive,float mix,double sr) {
    cutoff=clampf(cutoff,30,float(sr*0.43));
    for(auto& f:svf) {
        f.setType(kind==1 ? juce::dsp::StateVariableTPTFilterType::highpass :
                  (kind==2 || kind==3) ? juce::dsp::StateVariableTPTFilterType::bandpass :
                            juce::dsp::StateVariableTPTFilterType::lowpass);
        f.setCutoffFrequency(cutoff); f.setResonance(0.65f+resonance*9.0f);
    }
    auto gain=1.0f+drive*8.0f;
    for(int ch=0;ch<2;++ch) {
        auto* out=b.getWritePointer(ch);
        for(int i=0;i<b.getNumSamples();++i) {
            auto dry=out[i];auto s=std::tanh(dry*gain)/std::tanh(gain);
            float wet=0;
            if(kind==4) {
                auto& buf=comb[size_t(ch)]; int len=int(buf.size());
                auto distance=juce::jlimit(1,len-1,int(sr/cutoff));
                auto read=(cursor[size_t(ch)]-distance+len)%len;
                wet=buf[size_t(read)];
                buf[size_t(cursor[size_t(ch)])]=s+wet*resonance*0.9f;
                cursor[size_t(ch)]=(cursor[size_t(ch)]+1)%len;
            } else if(kind==3) {
                auto low=svf[size_t(ch)].processSample(0,s);
                wet=s-low;
            } else wet=svf[size_t(ch)].processSample(0,s);
            out[i]=dry*(1-mix)+wet*mix;
        }
    }
}

void AerytherAudioProcessor::makeImpulseResponses(double sr,int maxBlock) {
    // Colossal ambient halls: each profile is available in NORMAL and
    // fully REVERSE convolution, not reverse sample playback.
    // Use a 32kHz IR, resampled internally by the JUCE convolution engine.
    const double impulseRate=juce::jmin(sr,32000.0);
    for(int mode=0;mode<4;++mode) {
        const auto profile=aeryther::hugeHallProfiles[size_t(mode)];
        const int count=int(std::ceil(profile.durationSeconds*impulseRate));
        std::array<std::vector<float>,2> channels;
        for(int ch=0;ch<2;++ch)
            channels[size_t(ch)]=aeryther::generateHugeHallImpulse(mode,ch,impulseRate);
        for(int reversed=0;reversed<2;++reversed) {
            // Both variants are prepared and loaded ahead of playback;
            // switching reverse never generates impulses on the audio thread.
            auto& conv=convolutions[size_t(mode+4*reversed)];
            conv.prepare({sr,juce::uint32(maxBlock),2});
            juce::AudioBuffer<float> ir(2,count);
            for(int ch=0;ch<2;++ch) {
                const auto& original=channels[size_t(ch)];
                auto* dst=ir.getWritePointer(ch);
                if(reversed)
                    for(int i=0;i<count;++i)dst[i]=original[size_t(count-1-i)];
                else
                    std::copy(original.begin(),original.end(),dst);
            }
            conv.loadImpulseResponse(std::move(ir),impulseRate,
                juce::dsp::Convolution::Stereo::yes,
                juce::dsp::Convolution::Trim::no,
                juce::dsp::Convolution::Normalise::yes);
        }
    }
}

void AerytherAudioProcessor::prepareToPlay(double sr,int maximumBlock) {
    currentSampleRate=sr;
    subEnvelopeLevel=0.f;
    subCurrentNote=-1.0;
    subPhases.fill(0.0);
    heldNotes.fill(false);
    lastWaveNote=-1.0;
    blockCapacity=juce::jmax(64,maximumBlock);
    sampleControls.resize(size_t(blockCapacity));
    smoothStretch.reset(sr,0.085);
    smoothPitch.reset(sr,0.065);
    smoothCrossfade.reset(sr,0.045);
    smoothReverbMix.reset(sr,0.045);
    smoothStretch.setCurrentAndTargetValue(get("stretch"));
    smoothPitch.setCurrentAndTargetValue(get("sampleTranspose")+get("sampleFine")/100.f);
    smoothCrossfade.setCurrentAndTargetValue(get("crossfade"));
    smoothReverbMix.setCurrentAndTargetValue(get("reverbMix"));
    loopSamplerGrain.reset();
    activeSamplerData=nullptr;
    expectedSamplerFrame=-1;
    for(auto& voice:waveVoices)voice.envelope.setSampleRate(sr);
    for(auto& voice:sampleVoices)voice.envelope.setSampleRate(sr);
    // Keep enough delay history for a 1/1 note at very slow host tempos.
    for(auto& d:layerDelays)d.prepare(int(sr*8.0)+4,sr);
    for(auto& f:layerFilters)f.prepare(sr,maximumBlock);
    for(auto& b:layerAudio)b.setSize(2,blockCapacity);
    for(auto* b:{&convInput,&processed,&convWet,&wetScratch})b->setSize(2,blockCapacity);
    makeImpulseResponses(sr,blockCapacity);
    reverb.prepare(sr);
    convolverEQ.prepare(sr);
    filterEnvelope.setSampleRate(sr);
    amplitudeModEnvelope.setSampleRate(sr);
    transportFrameCounter=0;
    for(int k=0;k<2;++k){
        lfoPhases[k]=0.0;
        previousLfoValues[k]=0.f;
        lfoFreeCycles[k]=0;
        lfoSyncOffsets[k]=0.0;
        lfoPreviousPpq[k]=0.0;
        lfoSyncWasOn[k]=false;
    }
}
void AerytherAudioProcessor::releaseResources() {}

bool AerytherAudioProcessor::loadSample(const juce::File& file) {
    juce::AudioFormatManager formats;formats.registerBasicFormats();
    auto reader=std::unique_ptr<juce::AudioFormatReader>(formats.createReaderFor(file));
    if(!reader || reader->lengthInSamples<2 || reader->lengthInSamples>reader->sampleRate*120.0 ||
       reader->numChannels==0)return false;
    auto next=std::make_shared<SampleData>();
    next->rate=reader->sampleRate; next->path=file.getFullPathName();
    next->samples.setSize(juce::jmin(2,int(reader->numChannels)),int(reader->lengthInSamples));
    if(!reader->read(&next->samples,0,int(reader->lengthInSamples),0,true,true))return false;
    std::atomic_store(&sample,std::shared_ptr<const SampleData>(next));
    {juce::ScopedLock guard(fileNameMutex);loadedSamplePath=next->path;}
    return true;
}
juce::String AerytherAudioProcessor::sampleFileName() const {
    juce::ScopedLock guard(fileNameMutex);
    return juce::File(loadedSamplePath).getFileName();
}

void AerytherAudioProcessor::handleMidi(const juce::MidiMessage& m) {
    if(m.isAllNotesOff() || m.isAllSoundOff()) {
        heldNotes.fill(false);lastWaveNote=-1.0;filterEnvelope.reset();amplitudeModEnvelope.reset();
        for(auto& v:waveVoices)v.active=false;
        for(auto& v:sampleVoices)v.active=false;
        return;
    }
    if(!m.isNoteOnOrOff())return;
    int note=m.getNoteNumber();
    if(note<0 || note>127)return;
    if(m.isNoteOn()) {
        juce::ADSR::Parameters fe;
        fe.attack=get("fenvAttack");fe.decay=get("fenvDecay");
        fe.sustain=get("fenvSustain");fe.release=get("fenvRelease");
        filterEnvelope.setParameters(fe);filterEnvelope.noteOn();
        juce::ADSR::Parameters ae;
        ae.attack=get("ampAttack");ae.decay=get("ampDecay");
        ae.sustain=get("ampSustain");ae.release=get("ampRelease");
        amplitudeModEnvelope.setParameters(ae);amplitudeModEnvelope.noteOn();
        const bool legato=std::any_of(heldNotes.begin(),heldNotes.end(),[](bool held){return held;});
        heldNotes[size_t(note)]=true;
        auto makeVoice=[&](auto& pool){
            SynthVoice* v=nullptr;
            for(auto& item:pool)if(!item.active){v=&item;break;}
            if(!v)v=&pool[0];
            v->active=true;v->note=note;v->samplePosition=0;
            v->samplerGrain.reset();
            v->glidePitch=double(note);
            v->phases.fill(0);
            juce::ADSR::Parameters env;
            env.attack=get("ampAttack");env.decay=get("ampDecay");
            env.sustain=get("ampSustain");env.release=get("ampRelease");
            v->envelope.setParameters(env);v->envelope.reset();v->envelope.noteOn();
            return v;
        };
        auto* voice=makeVoice(waveVoices);
        voice->glidePitch=(legato && lastWaveNote>=0 && get("waveGlide")>0.0005f)
                          ?lastWaveNote:double(note);
        lastWaveNote=double(note);
        if(!enabled("loopPlay")) makeVoice(sampleVoices);
    } else if(m.isNoteOff()) {
        heldNotes[size_t(note)]=false;
        if(std::none_of(heldNotes.begin(),heldNotes.end(),[](bool on){return on;})) {
            filterEnvelope.noteOff();amplitudeModEnvelope.noteOff();
        }
        for(auto& v:waveVoices)if(v.active && v.note==note)v.envelope.noteOff();
        for(auto& v:sampleVoices)if(v.active && v.note==note)v.envelope.noteOff();
    }
}

float AerytherAudioProcessor::modulated(const char* destination,float original,float lfo1,float lfo2) const {
    // The matrix uses bipolar modulation with destination-specific musical ranges.
    struct Destination { const char* id; float depth; };
    static constexpr Destination destinations[]={
        {"off",0.f},{"cutoff",1.f},{"resonance",0.5f},{"drive",1.f},{"filterMix",1.f},
        {"detune",50.f},{"width",1.f},{"wavePitch",12.f},{"waveLevel",18.f},
        {"subPitch",12.f},{"subLevel",18.f},{"samplePitch",12.f},{"stretch",1.5f},
        {"sampleLevel",18.f},{"delayTime",0.75f},{"delayFeedback",0.5f},
        {"delayMix",1.f},{"convMix",1.f},{"reverbDecay",40.f},{"reverbMix",1.f},{"reverbTone",1.f},
        {"eqLow",18.f},{"eqMid",18.f},{"eqHigh",18.f},
        {"lfo1Rate",2.f},{"lfo2Rate",2.f},{"waveOctave",2.f}
    };
    for(int i=1;i<=8;++i) {
        const auto prefix="mod"+juce::String(i);
        const int target=int(routedParam(const_cast<juce::AudioProcessorValueTreeState&>(state),prefix+"Target"));
        const int source=int(routedParam(const_cast<juce::AudioProcessorValueTreeState&>(state),prefix+"Source"));
        if(target<=0 || target>=int(std::size(destinations)) || source<=0 ||
           juce::String(destination)!=destinations[target].id)continue;
        const float amount=routedParam(const_cast<juce::AudioProcessorValueTreeState&>(state),prefix+"Amount");
        const float signal=source==1?lfo1:source==2?lfo2:
                           source==3?currentAmpModEnv:currentFilterEnv;
        original+=amount*destinations[target].depth*signal;
    }
    return original;
}

void AerytherAudioProcessor::renderOscillators(juce::AudioBuffer<float>& w,juce::AudioBuffer<float>& sub,
                                                  int first,int length,float lfo1,float lfo2) {
    const int shape=juce::jlimit(0,8,int(get("waveShape")));
    const int unison=juce::jlimit(1,8,int(get("unison")));
    const float dt=clampf(modulated("detune",get("detune"),lfo1,lfo2),0,100);
    const float width=clampf(modulated("width",get("width"),lfo1,lfo2),0,1);
    const float level=clampf(modulated("waveLevel",get("waveLevel"),lfo1,lfo2),-60,6);
    const auto gain=juce::Decibels::decibelsToGain(level)*0.2f;
    const double glideSeconds=double(get("waveGlide"));
    const double glideStep=glideSeconds>0.0005 ?
        1.0-std::exp(-1.0/(currentSampleRate*glideSeconds)):1.0;
    // WAVE OCTAVE is an independent bipolar matrix target. At Amount=1
    // a full-depth LFO can move the octave knob by +/-2 octaves, without
    // changing the existing WAVE PITCH destination or the base knob value.
    const double octaveSemitones=aeryther::waveOctaveToSemitones(
        double(modulated("waveOctave",get("waveOctave"),lfo1,lfo2)));
    const double pitchOffset=double(get("waveFine"))/100.0+
        double(modulated("wavePitch",0.f,lfo1,lfo2));
    for(auto& v:waveVoices)if(v.active) {
        for(int i=first;i<first+length;++i) {
            float amp=v.envelope.getNextSample();
            if(!v.envelope.isActive()){v.active=false;break;}
            v.glidePitch += (double(v.note)-v.glidePitch)*glideStep;
            const double note=v.glidePitch+octaveSemitones+pitchOffset;
            float left=0,right=0;
            for(int u=0;u<unison;++u) {
                const double spread=unison>1?(double(u)/(unison-1)*2.0-1.0):0.0;
                const double freq=double(midiHz(note+spread*dt*0.0045));
                const double step=juce::jmin(0.49,freq/currentSampleRate);
                auto& phase=v.phases[size_t(u)]; phase+=step;
                if(phase>=1.0)phase-=std::floor(phase);
                const double value=oscWave(shape,phase,step);
                const double pan=spread*width;
                left+=float(value*(1.0-pan)*0.5);
                right+=float(value*(1.0+pan)*0.5);
            }
            w.addSample(0,i,left*amp*gain/unison);
            w.addSample(1,i,right*amp*gain/unison);
        }
    }
    int lowest=-1;
    for(int n=0;n<128;++n)if(heldNotes[size_t(n)]){lowest=n;break;}
    const float sg=juce::Decibels::decibelsToGain(
        clampf(modulated("subLevel",get("subLevel"),lfo1,lfo2),-60,6))*0.35f;
    // SUB used to stop at the first note-off sample and to start at full amplitude.
    // Both edges create audible discontinuities, especially in low-frequency bass.
    // Smooth amplitude per-sample (4 ms attack / 14 ms release), retaining the
    // oscillator phase while the last note releases. Also smooth note switches.
    const float attackStep=1.0f-std::exp(-1.0f/float(0.004*currentSampleRate));
    const float releaseStep=1.0f-std::exp(-1.0f/float(0.014*currentSampleRate));
    const double pitchStep=1.0-std::exp(-1.0/(0.016*currentSampleRate));
    const double transpose=get("subOctave")*12.0+get("subTune")/100.0+
                           modulated("subPitch",0.f,lfo1,lfo2);
    const int subShape=int(get("subShape"));
    for(int i=first;i<first+length;++i) {
        const float targetGain=lowest>=0?1.f:0.f;
        subEnvelopeLevel+=(targetGain-subEnvelopeLevel)*
                         (lowest>=0?attackStep:releaseStep);
        if(lowest>=0) {
            const double targetPitch=double(lowest)+transpose;
            if(subCurrentNote<0.0)subCurrentNote=targetPitch;
            else subCurrentNote+=(targetPitch-subCurrentNote)*pitchStep;
        }
        if(subCurrentNote<0.0 || (lowest<0 && subEnvelopeLevel<0.00001f)) {
            if(lowest<0 && subEnvelopeLevel<0.00001f)subEnvelopeLevel=0.f;
            continue;
        }
        const double step=double(midiHz(subCurrentNote))/currentSampleRate;
        subPhases[0]+=step;
        if(subPhases[0]>=1.0)subPhases[0]-=std::floor(subPhases[0]);
        const double phase=subPhases[0];
        const double value=subShape==0?std::sin(2.0*juce::MathConstants<double>::pi*phase)
                                      :4.0*std::abs(phase-0.5)-1.0;
        const float sample=float(value)*sg*subEnvelopeLevel;
        sub.addSample(0,i,sample);
        sub.addSample(1,i,sample);
    }
}

void AerytherAudioProcessor::renderSampler(juce::AudioBuffer<float>& out,int first,int length,
                                              bool looping,double ppq,double hostBpm,
                                              int64_t sampleTime,double srcBpm,float lfo1,float lfo2) {
    auto data=std::atomic_load(&sample);
    if(!data || data->samples.getNumSamples()<2)return;
    const int total=data->samples.getNumSamples();
    const int start=juce::jlimit(0,total-2,int(get("loopStart")*(total-1)));
    const int end=juce::jlimit(start+2,total,int(get("loopEnd")*total));
    const double loopSize=double(end-start);
    const float gain=juce::Decibels::decibelsToGain(
        clampf(modulated("sampleLevel",get("sampleLevel"),lfo1,lfo2),-60,6));
    const int direction=juce::jlimit(0,2,int(get("direction")));
    // 50ms fixed grain hop, 100ms fixed grain length. Neither knob changes
    // grain boundaries, avoiding the previous discontinuous grain rescheduling.
    const int hop=juce::jmax(64,int(currentSampleRate*.05));
    const double grainLength=2.0*double(hop);
    const double sourcePerOutput=data->rate/currentSampleRate;

    if(data.get()!=activeSamplerData) {
        activeSamplerData=data.get();
        loopSamplerGrain.reset();
        expectedSamplerFrame=-1;
    }

    auto readSource=[&](int channel,double unwrapped)->float {
        double position;
        if(direction==1) {
            position=double(end-1)-aeryther::wrapped(unwrapped,loopSize);
        } else if(direction==2) {
            const double x=aeryther::wrapped(unwrapped,loopSize*2.0);
            position=double(start)+(x<=loopSize?x:2.0*loopSize-x);
        } else {
            position=double(start)+aeryther::wrapped(unwrapped,loopSize);
        }
        return readInterpolated(data->samples,channel,
            juce::jlimit(double(start),double(end-1),position));
    };

    // One stereo grain pair is rendered and normalised per output frame.
    // Crossfade changes only the smoothly-ramped overlap envelope, not
    // the grain clock or the underlying read positions.
    auto readGrains=[&](const SamplerGrain& state,float fade)->std::array<float,2> {
        const double currentAge=double(state.clock);
        const double previousAge=currentAge+double(hop);
        const float wCurrent=state.current.active
            ? aeryther::samplerGrainWeight(currentAge,grainLength,fade):0.f;
        const float wPrevious=state.previous.active
            ? aeryther::samplerGrainWeight(previousAge,grainLength,fade):0.f;
        const float sum=wCurrent+wPrevious;
        if(sum<1.e-7f)return {0.f,0.f};
        std::array<float,2> result{};
        for(int channel=0;channel<2;++channel) {
            double value=0.0;
            if(wCurrent>0.f) {
                const auto& g=state.current;
                value+=double(wCurrent)*readSource(channel,
                    g.source+currentAge*sourcePerOutput*g.pitchRatio);
            }
            if(wPrevious>0.f) {
                const auto& g=state.previous;
                value+=double(wPrevious)*readSource(channel,
                    g.source+previousAge*sourcePerOutput*g.pitchRatio);
            }
            result[size_t(channel)]=float(value/double(sum));
        }
        return result;
    };
    auto beginGrain=[&](SamplerGrain& state,double semitones) {
        if(state.clock==0) {
            state.previous=state.current;
            state.current={state.sourcePosition,
                           aeryther::samplerPitchRatio(semitones),true};
        }
    };
    auto advanceGrain=[&](SamplerGrain& state,double sourceStep) {
        state.sourcePosition+=sourceStep;
        if(++state.clock>=hop)state.clock=0;
    };

    if(looping) {
        const int64_t firstFrame=sampleTime+int64_t(first);
        if(expectedSamplerFrame!=firstFrame) {
            // Realign only after a transport seek/start, never when turning
            // STRETCH. Subsequent rate changes integrate continuously.
            const double stretch=double(sampleControls[size_t(first)].stretch);
            const double sourceAtStart=ppq*60.0*data->rate/
                                       (juce::jmax(30.0,srcBpm)*stretch);
            loopSamplerGrain.reset(aeryther::wrapped(sourceAtStart,loopSize));
        }
        for(int i=first;i<first+length;++i) {
            const auto controls=sampleControls[size_t(i)];
            beginGrain(loopSamplerGrain,double(controls.pitch));
            const auto stereo=readGrains(loopSamplerGrain,controls.crossfade);
            out.addSample(0,i,stereo[0]*gain);
            out.addSample(1,i,stereo[1]*gain);
            const double step=sourcePerOutput*juce::jmax(30.0,hostBpm)/
                         (juce::jmax(30.0,srcBpm)*double(controls.stretch));
            advanceGrain(loopSamplerGrain,step);
            if(loopSamplerGrain.sourcePosition>=loopSize)
                loopSamplerGrain.sourcePosition=aeryther::wrapped(
                    loopSamplerGrain.sourcePosition,loopSize);
        }
        expectedSamplerFrame=firstFrame+length;
        playbackFraction.store(
            float(aeryther::wrapped(loopSamplerGrain.sourcePosition,loopSize)/loopSize),
            std::memory_order_relaxed);
        playbackActive.store(true,std::memory_order_relaxed);
    } else {
        bool active=false;
        for(auto& voice:sampleVoices)if(voice.active) {
            auto& grain=voice.samplerGrain;
            const double voiceOffset=double(voice.note)-double(get("root"));
            for(int i=first;i<first+length;++i) {
                const auto controls=sampleControls[size_t(i)];
                const float amp=voice.envelope.getNextSample();
                if(!voice.envelope.isActive() || grain.sourcePosition>=loopSize) {
                    voice.active=false;break;
                }
                beginGrain(grain,voiceOffset+double(controls.pitch));
                const auto stereo=readGrains(grain,controls.crossfade);
                // Fade out the final 5ms rather than abruptly killing a note.
                const double remaining=loopSize-grain.sourcePosition;
                const float ending=float(juce::jlimit(0.0,1.0,
                    remaining/(sourcePerOutput*currentSampleRate*0.005)));
                out.addSample(0,i,stereo[0]*amp*ending*gain);
                out.addSample(1,i,stereo[1]*amp*ending*gain);
                advanceGrain(grain,sourcePerOutput/double(controls.stretch));
                voice.samplePosition+=1.0;
            }
            if(voice.active){
                playbackFraction.store(float(aeryther::wrapped(
                    grain.sourcePosition,loopSize)/loopSize),std::memory_order_relaxed);
                active=true;
            }
        }
        playbackActive.store(active,std::memory_order_relaxed);
    }
}

void AerytherAudioProcessor::applyRouting(juce::AudioBuffer<float>& output,int samples,float lfo1,float lfo2,
                                          double hostBpm) {
    convInput.clear();processed.clear();convWet.clear();
    bool anyProcessedPath=false;
    for(int layer=0;layer<3;++layer) {
        const char* prefixes[]={"sample","wave","sub"};
        juce::String s(prefixes[layer]);
        auto& b=layerAudio[size_t(layer)];
        auto param=[&](const char* suffix){return routedParam(state,s+suffix)>0.5f;};
        const auto routes=aeryther::getPaths({
            param("Filter"),param("Delay"),param("Conv"),param("Direct")});
        // DIRECT taps the original layer in parallel; it never bypasses the
        // selected effects, and it never changes their inputs.
        if(routes.dry)
            for(int ch=0;ch<2;++ch)output.addFrom(ch,0,b,ch,0,samples);
        // No Direct, Filter, Delay or Convolution: completely silent layer.
        if(!routes.hasProcessedPath())continue;
        anyProcessedPath=true;
        if(routes.filtered){
            float originalCutoff=get("cutoff");
            float cutoff=clampf(originalCutoff*std::pow(2.0f,modulated("cutoff",0,lfo1,lfo2)*4.0f),30,18000);
            layerFilters[size_t(layer)].process(b,int(get("filterType")),cutoff,
                 clampf(modulated("resonance",get("resonance"),lfo1,lfo2),0.05f,0.95f),
                 clampf(modulated("drive",get("drive"),lfo1,lfo2),0.f,1.f),
                 clampf(modulated("filterMix",get("filterMix"),lfo1,lfo2),0.f,1.f),
                 currentSampleRate);
        }
        if(routes.delayed) {
            wetScratch.makeCopyOf(b,true);
            const bool synced=enabled("delaySync");
            const float seconds=synced
                ? float(aeryther::tempoDelaySeconds(hostBpm,
                      juce::jlimit(0,11,int(get("delayDivision")))))
                : clampf(modulated("delayTime",get("delayTime"),lfo1,lfo2),0.04f,1.8f);
            layerDelays[size_t(layer)].process(wetScratch,seconds,
                clampf(modulated("delayFeedback",get("delayFeedback"),lfo1,lfo2),0,0.94f),
                currentSampleRate,enabled("pingPong"),get("delayShimmer"),
                int(get("shimmerMaxOctaves"))+2);
            const float mix=clampf(modulated("delayMix",get("delayMix"),lfo1,lfo2),0.f,1.f);
            for(int ch=0;ch<2;++ch){
                b.applyGain(ch,0,samples,1.f-mix);
                b.addFrom(ch,0,wetScratch,ch,0,samples,mix);
            }
        }
        auto& dest=routes.convolved?convInput:processed;
        for(int ch=0;ch<2;++ch)dest.addFrom(ch,0,b,ch,0,samples);
    }
    if(!anyProcessedPath)return; // No effects enabled on any layer: do not emit lingering global FX.
    // EQ applies only to the convolution path: before the impulse response,
    // or to its wet result afterward. Dry/direct layers stay untouched.
    const bool eqOn=enabled("eqEnabled");
    const int eqPos=juce::jlimit(0,1,int(get("eqPosition")));
    if(eqOn){
        convolverEQ.configure(
            clampf(modulated("eqLow",get("eqLow"),lfo1,lfo2),-18.f,18.f),
            clampf(modulated("eqMid",get("eqMid"),lfo1,lfo2),-18.f,18.f),
            clampf(modulated("eqHigh",get("eqHigh"),lfo1,lfo2),-18.f,18.f));
    }
    auto mode=juce::jlimit(0,3,int(get("convMode")));
    convWet.makeCopyOf(convInput,true);
    // PRE shapes only the signal that enters convolution, not the bypass dry
    // copy. POST shapes the convolution result; both leave other layers alone.
    if(eqOn && eqPos==0)convolverEQ.process(convWet);
    juce::dsp::AudioBlock<float> block(convWet);
    juce::dsp::ProcessContextReplacing<float> context(block);
    convolutions[size_t(mode + (enabled("convReverse")?4:0))].process(context);
    float mix=clampf(modulated("convMix",get("convMix"),lfo1,lfo2),0,1);
    for(int ch=0;ch<2;++ch) {
        for(int i=0;i<samples;++i) {
            float dry=convInput.getSample(ch,i),wet=convWet.getSample(ch,i);
            processed.addSample(ch,i,aeryther::wetDry(dry,wet,mix));
        }
    }
    // POST EQ acts on the actual audible convolver output, including its
    // dry/wet blend. Unlike the prior post-only-wet path, dialing EQ gives
    // an immediate audible response even if a reverse impulse is still blooming.
    if(eqOn && eqPos==1)convolverEQ.process(processed);
    wetScratch.makeCopyOf(processed,true);
    reverb.process(wetScratch,
        clampf(modulated("reverbDecay",get("reverbDecay"),lfo1,lfo2),0.5f,60.f),
        clampf(modulated("reverbTone",get("reverbTone"),lfo1,lfo2),0.f,1.f));
    // Smooth MIX on individual audio samples to eliminate zipper noise when
    // sweeping a large diffuse convolution/reverb tail. Preserve the musical
    // constant-power gain law and previous wet makeup.
    smoothReverbMix.setTargetValue(
        clampf(modulated("reverbMix",get("reverbMix"),lfo1,lfo2),0.f,1.f));
    for(int i=0;i<samples;++i) {
        const auto gains=aeryther::reverbGains(smoothReverbMix.getNextValue());
        for(int ch=0;ch<2;++ch) {
            const float dry=processed.getSample(ch,i);
            const float wet=wetScratch.getSample(ch,i);
            output.addSample(ch,i,gains.dry*dry+gains.wet*wet);
        }
    }
}

void AerytherAudioProcessor::processBlock(juce::AudioBuffer<float>& output,juce::MidiBuffer& midi) {
    juce::ScopedNoDenormals denormals;
    int num=output.getNumSamples();
    if(num<1 || num>blockCapacity){output.clear();return;}
    for(auto& a:layerAudio){a.setSize(2,num,false,false,true);a.clear();}
#if defined(AERYTHER_FX)
    // FX edition: host stereo audio replaces the SAMPLE engine and travels
    // through exactly the same per-layer routing and shared effects.
    for(int ch=0;ch<2;++ch)layerAudio[0].copyFrom(ch,0,output,ch,0,num);
#endif
    output.clear();
    for(auto* a:{&convInput,&processed,&convWet,&wetScratch}){a->setSize(2,num,false,false,true);a->clear();}
    bool playing=false;double ppq=0,hostBpm=120;int64_t hostSamples=int64_t(transportFrameCounter);
    if(auto* ph=getPlayHead())if(auto pos=ph->getPosition()) {
        playing=pos->getIsPlaying();ppq=pos->getPpqPosition().orFallback(0.0);
        hostBpm=pos->getBpm().orFallback(120.0);
        hostSamples=pos->getTimeInSamples().orFallback(hostSamples);
    }
    const auto sampleMode = aeryther::samplePlaybackMode(enabled("loopPlay"), playing);
    if(sampleMode==aeryther::SamplePlaybackMode::Silent)
        playbackActive.store(false,std::memory_order_relaxed);
    bool loop=(sampleMode == aeryther::SamplePlaybackMode::HostLoop);
    // MIDI event sample offsets are honored; transport looping follows the host's absolute timeline.
    double lfo[2]{};
    for(int k=0;k<2;++k) {
        const char* rateId=k==0?"lfo1Rate":"lfo2Rate";
        const float rate=get(rateId);
        const int shape=juce::jlimit(0,4,int(get(k==0?"lfo1Shape":"lfo2Shape")));
        const bool sync=enabled(k==0?"lfo1Sync":"lfo2Sync");
        const int division=juce::jlimit(0,8,int(get(k==0?"lfo1Division":"lfo2Division")));
        static constexpr double beatsPerCycle[]{16.,8.,4.,2.,1.,0.5,0.25,0.125,0.0625};

        // Cross modulation is evaluated from the previous block to avoid
        // circular signal dependencies when both LFOs target each other's rate.
        // In SYNC mode, the division is the nominal speed, with modulation
        // continuously scaling that tempo-derived speed by 1/4..4x.
        const double octaveOffset=double(modulated(rateId,0.f,
                                      previousLfoValues[0],previousLfoValues[1]));
        const double multiplier=aeryther::lfoRateMultiplier(octaveOffset);
        double phase=0.0;
        int64_t cycle=0;
        if(sync) {
            const double baseCycles=ppq/beatsPerCycle[division];
            if(!lfoSyncWasOn[k] || !playing ||
               std::abs(ppq-lfoPreviousPpq[k])>beatsPerCycle[division]*2.0) {
                // Re-align with the grid after seeking, loop jumps and sync ON.
                lfoSyncOffsets[k]=0.0;
            } else {
                // Keep the host quarter-note grid at neutral Amount=0.
                // The accumulated offset is just the extra/subtracted
                // cycles caused by the LFO-to-LFO rate modulation.
                const double beatDelta=ppq-lfoPreviousPpq[k];
                lfoSyncOffsets[k]+=beatDelta/beatsPerCycle[division]*(multiplier-1.0);
            }
            const double totalCycles=baseCycles+lfoSyncOffsets[k];
            phase=aeryther::wrapped(totalCycles,1.0);
            cycle=static_cast<int64_t>(std::floor(totalCycles));
            lfoPreviousPpq[k]=ppq;
            lfoSyncWasOn[k]=true;
        } else {
            lfoSyncWasOn[k]=false;
            phase=lfoPhases[k];
            cycle=lfoFreeCycles[k];
            const double advance=double(rate)*multiplier*double(num)/currentSampleRate;
            const double ending=phase+advance;
            lfoFreeCycles[k]+=static_cast<int64_t>(std::floor(ending));
            lfoPhases[k]=aeryther::wrapped(ending,1.0);
        }
        const double shapeValue=shape==0?std::sin(2*pi*phase):
                                shape==1?4*std::abs(phase-0.5)-1:
                                shape==2?2*phase-1:
                                shape==3?(phase<0.5?1.0:-1.0):
                                double(aeryther::lfoRandomForCycle(cycle,k));
        lfo[k]=shapeValue*get(k==0?"lfo1Depth":"lfo2Depth");
    }
    previousLfoValues[0]=float(lfo[0]);
    previousLfoValues[1]=float(lfo[1]);
    // Generate parameter ramps once per block, not once per voice; all notes
    // share exactly the same smooth pitch/stretch/crossfade trajectory.
    smoothStretch.setTargetValue(clampf(modulated("stretch",get("stretch"),float(lfo[0]),float(lfo[1])),0.25f,4.f));
    smoothPitch.setTargetValue(clampf(get("sampleTranspose")+get("sampleFine")/100.f+
        modulated("samplePitch",0.f,float(lfo[0]),float(lfo[1])),-36.f,36.f));
    smoothCrossfade.setTargetValue(clampf(get("crossfade"),0.f,0.5f));
    for(int i=0;i<num;++i){
        auto& control=sampleControls[size_t(i)];
        control.stretch=smoothStretch.getNextValue();
        control.pitch=smoothPitch.getNextValue();
        control.crossfade=smoothCrossfade.getNextValue();
    }
#if !defined(AERYTHER_FX)
    int rendered=0;
    for(const auto metadata:midi) {
        int offset=juce::jlimit(0,num,metadata.samplePosition);
        if(offset>rendered) {
            if(sampleMode != aeryther::SamplePlaybackMode::Silent)
                renderSampler(layerAudio[0],rendered,offset-rendered,loop,ppq,hostBpm,hostSamples,get("sampleBpm"),float(lfo[0]),float(lfo[1]));
            renderOscillators(layerAudio[1],layerAudio[2],rendered,offset-rendered,float(lfo[0]),float(lfo[1]));
        }
        handleMidi(metadata.getMessage());rendered=offset;
    }
    if(rendered<num) {
        if(sampleMode != aeryther::SamplePlaybackMode::Silent)
            renderSampler(layerAudio[0],rendered,num-rendered,loop,ppq,hostBpm,hostSamples,get("sampleBpm"),float(lfo[0]),float(lfo[1]));
        renderOscillators(layerAudio[1],layerAudio[2],rendered,num-rendered,float(lfo[0]),float(lfo[1]));
    }
#endif
    currentFilterEnv=filterEnvelope.getNextSample();
    currentAmpModEnv=amplitudeModEnvelope.getNextSample();
    for(int i=1;i<num;++i) {filterEnvelope.getNextSample();amplitudeModEnvelope.getNextSample();}
    applyRouting(output,num,float(lfo[0]),float(lfo[1]),hostBpm);
    for(int ch=0;ch<output.getNumChannels();++ch)
        output.applyGain(ch,0,num,0.8f);
    transportFrameCounter+=uint64_t(num);
}

void AerytherAudioProcessor::getStateInformation(juce::MemoryBlock& dest) {
    auto xml=state.copyState();
    {juce::ScopedLock guard(fileNameMutex);xml.setProperty("samplePath",loadedSamplePath,nullptr);}
    if(auto doc=xml.createXml())copyXmlToBinary(*doc,dest);
}
void AerytherAudioProcessor::setStateInformation(const void* ptr,int n) {
    auto doc=getXmlFromBinary(ptr,n);
    if(doc && doc->hasTagName(state.state.getType().toString())) {
        auto restored=juce::ValueTree::fromXml(*doc);
        auto path=restored.getProperty("samplePath").toString();
        state.replaceState(restored);
        if(path.isNotEmpty())loadSample(juce::File(path));
    }
}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {return new AerytherAudioProcessor();}

std::vector<float> AerytherAudioProcessor::waveformPeaks(int columns) const {
    std::vector<float> values(size_t(juce::jmax(0,columns)),0.0f);
    auto d=std::atomic_load(&sample);
    if(!d || columns<1)return values;
    int n=d->samples.getNumSamples();
    for(int i=0;i<columns;++i){
        int from=int((int64_t(i)*n)/columns),to=int((int64_t(i+1)*n)/columns);
        float peak=0;
        for(int j=from;j<to;++j)peak=juce::jmax(peak,std::abs(d->samples.getSample(0,j)));
        values[size_t(i)]=peak;
    }
    return values;
}
