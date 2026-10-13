#include "PluginProcessor.h"
#include "PluginEditor.h"
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
    c("waveShape","Wave",{"Sine","Triangle","Saw","Square"},2);
    f("unison","Unison",1,8,1,1); f("detune","Detune",0,100,0); f("width","Width",0,1,0);
    f("waveOctave","Wave Octave",-3,3,0,1); f("waveLevel","Wave Level",-60,6,0,0.1);
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
        c((pre+"Shape").toRawUTF8(),(pre+" Shape").toRawUTF8(),{"Sine","Triangle","Saw","Square"},0);
    }
    c("filterType","Filter Type",{"Low Pass","High Pass","Band Pass","Notch","Comb"},0);
    f("cutoff","Cutoff",30,18000,1200); f("resonance","Resonance",0.05f,0.95f,0.15f);
    f("drive","Drive",0,1,0); f("filterMix","Filter Mix",0,1,1);
    f("delayTime","Delay Time",0.04f,1.8f,0.35f);
    f("delayFeedback","Delay Feedback",0,0.94f,0.35f);
    b("pingPong","Ping Pong",true); f("delayMix","Delay Mix",0,1,0.4f);
    c("convMode","Convolver",{"AUUU","CROSS","CATHOLIC","DARK"},0);
    f("convMix","Convolver Mix",0,1,1);
    f("reverbDecay","Reverb Decay",0.5f,60,60);
    f("reverbMix","Reverb Mix",0,1,1);
    c("mod1Source","Mod 1 Source",{"Off","LFO 1","LFO 2","AMP ENV","FILTER ENV"},1);
    c("mod2Source","Mod 2 Source",{"Off","LFO 1","LFO 2","AMP ENV","FILTER ENV"},2);
    c("mod3Source","Mod 3 Source",{"Off","LFO 1","LFO 2","AMP ENV","FILTER ENV"},0);
    c("mod4Source","Mod 4 Source",{"Off","LFO 1","LFO 2","AMP ENV","FILTER ENV"},0);
    for(int i=1;i<=4;++i) {
        auto s="mod"+juce::String(i);
        c((s+"Target").toRawUTF8(),(s+" Target").toRawUTF8(),{"Off","Cutoff","Wave Detune","Wave Width","Delay Feedback","Convolver Mix"},0);
        f((s+"Amount").toRawUTF8(),(s+" Amount").toRawUTF8(),-1,1,0);
    }
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

void AerytherAudioProcessor::DelayLine::prepare(int maximumSamples) {
    for(auto& r:ring)r.assign(size_t(maximumSamples),0.0f);
    cursor=0;
}
void AerytherAudioProcessor::DelayLine::process(juce::AudioBuffer<float>& b,float seconds,float feedback,
                                                double sr,bool pingPong) {
    if(ring[0].empty())return;
    int n=int(ring[0].size()),delay=juce::jlimit(1,n-1,int(seconds*sr));
    for(int i=0;i<b.getNumSamples();++i) {
        int read=(cursor-delay+n)%n;
        float l=ring[0][size_t(read)],r=ring[1][size_t(read)];
        float inl=b.getSample(0,i),inr=b.getSample(1,i);
        ring[0][size_t(cursor)]=inl+(pingPong?r:l)*feedback;
        ring[1][size_t(cursor)]=inr+(pingPong?l:r)*feedback;
        b.setSample(0,i,l);b.setSample(1,i,r);
        cursor=(cursor+1)%n;
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
    for(int mode=0;mode<4;++mode) {
        auto& conv=convolutions[size_t(mode)];
        conv.prepare({sr,juce::uint32(maxBlock),2});
        const double duration=mode==3?6.0:mode==2?4.5:mode==1?4.0:5.0;
        auto length=int(sr*duration);
        juce::AudioBuffer<float> ir(2,length);
        std::mt19937 random(uint32_t(0xAEA7 + mode*179));
        std::uniform_real_distribution<float> dist(-1.f,1.f);
        for(int ch=0;ch<2;++ch) {
            float low=0; auto* p=ir.getWritePointer(ch);
            for(int i=0;i<length;++i) {
                double t=double(i)/sr, fade=std::sin(juce::jmin(1.0,t/0.13)*juce::MathConstants<double>::halfPi);
                double fall=std::exp(-t/(duration*(mode==3?0.30:0.41)));
                float noise=dist(random); float smoothing=mode==3?0.02f:mode==1?0.085f:0.16f;
                low+=smoothing*(noise-low);
                double shimmer=mode==2?std::sin(2*juce::MathConstants<double>::pi*float(420.0+ch*7)*t):0;
                p[i]=float(fall*fade*(low*0.9+noise*0.055+shimmer*0.018));
            }
            p[0]=0.0f; // suppress dry transient, IR is intentionally diffuse
        }
        conv.loadImpulseResponse(std::move(ir),sr,juce::dsp::Convolution::Stereo::yes,
                                 juce::dsp::Convolution::Trim::no,
                                 juce::dsp::Convolution::Normalise::yes);
    }
}

void AerytherAudioProcessor::prepareToPlay(double sr,int maximumBlock) {
    currentSampleRate=sr;
    blockCapacity=juce::jmax(64,maximumBlock);
    for(auto& voice:waveVoices)voice.envelope.setSampleRate(sr);
    for(auto& voice:sampleVoices)voice.envelope.setSampleRate(sr);
    for(auto& d:layerDelays)d.prepare(int(sr*2.0)+4);
    for(auto& f:layerFilters)f.prepare(sr,maximumBlock);
    for(auto& b:layerAudio)b.setSize(2,blockCapacity);
    for(auto* b:{&convInput,&processed,&convWet,&wetScratch})b->setSize(2,blockCapacity);
    makeImpulseResponses(sr,blockCapacity);
    reverb.prepare(sr);
    filterEnvelope.setSampleRate(sr);
    amplitudeModEnvelope.setSampleRate(sr);
    transportFrameCounter=0;
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
        heldNotes.fill(false);filterEnvelope.reset();amplitudeModEnvelope.reset();
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
        heldNotes[size_t(note)]=true;
        auto makeVoice=[&](auto& pool){
            SynthVoice* v=nullptr;
            for(auto& item:pool)if(!item.active){v=&item;break;}
            if(!v)v=&pool[0];
            v->active=true;v->note=note;v->samplePosition=0;
            v->phases.fill(0);
            juce::ADSR::Parameters env;
            env.attack=get("ampAttack");env.decay=get("ampDecay");
            env.sustain=get("ampSustain");env.release=get("ampRelease");
            v->envelope.setParameters(env);v->envelope.reset();v->envelope.noteOn();
        };
        makeVoice(waveVoices);
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
    for(int i=1;i<=4;++i) {
        auto prefix="mod"+juce::String(i);
        int target=int(routedParam(const_cast<juce::AudioProcessorValueTreeState&>(state),prefix+"Target"));
        int source=int(routedParam(const_cast<juce::AudioProcessorValueTreeState&>(state),prefix+"Source"));
        float depth=routedParam(const_cast<juce::AudioProcessorValueTreeState&>(state),prefix+"Amount");
        int requested=juce::String(destination)=="cutoff"?1:
                      juce::String(destination)=="detune"?2:
                      juce::String(destination)=="width"?3:
                      juce::String(destination)=="delayFeedback"?4:
                      juce::String(destination)=="convMix"?5:0;
        if(target==requested && source>0)
            original+=depth*(source==1?lfo1:source==2?lfo2:source==3?currentAmpModEnv:currentFilterEnv);
    }
    return original;
}

void AerytherAudioProcessor::renderOscillators(juce::AudioBuffer<float>& w,juce::AudioBuffer<float>& sub,
                                                  int first,int length,float lfo1,float lfo2) {
    const int shape=int(get("waveShape")),unison=int(get("unison"));
    const float dt=clampf(modulated("detune",get("detune"),lfo1,lfo2),0,100);
    const float width=clampf(modulated("width",get("width"),lfo1,lfo2),0,1);
    const auto gain=juce::Decibels::decibelsToGain(get("waveLevel"))*0.2f;
    for(auto& v:waveVoices)if(v.active) {
        const double note=double(v.note)+12.0*get("waveOctave");
        for(int i=first;i<first+length;++i) {
            float amp=v.envelope.getNextSample();
            if(!v.envelope.isActive()){v.active=false;break;}
            float left=0,right=0;
            for(int u=0;u<unison;++u) {
                double spread=unison>1?(double(u)/(unison-1)*2.0-1.0):0.0;
                double f=midiHz(note+spread*dt*0.0045);
                auto& phase=v.phases[size_t(u)];phase+=f/currentSampleRate;
                if(phase>=1)phase-=std::floor(phase);
                double s=shape==0?std::sin(phase*2*pi):
                        shape==1?4.0*std::abs(phase-0.5)-1.0:
                        shape==2?2.0*phase-1.0:(phase<0.5?1.0:-1.0);
                double pan=spread*width;
                left+=float(s*(1.0-pan)*0.5);right+=float(s*(1.0+pan)*0.5);
            }
            w.addSample(0,i,left*amp*gain/unison);
            w.addSample(1,i,right*amp*gain/unison);
        }
    }
    int lowest=-1;
    for(int n=0;n<128;++n)if(heldNotes[size_t(n)]){lowest=n;break;}
    const auto sg=juce::Decibels::decibelsToGain(get("subLevel"))*0.35f;
    for(int i=first;i<first+length;++i) {
        if(lowest<0)continue;
        double n=lowest+12.0*get("subOctave")+get("subTune")/100.0;
        subPhases[0]+=midiHz(n)/currentSampleRate;
        if(subPhases[0]>=1)subPhases[0]-=std::floor(subPhases[0]);
        double s=int(get("subShape"))==0?std::sin(2*pi*subPhases[0]):
                   4.0*std::abs(subPhases[0]-0.5)-1.0;
        sub.addSample(0,i,float(s)*sg);sub.addSample(1,i,float(s)*sg);
    }
}

void AerytherAudioProcessor::renderSampler(juce::AudioBuffer<float>& out,int first,int length,
                                             bool looping,double ppq,double hostBpm,
                                             int64_t sampleTime,double srcBpm) {
    auto data=std::atomic_load(&sample);
    if(!data || data->samples.getNumSamples()<2)return;
    int total=data->samples.getNumSamples();
    int start=juce::jlimit(0,total-2,int(get("loopStart")*(total-1)));
    int end=juce::jlimit(start+2,total,int(get("loopEnd")*total));
    double loopSize=double(end-start);
    auto gain=juce::Decibels::decibelsToGain(get("sampleLevel"));
    auto dir=int(get("direction"));
    if(looping) {
        // 50%-overlapped Hann grains: playhead anchor follows host beats, while the
        // pitch within each grain remains tied to the file's original sample rate.
        // Crossfade controls how long the smoothed grains remain overlapped.
        int grainLength=juce::jmax(64,int(currentSampleRate*(0.035+get("crossfade")*0.55)));
        int hop=grainLength/2;
        for(int i=first;i<first+length;++i) {
            double positionInHost=double(sampleTime+i);
            int64_t k=int64_t(std::floor(positionInHost/hop));
            for(int ch=0;ch<2;++ch) {
                double sum=0,weight=0;
                for(int overlap=0;overlap<2;++overlap){
                    int64_t anchor=(k-overlap)*int64_t(hop);
                    double offset=positionInHost-double(anchor);
                    if(offset<0 || offset>=grainLength)continue;
                    double ppqAtAnchor=ppq+(double(anchor)-double(sampleTime))*hostBpm/(60.0*currentSampleRate);
                    double head=ppqAtAnchor*60.0/(srcBpm*get("stretch"))*data->rate;
                    double play=head+offset*data->rate/currentSampleRate;
                    double x=aeryther::wrapped(play,loopSize);
                    double samplePos=dir==1 ? end-1-x : dir==2 ?
                        start+([&](){ const auto t=aeryther::wrapped(play,loopSize*2);return t<=loopSize?t:2*loopSize-t; }()) : start+x;
                    if(dir==1) samplePos=juce::jmax(double(start),samplePos);
                    float window=0.5f-0.5f*std::cos(2*pi*float(offset)/grainLength);
                    sum+=readInterpolated(data->samples,ch,samplePos)*window;
                    weight+=window;
                }
                if(weight>0.001)out.addSample(ch,i,float(sum/weight)*gain);
            }
        }
    } else {
        // Independent MIDI notes: note pitch and time stretch are decoupled by
        // overlapping grains. The note-on point always starts at the selected region.
        const int grainLength=juce::jmax(64,int(currentSampleRate*(0.035+get("crossfade")*0.55)));
        const int hop=juce::jmax(1,grainLength/2);
        const double sourcePerOutput=data->rate/currentSampleRate;
        const double stretch=get("stretch");
        for(auto& v:sampleVoices)if(v.active) {
            const double pitch=std::pow(2.0,(v.note-get("root")+get("sampleFine")/100.0)/12.0);
            const double noteTotalOutputFrames=loopSize/sourcePerOutput*stretch;
            for(int i=first;i<first+length;++i){
                auto amp=v.envelope.getNextSample();
                if(!v.envelope.isActive() || v.samplePosition>=noteTotalOutputFrames){v.active=false;break;}
                const int64_t k=int64_t(std::floor(v.samplePosition/hop));
                for(int ch=0;ch<2;++ch){
                    double sum=0, weight=0;
                    for(int overlap=0;overlap<2;++overlap){
                        int64_t anchor=(k-overlap)*int64_t(hop);
                        double offset=v.samplePosition-double(anchor);
                        if(offset<0 || offset>=grainLength)continue;
                        double at=double(anchor)*sourcePerOutput/stretch+offset*pitch*sourcePerOutput;
                        double sampleAt=start+aeryther::wrapped(at,loopSize);
                        float window=0.5f-0.5f*std::cos(2*pi*float(offset)/grainLength);
                        sum+=readInterpolated(data->samples,ch,sampleAt)*window;
                        weight+=window;
                    }
                    if(weight>0.001)out.addSample(ch,i,float(sum/weight)*amp*gain);
                }
                v.samplePosition+=1;
            }
        }
    }
}

void AerytherAudioProcessor::applyRouting(juce::AudioBuffer<float>& output,int samples,float lfo1,float lfo2) {
    convInput.clear();processed.clear();convWet.clear();
    for(int layer=0;layer<3;++layer) {
        const char* prefixes[]={"sample","wave","sub"};
        juce::String s(prefixes[layer]);
        auto& b=layerAudio[size_t(layer)];
        auto param=[&](const char* suffix){return routedParam(state,s+suffix)>0.5f;};
        if(param("Direct")) {
            for(int ch=0;ch<2;++ch)output.addFrom(ch,0,b,ch,0,samples);
            continue;
        }
        if(param("Filter")){
            float originalCutoff=get("cutoff");
            float cutoff=clampf(originalCutoff*std::pow(2.0f,modulated("cutoff",0,lfo1,lfo2)*4.0f),30,18000);
            layerFilters[size_t(layer)].process(b,int(get("filterType")),cutoff,get("resonance"),
                                                get("drive"),get("filterMix"),currentSampleRate);
        }
        if(param("Delay")) {
            wetScratch.makeCopyOf(b,true);
            layerDelays[size_t(layer)].process(wetScratch,get("delayTime"),
                clampf(modulated("delayFeedback",get("delayFeedback"),lfo1,lfo2),0,0.94f),
                currentSampleRate,enabled("pingPong"));
            const float mix=clampf(get("delayMix"),0.f,1.f);
            for(int ch=0;ch<2;++ch){
                b.applyGain(ch,0,samples,1.f-mix);
                b.addFrom(ch,0,wetScratch,ch,0,samples,mix);
            }
        }
        auto& dest=param("Conv")?convInput:processed;
        for(int ch=0;ch<2;++ch)dest.addFrom(ch,0,b,ch,0,samples);
    }
    auto mode=juce::jlimit(0,3,int(get("convMode")));
    convWet.makeCopyOf(convInput,true);
    juce::dsp::AudioBlock<float> block(convWet);
    juce::dsp::ProcessContextReplacing<float> context(block);
    convolutions[size_t(mode)].process(context);
    float mix=clampf(modulated("convMix",get("convMix"),lfo1,lfo2),0,1);
    for(int ch=0;ch<2;++ch) {
        for(int i=0;i<samples;++i) {
            float dry=convInput.getSample(ch,i),wet=convWet.getSample(ch,i);
            processed.addSample(ch,i,aeryther::wetDry(dry,wet,mix));
        }
    }
    wetScratch.makeCopyOf(processed,true);
    reverb.process(wetScratch,get("reverbDecay"));
    float rv=get("reverbMix");
    for(int ch=0;ch<2;++ch) {
        for(int i=0;i<samples;++i)
            output.addSample(ch,i,aeryther::wetDry(processed.getSample(ch,i),wetScratch.getSample(ch,i),rv));
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
    bool loop=(sampleMode == aeryther::SamplePlaybackMode::HostLoop);
    // MIDI event sample offsets are honored; transport looping follows the host's absolute timeline.
    double lfo[2]{};
    for(int k=0;k<2;++k) {
        auto rate=get(k==0?"lfo1Rate":"lfo2Rate");
        int shape=int(get(k==0?"lfo1Shape":"lfo2Shape"));
        auto phase=lfoPhases[k];
        lfo[k]=(shape==0?std::sin(2*pi*phase):shape==1?4*std::abs(phase-0.5)-1:
                shape==2?2*phase-1:phase<0.5?1:-1)*get(k==0?"lfo1Depth":"lfo2Depth");
        lfoPhases[k]=aeryther::wrapped(phase+rate*num/currentSampleRate,1.0);
    }
#if !defined(AERYTHER_FX)
    int rendered=0;
    for(const auto metadata:midi) {
        int offset=juce::jlimit(0,num,metadata.samplePosition);
        if(offset>rendered) {
            if(sampleMode != aeryther::SamplePlaybackMode::Silent)
                renderSampler(layerAudio[0],rendered,offset-rendered,loop,ppq,hostBpm,hostSamples,get("sampleBpm"));
            renderOscillators(layerAudio[1],layerAudio[2],rendered,offset-rendered,float(lfo[0]),float(lfo[1]));
        }
        handleMidi(metadata.getMessage());rendered=offset;
    }
    if(rendered<num) {
        if(sampleMode != aeryther::SamplePlaybackMode::Silent)
            renderSampler(layerAudio[0],rendered,num-rendered,loop,ppq,hostBpm,hostSamples,get("sampleBpm"));
        renderOscillators(layerAudio[1],layerAudio[2],rendered,num-rendered,float(lfo[0]),float(lfo[1]));
    }
#endif
    currentFilterEnv=filterEnvelope.getNextSample();
    currentAmpModEnv=amplitudeModEnvelope.getNextSample();
    for(int i=1;i<num;++i) {filterEnvelope.getNextSample();amplitudeModEnvelope.getNextSample();}
    applyRouting(output,num,float(lfo[0]),float(lfo[1]));
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
