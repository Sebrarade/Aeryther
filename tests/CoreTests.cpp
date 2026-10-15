#include "Logic.h"
#include "HugeHallImpulse.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>

int main() {
  using namespace aeryther;
  static_assert(Defaults::convolutionMix == 1.0f);
  static_assert(Defaults::reverbMix == 1.0f);
  static_assert(Defaults::neutralDetune == 0.0f);
  static_assert(Defaults::unityGainDecibels == 0.0f);
  assert(wetDry(.8f,.2f,1.f)==.2f);
  assert(wetDry(.8f,.2f,0.f)==.8f);
  assert(wetDry(.8f,.2f,2.f)==.2f);
  // Every route combination must have independent DIRECT and explicit
  // serial effect flags. All-off must be silent (no implicit bypass).
  for (bool f : {false,true})
  for (bool d : {false,true})
  for (bool c : {false,true})
  for (bool direct : {false,true}) {
    const auto p=getPaths({f,d,c,direct});
    assert(p.dry==direct);
    assert(p.filtered==f);
    assert(p.delayed==d);
    assert(p.convolved==c);
    assert(p.hasProcessedPath()==(f||d||c));
    assert(p.audible()==(direct||f||d||c));
  }
  assert(!getPaths({false,false,false,false}).audible());
  assert(getPaths({true,true,true,true}).dry &&
         getPaths({true,true,true,true}).convolved);
  assert(getPaths({false,false,false,true}).dry &&
         !getPaths({false,false,false,true}).hasProcessedPath());
  assert(midiTriggersSample(false));
  assert(!midiTriggersSample(true));
  assert(midiTriggersWave(true)&&midiTriggersSub(true));
  assert(sampleLoops(true,true)&&!sampleLoops(true,false));
  assert(samplePlaybackMode(true,false)==SamplePlaybackMode::Silent);
  assert(samplePlaybackMode(true,true)==SamplePlaybackMode::HostLoop);
  assert(samplePlaybackMode(false,false)==SamplePlaybackMode::Midi);
  assert(samplePlaybackMode(false,true)==SamplePlaybackMode::Midi);
  assert(wrapped(-3,10)==7);
  assert(wrapped(12,10)==2);
  assert(sampleReadPosition(0,120,48000,100,48100,0)==100);
  assert(std::abs(sampleReadPosition(1,120,48000,100,48100,0)-24100)<0.001);
  assert(std::abs(sampleReadPosition(1,120,48000,100,48100,1)-24100)<0.001);
  assert(std::abs(sampleReadPosition(2,120,48000,100,48100,2)-48100)<0.001);
  assert(sampleReadPosition(1,0,48000,0,48000,0)==0);
  assert(std::isfinite(wetDry(.4f,.2f,.5f)));
  // Delay sync: at 120 BPM 1/4 = 500ms, dotted 1/8 = 375ms,
  // triplet 1/8 = 166.67ms; safe BPM bounds and shape windows.
  assert(std::abs(tempoDelaySeconds(120.0,2)-0.5)<1.e-7);
  assert(std::abs(tempoDelaySeconds(120.0,7)-0.375)<1.e-7);
  assert(std::abs(tempoDelaySeconds(120.0,10)-1.0/6.0)<1.e-7);
  assert(std::abs(tempoDelaySeconds(60.0,0)-4.0)<1.e-7);
  assert(std::abs(tempoDelaySeconds(0.0,0)-8.0)<1.e-7);
  // Reject invalid ceiling choices and never allow feedback above +3 octaves.
  assert(shimmerOctaveCeiling(-1)==2);
  assert(shimmerOctaveCeiling(2)==2);
  assert(shimmerOctaveCeiling(3)==3);
  assert(shimmerOctaveCeiling(99)==3);
  assert(std::abs(shimmerGrainWindow(0.f))<1.e-6f);
  assert(std::abs(shimmerGrainWindow(.5f)-1.f)<1.e-6f);
  for (int i=0;i<100;++i){
      float p=float(i)/100.f;
      const float p2=std::fmod(p+.5f,1.f);
      assert(std::abs(shimmerGrainWindow(p)+shimmerGrainWindow(p2)-1.f)<1.e-5f);
  }
  // Reverb 0% must preserve dry at unity, while intermediate positions use
  // an equal-power fade instead of the old loudness-dropping linear blend.
  assert(std::abs(reverbGains(0.f).dry-1.f)<0.00001f);
  assert(std::abs(reverbGains(0.f).wet)<0.00001f);
  assert(std::abs(reverbGains(1.f).dry)<0.00001f);
  assert(std::abs(reverbGains(1.f).wet-2.2f)<0.00001f);
  assert(reverbGains(.5f).dry>.70f && reverbGains(.5f).wet>1.55f);
  assert(std::isfinite(reverbGains(.67f).wet));
  const auto energy=[](const std::vector<float>& audio,int a,int b) {
    const int from=std::clamp(a,0,int(audio.size()));
    const int end=std::clamp(b,from,int(audio.size()));
    double sum=0;
    for(int i=from;i<end;++i)sum+=double(audio[size_t(i)])*audio[size_t(i)];
    return end>from?std::sqrt(sum/double(end-from)):0.0;
  };
  for(int mode=0;mode<4;++mode) {
    const auto profile=hugeHallProfiles[size_t(mode)];
    assert(profile.durationSeconds>=30.0);
    assert(profile.bloomSeconds>=0.8);
    auto left=generateHugeHallImpulse(mode,0,4000.0);
    auto right=generateHugeHallImpulse(mode,1,4000.0);
    assert(left.size()==size_t(std::ceil(profile.durationSeconds*4000.0)));
    assert(left.size()==right.size());
    assert(energy(left,0,300)==0.0);
    assert(energy(left,4000*3,4000*6)>0.001);
    assert(energy(left,4000*18,4000*22)>0.0000001);
    assert(energy(left,int(left.size())-4000,int(left.size()))<
           energy(left,4000*4,4000*7));
    assert(energy(right,4000*3,4000*6)>0.001);
    bool distinct=false;
    for(int i=4000*5;i<4000*6;++i)if(left[size_t(i)]!=right[size_t(i)]) {
      distinct=true;break;
    }
    assert(distinct);
    // Full reverse inverts the acoustic impulse only and preserves the same
    // stereo response and length. A second reverse restores the original.
    auto before=left;
    reverseHugeHallImpulse(left);
    assert(left.size()==before.size());
    for(size_t i : {size_t(0),left.size()/3,left.size()-1})
        assert(left[i]==before[left.size()-1-i]);
    reverseHugeHallImpulse(left);
    for(size_t i : {size_t(0),left.size()/3,left.size()-1})
        assert(left[i]==before[i]);
  }
  std::cout << "Huge-hall IR: 4 modes, long tails, stereo decorrelation passed\\n";
  std::cout << "Aeryther source tests: defaults/routing/wet/transport/direction passed\n";
}
