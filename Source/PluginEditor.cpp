#include "PluginEditor.h"
#include <cmath>

namespace {
const juce::Colour ink=juce::Colour::fromRGB(33,53,78);
const juce::Colour blue=juce::Colour::fromRGB(69,143,218);
const juce::Colour pale=juce::Colour::fromRGB(235,241,249);
const juce::Colour border=juce::Colour::fromRGB(205,215,229);
const juce::Colour white=juce::Colour::fromRGB(248,250,254);
#if defined(AERYTHER_FX)
constexpr float uiScale=0.70f;
#else
constexpr float uiScale=0.68f;
#endif
juce::Rectangle<int> compact(juce::Rectangle<int> r){
 return {juce::roundToInt(r.getX()*uiScale),juce::roundToInt(r.getY()*uiScale),
 juce::roundToInt(r.getWidth()*uiScale),juce::roundToInt(r.getHeight()*uiScale)};
}
}
AerytherAudioProcessorEditor::LightLook::LightLook(){
    setColour(juce::ComboBox::backgroundColourId,pale);
    setColour(juce::ComboBox::textColourId,ink);
    setColour(juce::ComboBox::outlineColourId,border);
    setColour(juce::TextButton::buttonColourId,pale);
    setColour(juce::TextButton::textColourOffId,ink);
    setColour(juce::Label::textColourId,ink);
    setColour(juce::Slider::textBoxTextColourId,ink);
    setColour(juce::Slider::textBoxBackgroundColourId,pale);
    setColour(juce::Slider::textBoxOutlineColourId,border);
    setColour(juce::ComboBox::arrowColourId,blue);
    setColour(juce::PopupMenu::backgroundColourId,white);
    setColour(juce::PopupMenu::textColourId,ink);
}
void AerytherAudioProcessorEditor::LightLook::drawRotarySlider(juce::Graphics& g,int x,int y,int w,int h,
                float pos,float from,float to,juce::Slider&) {
    auto cx=float(x)+float(w)*0.5f,cy=float(y)+float(h)*0.5f;
    auto r=float(juce::jmin(w,h))*0.38f;
    g.setColour(white);g.fillEllipse(cx-r,cy-r,2*r,2*r);
    g.setColour(border);g.drawEllipse(cx-r,cy-r,2*r,2*r,1.5f);
    juce::Path arc; arc.addCentredArc(cx,cy,r+3,r+3,0,from,to,true);
    g.setColour(border);g.strokePath(arc,juce::PathStrokeType(2.3f));
    juce::Path active;active.addCentredArc(cx,cy,r+3,r+3,0,from,from+pos*(to-from),true);
    g.setColour(blue);g.strokePath(active,juce::PathStrokeType(2.3f));
    // Exactly ONE pointer, not the double blue marker from the concept renders.
    float a=from+pos*(to-from);
    g.setColour(ink);g.drawLine(cx+std::sin(a)*r*.24f,cy-std::cos(a)*r*.24f,
                               cx+std::sin(a)*r*.80f,cy-std::cos(a)*r*.80f,1.7f);
}
void AerytherAudioProcessorEditor::LightLook::drawButtonBackground(juce::Graphics& g,juce::Button& b,
                                  const juce::Colour&,bool over,bool down) {
    auto r=b.getLocalBounds().toFloat().reduced(1);
    g.setColour(down?pale:over?white:pale);g.fillRoundedRectangle(r,5);
    g.setColour(border);g.drawRoundedRectangle(r,5,1);
}
void AerytherAudioProcessorEditor::LightLook::drawToggleButton(juce::Graphics& g,juce::ToggleButton& b,
                                                                 bool,bool) {
    auto r=b.getLocalBounds().toFloat().reduced(1);
    g.setColour(b.getToggleState()?juce::Colour::fromRGB(208,227,247):pale);
    g.fillRoundedRectangle(r,5);
    g.setColour(b.getToggleState()?blue:border);g.drawRoundedRectangle(r,5,1);
    g.setColour(ink);g.setFont(juce::FontOptions(14.f));
    g.drawFittedText(b.getButtonText(),b.getLocalBounds().reduced(2,0),juce::Justification::centred,1);
}
void AerytherAudioProcessorEditor::LightLook::drawLinearSlider(
    juce::Graphics& g,int x,int y,int width,int height,float sliderPos,
    float,float,const juce::Slider::SliderStyle style,juce::Slider& slider) {
    if(style!=juce::Slider::LinearHorizontal) {
        juce::LookAndFeel_V4::drawLinearSlider(g,x,y,width,height,sliderPos,0.f,0.f,style,slider);
        return;
    }
    // Bipolar modulation: deep blue centre line, blue level and small circular thumb.
    const auto area=juce::Rectangle<float>{float(x),float(y),float(width),float(height)};
    g.setColour(pale);
    g.fillRoundedRectangle(area.reduced(0.f,2.f),5.f);
    const float midX=area.getCentreX();
    const float yMid=area.getCentreY();
    g.setColour(border);
    g.fillRoundedRectangle(area.getX()+6.f,yMid-2.f,area.getWidth()-12.f,4.f,2.f);
    const float pos=juce::jlimit(area.getX()+7.f,area.getRight()-7.f,sliderPos);
    g.setColour(blue.withAlpha(.9f));
    if(std::abs(pos-midX)>1.f)
        g.fillRoundedRectangle(juce::jmin(midX,pos),yMid-2.f,
                               std::abs(midX-pos),4.f,2.f);
    g.setColour(ink.withAlpha(.65f));
    g.drawLine(midX,yMid-6.f,midX,yMid+6.f,1.f);
    g.setColour(blue);
    g.fillEllipse(pos-5.f,yMid-5.f,10.f,10.f);
}
AerytherAudioProcessorEditor::AerytherAudioProcessorEditor(AerytherAudioProcessor& p)
    :juce::AudioProcessorEditor(&p),processor(p){
    setLookAndFeel(&look);setOpaque(true);
#if defined(AERYTHER_FX)
    setSize(758,338);
    // FX keeps only the processing controls; no sampler or MIDI layers.
    addSwitch("sampleFilter","FILTER ON",{55,80,140,32});
    addChoice("filterType",{54,124,195,28});
    addDial("cutoff","CUTOFF",{48,161,75,85});
    addDial("resonance","RESONANCE",{140,161,79,85});
    addDial("filterMix","MIX",{235,161,75,85});
    addSwitch("sampleDelay","DELAY ON",{340,80,140,32});
    addSwitch("pingPong","PING PONG",{486,80,125,32});
    addSwitch("delaySync","SYNC",{337,125,78,28});
    addChoice("delayDivision",{426,125,178,28});
    addDial("delayTime","TIME",{330,179,60,83});
    addDial("delayFeedback","FEEDBACK",{402,179,60,83});
    addDial("delayMix","MIX",{474,179,60,83});
    addDial("delayShimmer","SHIMMER",{546,179,60,83});
    addChoice("shimmerMaxOctaves",{337,282,267,26});
    addSwitch("sampleConv","CONV ON",{641,80,110,32});
    addChoice("convMode",{636,126,160,28});
    addSwitch("convReverse","REVERSE",{638,174,87,27});
    addDial("convMix","MIX",{735,172,75,80});
    addDial("reverbDecay","DECAY",{846,161,66,85});
    addDial("reverbTone","TONE",{923,161,66,85});
    addDial("reverbMix","MIX",{999,161,66,85});
    addSwitch("sampleDirect","DIRECT",{640,258,140,30});
    // Shared convolver EQ strip: EQ colour follows PRE / POST convolution.
    addSwitch("eqEnabled","EQ ON",{49,396,114,32});
    addChoice("eqPosition",{193,398,236,30});
    // Three dials share equal widths and generous horizontal spacing.
    addDial("eqLow","LOW",{510,384,92,82});
    addDial("eqMid","MID",{680,384,92,82});
    addDial("eqHigh","HIGH",{850,384,92,82});
#else
    setSize(1033,714);
    browse.onClick=[this]{browseForAudio();};addAndMakeVisible(browse);
    // Top row: Sample (left), Wave (middle), Sub (right)
    // All SAMPLE dials share one row; BPM and ROOT get generous, readable
    // numeric text boxes below the knobs without overlapping other controls.
    auto sampleKnobs=std::array<const char*,6>{"root","sampleBpm","sampleFine",
                                              "stretch","crossfade","sampleLevel"};
    auto sampleLabels=std::array<const char*,6>{"ROOT","BPM","FINE","STRETCH",
                                               "CROSSFADE","LEVEL"};
    for(int i=0;i<6;++i)
        addDial(sampleKnobs[i],sampleLabels[i],{56+i*84,290,70,96});
    addSwitch("loopPlay","LOOP PLAY",{54,245,112,31});
    addChoice("direction",{260,246,270,28});
    auto waveKnobs=std::array<const char*,7>{"unison","detune","width","waveOctave","waveFine","waveGlide","waveLevel"};
    auto waveLabels=std::array<const char*,7>{"UNISON","DETUNE","WIDTH","OCTAVE","FINE","GLIDE","LEVEL"};
    // Seven evenly spaced controls fit entirely inside WAVE (576..1185).
    for(int i=0;i<7;++i)addDial(waveKnobs[i],waveLabels[i],{592+i*82,292,70,80});
    addChoice("waveShape",{705,180,215,30});
    addChoice("subShape",{1275,180,185,30});
    for(int i=0;i<3;++i){const char* ids[]={"subOctave","subTune","subLevel"};
       const char* names[]={"OCTAVE","TUNE","LEVEL"};
       addDial(ids[i],names[i],{1203+i*111,291,72,83});}
    const char* routing[3]={"sample","wave","sub"};
    const int startX[3]={55,584,1200}, widths[3]={110,119,79};
    for(int k=0;k<3;++k)for(int i=0;i<4;++i){
        const juce::String prefix=routing[k];
        const char* suffix[]={"Filter","Delay","Conv","Direct"};
        const char* labels[]={"FILTER","DELAY","CONV","DIRECT"};
        addSwitch((prefix+suffix[i]).toRawUTF8(),labels[i],{startX[k]+i*widths[k],394,widths[k]-6,34});
    }
    const int envStarts[2]={58,448};
    for(int j=0;j<2;++j)for(int i=0;i<4;++i){
        juce::String prefix=j==0?"amp":"fenv";
        const char* names[]={"Attack","Decay","Sustain","Release"};
        addDial((prefix+names[i]).toRawUTF8(),juce::String(names[i]).toUpperCase().toRawUTF8(),
                {envStarts[j]+i*93,524,68,77});
    }
    for(int j=0;j<2;++j){
        int xx=840+j*343;
        juce::String prefix=j==0?"lfo1":"lfo2";
        addDial((prefix+"Rate").toRawUTF8(),"FREE RATE",{xx,526,68,76});
        addDial((prefix+"Depth").toRawUTF8(),"DEPTH",{xx+100,526,68,76});
        // Controls occupy separate vertical columns, below the LFO heading.
        // Avoid decorative waveforms underneath live ComboBoxes and dials.
        addChoice((prefix+"Shape").toRawUTF8(),{xx+187,507,126,27});
        addSwitch((prefix+"Sync").toRawUTF8(),"SYNC",{xx+187,541,75,26});
        addChoice((prefix+"Division").toRawUTF8(),{xx+187,578,126,27});
    }
    // Eight modulation assignments, arranged in two four-row banks.
    // Every control is inside the MODULATION MATRIX card and clear of its title.
    for(int i=1;i<=8;++i){
        juce::String prefix="mod"+juce::String(i);
        const int bank=(i-1)/4, row=(i-1)%4;
        const int y=681+row*24, dx=bank*730;
        addChoice((prefix+"Source").toRawUTF8(),{100+dx,y,195,22});
        addDial((prefix+"Amount").toRawUTF8(),"",{305+dx,y,134,22});
        addChoice((prefix+"Target").toRawUTF8(),{452+dx,y,240,22});
    }
    addChoice("filterType",{194,805,210,29});
    for(int i=0;i<4;++i){const char* ids[]={"cutoff","resonance","drive","filterMix"};
        const char* names[]={"CUTOFF","RESONANCE","DRIVE","MIX"};
        addDial(ids[i],names[i],{53+i*113,835,71,73});}
    // Two compact DELAY rows leave room for EQ without increasing window height.
    addChoice("shimmerMaxOctaves",{637,805,197,26});
    for(int i=0;i<4;++i){
        const char* ids[]={"delayTime","delayFeedback","delayMix","delayShimmer"};
        const char* names[]={"TIME","FEEDBACK","MIX","SHIMMER"};
        addDial(ids[i],names[i],{504+i*82,836,65,64});
    }
    addSwitch("delaySync","SYNC",{511,906,81,26});
    addChoice("delayDivision",{602,906,118,26});
    addSwitch("pingPong","PING-PONG",{730,906,106,26});
    addChoice("convMode",{877,830,295,29});
    addSwitch("convReverse","REVERSE",{1041,866,126,30});
    addDial("convMix","MIX",{1228,829,72,80});
    addDial("reverbDecay","DECAY",{1332,831,54,80});
    addDial("reverbTone","TONE",{1394,831,54,80});
    addDial("reverbMix","MIX",{1456,831,54,80});
    // Compact 3-band strip: EQ stays visible below the FX without a huge footer.
    addSwitch("eqEnabled","EQ ON",{55,993,132,28});
    addChoice("eqPosition",{216,993,231,28});
    // Evenly distribute LOW / MID / HIGH across the strip, not clustered left.
    addDial("eqLow","LOW",{620,971,88,62});
    addDial("eqMid","MID",{917,971,88,62});
    addDial("eqHigh","HIGH",{1214,971,88,62});
    refreshParameterReadouts();
    startTimerHz(24);
#endif
    // Native corner resizer; the ratio is locked, so every control, font and
    // waveform scales together without reflow or collision.
    setResizable(true,true);
    if(auto* bounds=getConstrainer()) {
        bounds->setFixedAspectRatio(double(defaultWidth)/double(defaultHeight));
        bounds->setSizeLimits(
            juce::roundToInt(defaultWidth*0.76),juce::roundToInt(defaultHeight*0.76),
            juce::roundToInt(defaultWidth*1.42),juce::roundToInt(defaultHeight*1.42));
    }
    resized();
}
AerytherAudioProcessorEditor::~AerytherAudioProcessorEditor(){setLookAndFeel(nullptr);}
float AerytherAudioProcessorEditor::contentScale() const noexcept {
    return float(getWidth())/float(defaultWidth);
}
void AerytherAudioProcessorEditor::addDial(const char* id,const char* title,juce::Rectangle<int> r){
    r=compact(r);
    auto c=std::make_unique<Dial>();
    const auto parameterId=juce::String(id);
    c->name=title;
    c->parameterID=parameterId;
    const bool matrixAmount=parameterId.startsWith("mod") &&
                             parameterId.endsWith("Amount");
    const bool hasReadout=parameterId=="root" || parameterId=="sampleBpm";
    c->slider.setSliderStyle(matrixAmount?juce::Slider::LinearHorizontal:
                                           juce::Slider::RotaryHorizontalVerticalDrag);
    if(matrixAmount) {
        // The previous JUCE textbox rendered white-on-white squares in Ableton.
        // Draw the value in our own high-contrast Label, outside the slider.
        c->slider.setTextBoxStyle(juce::Slider::NoTextBox,false,0,0);
        c->slider.setBounds(r.getX(),r.getY(),r.getWidth()-37,r.getHeight());
    } else if(hasReadout) {
        // Never rely on a Slider's embedded textbox. On the compact UI its
        // text can be clipped or drawn underneath the rotary slider.
        // Title, rotary knob and independently painted value occupy separate rows.
        c->slider.setTextBoxStyle(juce::Slider::NoTextBox,false,0,0);
        c->slider.setBounds(r.getX()+5,r.getY()+15,
                            juce::jmax(20,r.getWidth()-10),38);
    } else {
        c->slider.setTextBoxStyle(juce::Slider::NoTextBox,false,0,0);
        c->slider.setBounds(r.removeFromTop(juce::jmax(10,r.getHeight()-14)));
    }
    addAndMakeVisible(c->slider);

    if(!matrixAmount) {
        c->label.setText(title,juce::dontSendNotification);
        const float fontSize=juce::String(title).length()>7?11.25f:12.5f;
        c->label.setFont(juce::Font(juce::FontOptions(fontSize)));
        c->label.setJustificationType(juce::Justification::centred);
        c->label.setBounds(r.getX()-3,r.getY(),r.getWidth()+6,14);
        addAndMakeVisible(c->label);
    }

    if(matrixAmount) {
        c->valueLabel.setText("0.00",juce::dontSendNotification);
        c->valueLabel.setJustificationType(juce::Justification::centredRight);
        c->valueLabel.setFont(juce::Font(juce::FontOptions(13.25f)));
        c->valueLabel.setColour(juce::Label::textColourId,ink);
        c->valueLabel.setColour(juce::Label::backgroundColourId,pale);
        c->valueLabel.setBounds(r.getRight()-37,r.getY(),37,r.getHeight());
        c->valueLabel.setInterceptsMouseClicks(false,false);
        addAndMakeVisible(c->valueLabel);
    }
    if(hasReadout) {
        c->valueLabel.setText(parameterId=="root"?"C3":"120",juce::dontSendNotification);
        c->valueLabel.setJustificationType(juce::Justification::centred);
        c->valueLabel.setFont(juce::Font(juce::FontOptions(14.5f)));
        c->valueLabel.setColour(juce::Label::textColourId,ink);
        c->valueLabel.setColour(juce::Label::backgroundColourId,pale);
        c->valueLabel.setColour(juce::Label::outlineColourId,border);
        c->valueLabel.setBounds(r.getX()-3,r.getY()+54,r.getWidth()+6,19);
        c->valueLabel.setEditable(false,true,false); // double-click to type BPM or note
        auto* control=c.get();
        const bool isRoot=parameterId=="root";
        c->valueLabel.onTextChange=[this,control,isRoot]{
            const auto input=control->valueLabel.getText().trim();
            double next=control->slider.getValue();
            bool valid=false;
            if(isRoot) {
                for(int n=0;n<128;++n) {
                    if(juce::MidiMessage::getMidiNoteName(n,true,true,3)
                            .equalsIgnoreCase(input)) {
                        next=double(n);valid=true;break;
                    }
                }
                if(!valid && input.containsOnly("0123456789") && input.isNotEmpty()) {
                    next=juce::jlimit(0.0,127.0,input.getDoubleValue());
                    valid=true;
                }
            } else if(input.isNotEmpty() && input.containsOnly("0123456789.")) {
                next=juce::jlimit(30.0,300.0,input.getDoubleValue());
                valid=true;
            }
            if(valid)control->slider.setValue(next,juce::sendNotificationSync);
            refreshParameterReadouts();
        };
        addAndMakeVisible(c->valueLabel);
        c->slider.setTooltip(isRoot?"Root note: turn knob, or double-click note value to enter C3, F#3 etc"
                                   :"Sample BPM: turn knob, or double-click the number to enter BPM");
    } else {
        c->slider.setTooltip(juce::String(title)+": drag to adjust (double-click for default)");
    }
    c->attachment=std::make_unique<SlideAttach>(processor.state,id,c->slider);
    dials.push_back(std::move(c));
}
void AerytherAudioProcessorEditor::refreshParameterReadouts(){
    for(const auto& dial:dials) {
        const bool isRoot=dial->parameterID=="root";
        const bool isBpm=dial->parameterID=="sampleBpm";
        const bool isAmount=dial->parameterID.startsWith("mod") &&
                            dial->parameterID.endsWith("Amount");
        if((!isRoot && !isBpm && !isAmount) || dial->valueLabel.isBeingEdited())continue;
        const auto* raw=processor.state.getRawParameterValue(dial->parameterID);
        if(!raw)continue;
        const auto n=raw->load();
        const auto value=isRoot
            ? juce::MidiMessage::getMidiNoteName(juce::jlimit(0,127,juce::roundToInt(n)),true,true,3)
            : isBpm ? juce::String(juce::jlimit(30,300,juce::roundToInt(n)))
                    : juce::String(n,2);
        if(dial->valueLabel.getText()!=value)
            dial->valueLabel.setText(value,juce::dontSendNotification);
    }
}
void AerytherAudioProcessorEditor::timerCallback(){
    refreshParameterReadouts();
    repaint();
}
void AerytherAudioProcessorEditor::addSwitch(const char* id,const char* title,juce::Rectangle<int> r){
    r=compact(r);
    auto c=std::make_unique<Switch>();c->button.setButtonText(title);c->button.setBounds(r);
    const auto route=juce::String(id);
    if(route.endsWith("Direct"))
        c->button.setTooltip("Parallel dry path: adds the original layer beside the processed sound");
    else if(route.endsWith("Filter") || route.endsWith("Delay") || route.endsWith("Conv"))
        c->button.setTooltip("Processed path runs in order: FILTER > DELAY > CONVOLVER");
    addAndMakeVisible(c->button);
    c->attachment=std::make_unique<BoolAttach>(processor.state,id,c->button);
    switches.push_back(std::move(c));
}
void AerytherAudioProcessorEditor::addChoice(const char* id,juce::Rectangle<int> r){
    r=compact(r);
    auto c=std::make_unique<Choice>();c->box.setBounds(r);
    if(auto* p=dynamic_cast<juce::AudioParameterChoice*>(processor.state.getParameter(id))) {
        int item=1;for(auto& name:p->choices)c->box.addItem(name,item++);
    }
    addAndMakeVisible(c->box);
    c->attachment=std::make_unique<ChoiceAttach>(processor.state,id,c->box);
    choices.push_back(std::move(c));
}
void AerytherAudioProcessorEditor::addLabel(juce::Graphics& g,const juce::String& name,
                                            juce::Rectangle<float> r,int size) const {
    g.setColour(ink);g.setFont(juce::FontOptions(float(size),juce::Font::plain));
    g.drawText(name,r,juce::Justification::centredLeft,false);
}
void AerytherAudioProcessorEditor::panel(juce::Graphics& g,juce::Rectangle<int> rect,const char* title)const {
    auto r=rect.toFloat();g.setColour(white);g.fillRoundedRectangle(r,13.f);
    g.setColour(border);g.drawRoundedRectangle(r,13.f,1.f);
    g.setColour(blue);g.fillEllipse(r.getX()+17,r.getY()+17,10,10);
    addLabel(g,title,{r.getX()+35,r.getY()+10,r.getWidth()-45,25},16);
}
void AerytherAudioProcessorEditor::paintWaveform(juce::Graphics& g){
    auto r=waveformRect.toFloat();
    g.setColour(pale);g.fillRoundedRectangle(r,6);
    const auto name=processor.sampleFileName();
    if(name!=cachedSampleFile || cachedPeaks.empty()){
        cachedPeaks=processor.waveformPeaks(240);
        cachedSampleFile=name;
    }
    const auto& peaks=cachedPeaks;
    auto hasAudio=name.isNotEmpty();
    if(!hasAudio){addLabel(g,"Drop WAV / AIFF / FLAC here",r.reduced(17),15);return;}
    int n=int(peaks.size());
    g.setColour(juce::Colour::fromRGB(119,156,195));
    for(int i=0;i<n;++i){
        float x=r.getX()+r.getWidth()*i/n;
        float h=peaks[size_t(i)]*r.getHeight()*0.42f;
        g.drawVerticalLine(int(x),r.getCentreY()-h,r.getCentreY()+h);
    }
    float start=processor.state.getRawParameterValue("loopStart")->load();
    float end=processor.state.getRawParameterValue("loopEnd")->load();
    g.setColour(blue.withAlpha(.14f));g.fillRect(r.getX()+r.getWidth()*start,r.getY(),
                                          r.getWidth()*(end-start),r.getHeight());
    g.setColour(blue);g.drawVerticalLine(int(r.getX()+r.getWidth()*start),r.getY(),r.getBottom());
    g.drawVerticalLine(int(r.getX()+r.getWidth()*end),r.getY(),r.getBottom());
    // Animated sample readhead; host sync or MIDI-controlled playback.
    if(processor.sampleHeadActive()){
       float position=juce::jlimit(0.f,1.f,processor.sampleReadhead());
       float x=r.getX()+r.getWidth()*(start+(end-start)*position);
       g.setColour(juce::Colours::white.withAlpha(.75f));
       g.drawVerticalLine(int(x)+1,r.getY()+2,r.getBottom()-2);
       g.setColour(juce::Colour::fromRGB(23,107,213));
       g.drawVerticalLine(int(x),r.getY()+2,r.getBottom()-2);
    }
}
void AerytherAudioProcessorEditor::paint(juce::Graphics& g){
    g.fillAll(juce::Colour::fromRGB(239,243,249));
    g.addTransform(juce::AffineTransform::scale(uiScale*contentScale()));
#if defined(AERYTHER_FX)
    g.setColour(ink);g.setFont(juce::FontOptions(23.f));
    g.drawText("AERYTHER FX",juce::Rectangle<int>{0,7,1080,40},juce::Justification::centred);
    g.setColour(ink);g.setFont(juce::FontOptions(11.f));
    g.drawText("Volodymyr Gnatenko",juce::Rectangle<int>{18,17,235,24},juce::Justification::centredLeft);
    g.setColour(blue);g.setFont(juce::FontOptions(13.f));
    g.drawText("AERYTHER 1.0",juce::Rectangle<int>{914,15,145,25},juce::Justification::centredRight);
    panel(g,{20,56,294,235},"FILTER");
    panel(g,{321,56,290,286},"DELAY");
    panel(g,{619,56,205,235},"CONVOLVER");
    panel(g,{832,56,238,235},"REVERB");
    panel(g,{20,357,1050,119},"3 BAND EQ");
#else
    // Less vertical emphasis on branding; more emphasis on the working controls.
    g.setColour(ink);g.setFont(juce::FontOptions(25.f));
    g.drawFittedText("AERYTHER",{555,8,430,30},juce::Justification::centred,1);
    g.setFont(juce::FontOptions(10.f));
    g.drawFittedText("3 LAYER AMBIENT INSTRUMENT",{555,39,430,18},juce::Justification::centred,1);
    g.setColour(blue);g.setFont(juce::FontOptions(16.f));
    g.drawText("VERSION 1.1.1",juce::Rectangle<int>{1338,23,160,26},juce::Justification::centredRight);
    panel(g,{35,82,535,371},"SAMPLE");
    panel(g,{576,82,609,371},"WAVE");
    panel(g,{1190,82,320,371},"SUB");
    panel(g,{35,463,400,157},"AMP ENV");
    panel(g,{441,463,390,157},"FILTER ENV");
    panel(g,{836,463,339,157},"LFO 1");
    panel(g,{1181,463,329,157},"LFO 2");
    panel(g,{35,627,1475,155},"MODULATION MATRIX");
    panel(g,{35,795,452,118},"FILTER");
    panel(g,{493,795,350,144},"DELAY");
    panel(g,{849,795,467,118},"CONVOLVER");
    panel(g,{1322,795,188,118},"REVERB");
    panel(g,{35,948,1475,87},"3 BAND EQ");
    waveformRect={57,139,490,92};paintWaveform(g);
    // Minimal functional illustrations, drawn in JUCE rather than static imagery.
    auto graph=[&](juce::Rectangle<float> r, int kind, int traces){
        g.setColour(pale);g.fillRoundedRectangle(r,6);
        for(int trace=0;trace<traces;++trace){
            juce::Path curve;
            for(int i=0;i<210;++i){
                float t=float(i)/209.f;
                float phase=2.f*juce::MathConstants<float>::pi*(t*(kind==0?2.3f:1.f));
                float value=kind==0?std::sin(phase+trace*.35f):
                            kind==1?(1.f-4.f*std::abs(t-0.5f)):
                            std::sin(2.f*juce::MathConstants<float>::pi*t*(kind==3?3.f:2.f));
                float x=r.getX()+8.f+t*(r.getWidth()-16.f);
                float y=r.getCentreY()-value*(r.getHeight()*.34f);
                if(i==0)curve.startNewSubPath(x,y);else curve.lineTo(x,y);
            }
            g.setColour(blue.withAlpha(traces==1?.85f:.25f+trace*.09f));
            g.strokePath(curve,juce::PathStrokeType(1.4f));
        }
    };
    // Keep decorative waveforms in small dedicated spaces above the waveform selectors.
    // The old envelope/LFO art painted directly beneath actual interactive controls.
    graph({600.f,139.f,554.f,33.f},0,6);
    graph({1210.f,139.f,276.f,33.f},1,1);
    g.setColour(ink.withAlpha(.6f));g.setFont(juce::FontOptions(11.f));
    // Artist credit stays in the header and does not intersect the SAMPLE panel.
    g.drawText("Volodymyr Gnatenko",juce::Rectangle<int>{56,47,260,17},juce::Justification::left);
    g.drawFittedText(processor.sampleFileName(),
                     juce::Rectangle<int>{206,105,185,21},
                     juce::Justification::centredRight,1);
    for(int bank=0;bank<2;++bank){
        const int dx=bank*730;
        g.drawText("SOURCE",juce::Rectangle<int>{100+dx,662,180,16},juce::Justification::left);
        g.drawText("AMOUNT",juce::Rectangle<int>{305+dx,662,125,16},juce::Justification::left);
        g.drawText("DESTINATION",juce::Rectangle<int>{452+dx,662,190,16},juce::Justification::left);
    }
#endif
}
void AerytherAudioProcessorEditor::resized(){
    // Bounds are fixed in compact design coordinates. Scaling the children via
    // their component transforms preserves text, knobs, hit testing and spacing,
    // while painting uses the identical uniform scale factor.
    const float s=contentScale();
    const auto t=juce::AffineTransform::scale(s);
    for(auto& dial:dials) {
        dial->slider.setTransform(t);
        dial->label.setTransform(t);
        dial->valueLabel.setTransform(t);
    }
    for(auto& sw:switches)sw->button.setTransform(t);
    for(auto& ch:choices)ch->box.setTransform(t);
#if !defined(AERYTHER_FX)
    browse.setBounds(compact({409,103,137,27}));
    browse.setTransform(t);
#endif
}
bool AerytherAudioProcessorEditor::isInterestedInFileDrag(const juce::StringArray& files){
    for(const auto& item:files)if(juce::File(item).hasFileExtension("wav;aif;aiff;flac"))return true;
    return false;
}
void AerytherAudioProcessorEditor::filesDropped(const juce::StringArray& files,int,int){
    if(files.size() && processor.loadSample(juce::File(files[0])))repaint();
}
void AerytherAudioProcessorEditor::browseForAudio(){
    chooser=std::make_unique<juce::FileChooser>("Load audio",juce::File{},"*.wav;*.aiff;*.aif;*.flac");
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [safe=juce::Component::SafePointer<AerytherAudioProcessorEditor>(this)](const juce::FileChooser& fc){
            if(safe && fc.getResult().existsAsFile()){
                safe->processor.loadSample(fc.getResult());safe->repaint();
            }
        });
}
void AerytherAudioProcessorEditor::setLoopMarker(float fraction,bool start){
    const char* id=start?"loopStart":"loopEnd";
    auto* param=processor.state.getParameter(id);
    if(!param)return;
    auto other=processor.state.getRawParameterValue(start?"loopEnd":"loopStart")->load();
    fraction=start?juce::jlimit(0.f,juce::jmax(0.f,other-0.002f),fraction):
                    juce::jlimit(juce::jmin(1.f,other+0.002f),1.f,fraction);
    param->beginChangeGesture();param->setValueNotifyingHost(param->convertTo0to1(fraction));
    param->endChangeGesture();
    repaint();
}
void AerytherAudioProcessorEditor::mouseDown(const juce::MouseEvent& e){
    int x=juce::roundToInt(float(e.getPosition().x)/(uiScale*contentScale()));
    int y=juce::roundToInt(float(e.getPosition().y)/(uiScale*contentScale()));
    if(!waveformRect.contains(x,y))return;
    float frac=float(x-waveformRect.getX())/waveformRect.getWidth();
    float a=processor.state.getRawParameterValue("loopStart")->load(),
          b=processor.state.getRawParameterValue("loopEnd")->load();
    draggingLoopStart=std::abs(frac-a)<std::abs(frac-b);
    setLoopMarker(frac,draggingLoopStart);
}
void AerytherAudioProcessorEditor::mouseDrag(const juce::MouseEvent& e){
    int x=juce::roundToInt(float(e.getPosition().x)/(uiScale*contentScale()));
    int y=juce::roundToInt(float(e.getPosition().y)/(uiScale*contentScale()));
    if(!waveformRect.expanded(60).contains(x,y))return;
    setLoopMarker(float(x-waveformRect.getX())/waveformRect.getWidth(),draggingLoopStart);
}
juce::AudioProcessorEditor* AerytherAudioProcessor::createEditor(){return new AerytherAudioProcessorEditor(*this);}
