#include "PluginEditor.h"
#include <cmath>

namespace {
const juce::Colour ink=juce::Colour::fromRGB(33,53,78);
const juce::Colour blue=juce::Colour::fromRGB(69,143,218);
const juce::Colour pale=juce::Colour::fromRGB(235,241,249);
const juce::Colour border=juce::Colour::fromRGB(205,215,229);
const juce::Colour white=juce::Colour::fromRGB(248,250,254);
#if defined(AERYTHER_FX)
constexpr float uiScale=0.8f;
#else
constexpr float uiScale=0.76f;
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
    g.setColour(ink);g.setFont(12);g.drawFittedText(b.getButtonText(),b.getLocalBounds(),juce::Justification::centred,1);
}
AerytherAudioProcessorEditor::AerytherAudioProcessorEditor(AerytherAudioProcessor& p)
    :juce::AudioProcessorEditor(&p),processor(p){
    setLookAndFeel(&look);setOpaque(true);
#if defined(AERYTHER_FX)
    setSize(872,240);
    // FX keeps only the processing controls; no sampler or MIDI layers.
    addSwitch("sampleFilter","FILTER ON",{55,80,140,32});
    addChoice("filterType",{54,124,195,28});
    addDial("cutoff","CUTOFF",{48,161,75,85});
    addDial("resonance","RESONANCE",{140,161,79,85});
    addDial("filterMix","MIX",{235,161,75,85});
    addSwitch("sampleDelay","DELAY ON",{340,80,140,32});
    addSwitch("pingPong","PING PONG",{486,80,125,32});
    addDial("delayTime","TIME",{334,161,75,85});
    addDial("delayFeedback","FEEDBACK",{426,161,75,85});
    addDial("delayMix","MIX",{516,161,75,85});
    addSwitch("sampleConv","CONV ON",{641,80,110,32});
    addChoice("convMode",{636,126,160,28});
    addDial("convMix","MIX",{735,172,75,80});
    addDial("reverbDecay","DECAY",{848,161,78,85});
    addDial("reverbMix","MIX",{949,161,78,85});
    addSwitch("sampleDirect","DIRECT",{640,258,140,30});
#else
    setSize(1170,684);
    browse.onClick=[this]{browseForAudio();};addAndMakeVisible(browse);
    // Top row: Sample (left), Wave (middle), Sub (right)
    auto sampleKnobs=std::array<const char*,5>{"root","sampleFine","stretch","crossfade","sampleLevel"};
    auto sampleLabels=std::array<const char*,5>{"ROOT","FINE","STRETCH","CROSSFADE","LEVEL"};
    for(int i=0;i<5;++i)addDial(sampleKnobs[i],sampleLabels[i],{57+i*100,296,70,77});
    addSwitch("loopPlay","LOOP PLAY",{54,247,112,31});
    // Keep the source BPM entry fully ABOVE the row of SAMPLE dials.
    // The value is always visible and editable (30-300 BPM).
    addDial("sampleBpm","BPM",{174,234,78,60});
    addChoice("direction",{260,248,270,28});
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
    const int startX[3]={55,575,1200}, widths[3]={110,119,79};
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
    for(int i=1;i<=4;++i){
        juce::String prefix="mod"+juce::String(i);
        int y=656+(i-1)*29;
        addChoice((prefix+"Source").toRawUTF8(),{290,y,250,26});
        addDial((prefix+"Amount").toRawUTF8(),"",{562,y-7,72,37});
        addChoice((prefix+"Target").toRawUTF8(),{780,y,335,26});
    }
    addChoice("filterType",{194,786,210,29});
    for(int i=0;i<4;++i){const char* ids[]={"cutoff","resonance","drive","filterMix"};
        const char* names[]={"CUTOFF","RESONANCE","DRIVE","MIX"};
        addDial(ids[i],names[i],{53+i*113,816,71,73});}
    for(int i=0;i<3;++i){const char* ids[]={"delayTime","delayFeedback","delayMix"};
        const char* names[]={"TIME","FEEDBACK","MIX"};
        addDial(ids[i],names[i],{523+i*105,814,72,76});}
    addSwitch("pingPong","PING-PONG",{729,780,112,30});
    addChoice("convMode",{877,798,295,31});
    addDial("convMix","MIX",{1228,808,72,82});
    addDial("reverbDecay","DECAY",{1345,812,72,80});
    addDial("reverbMix","MIX",{1440,812,72,80});
    startTimerHz(24);
#endif
}
AerytherAudioProcessorEditor::~AerytherAudioProcessorEditor(){setLookAndFeel(nullptr);}
void AerytherAudioProcessorEditor::addDial(const char* id,const char* title,juce::Rectangle<int> r){
    r=compact(r);
    auto c=std::make_unique<Dial>(); c->name=title;
    const auto parameterId=juce::String(id);
    c->slider.setSliderStyle(parameterId=="sampleBpm"
        ? juce::Slider::LinearBar : juce::Slider::RotaryHorizontalVerticalDrag);
    if (parameterId == "sampleBpm") {
        c->slider.textFromValueFunction=[](double v) {
            return juce::String(juce::jlimit(30,300,juce::roundToInt(v)));
        };
        c->slider.valueFromTextFunction=[](const juce::String& text) {
            return juce::jlimit(30.0,300.0,text.trim().getDoubleValue());
        };
        c->slider.setTextBoxStyle(juce::Slider::TextBoxBelow,false,58,17);
        c->slider.setTooltip("Source BPM: click the number and type e.g. 128");
    } else if (parameterId == "root") {
        // Expose the sampler root as a musical note, not a bare MIDI integer.
        // MIDI 60 = C3 in Ableton's middle-C naming convention.
        c->slider.textFromValueFunction=[](double v) {
            return juce::MidiMessage::getMidiNoteName(
                juce::jlimit(0,127,int(std::lround(v))),true,true,3);
        };
        c->slider.valueFromTextFunction=[](const juce::String& input) {
            const auto requested=input.trim().toUpperCase();
            for(int n=0;n<128;++n)
                if(juce::MidiMessage::getMidiNoteName(n,true,true,3).equalsIgnoreCase(requested))
                    return double(n);
            return juce::jlimit(0.0,127.0,requested.getDoubleValue());
        };
        c->slider.setTextBoxStyle(juce::Slider::TextBoxBelow,false,49,16);
    } else c->slider.setTextBoxStyle(juce::Slider::NoTextBox,false,0,0);
    c->slider.setBounds(r.removeFromTop(juce::jmax(10,r.getHeight()-14)));
    addAndMakeVisible(c->slider);
    c->label.setText(title,juce::dontSendNotification);c->label.setFont(11);
    c->label.setJustificationType(juce::Justification::centred);
    c->label.setBounds(r.getX()-8,r.getY(),r.getWidth()+16,14);
    addAndMakeVisible(c->label);
    c->attachment=std::make_unique<SlideAttach>(processor.state,id,c->slider);
    c->slider.setTooltip(juce::String(title)+": drag to adjust (double-click for default)");
    dials.push_back(std::move(c));
}
void AerytherAudioProcessorEditor::addSwitch(const char* id,const char* title,juce::Rectangle<int> r){
    r=compact(r);
    auto c=std::make_unique<Switch>();c->button.setButtonText(title);c->button.setBounds(r);
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
    g.addTransform(juce::AffineTransform::scale(uiScale));
#if defined(AERYTHER_FX)
    g.setColour(ink);g.setFont(juce::FontOptions(23.f));
    g.drawText("AERYTHER FX",juce::Rectangle<int>{0,7,getWidth(),40},juce::Justification::centred);
    g.setColour(ink);g.setFont(juce::FontOptions(11.f));
    g.drawText("Volodymyr Gnatenko",juce::Rectangle<int>{18,17,235,24},juce::Justification::centredLeft);
    g.setColour(blue);g.setFont(juce::FontOptions(13.f));
    g.drawText("BETA 0.3.0",juce::Rectangle<int>{925,15,135,25},juce::Justification::centredRight);
    panel(g,{20,56,294,235},"FILTER");
    panel(g,{321,56,290,235},"DELAY");
    panel(g,{619,56,205,235},"CONVOLVER");
    panel(g,{832,56,238,235},"REVERB");
#else
    g.setColour(ink);g.setFont(juce::FontOptions(33.f));
    g.drawFittedText("AERYTHER",{500,6,540,45},juce::Justification::centred,1);
    g.setFont(juce::FontOptions(13.f));
    g.drawFittedText("3 LAYER AMBIENT INSTRUMENT",{500,47,540,24},juce::Justification::centred,1);
    g.setColour(blue);g.setFont(juce::FontOptions(16.f));
    g.drawText("BETA 0.3.0",juce::Rectangle<int>{1360,23,140,26},juce::Justification::centredRight);
    panel(g,{35,82,535,371},"SAMPLE");
    panel(g,{576,82,609,371},"WAVE");
    panel(g,{1190,82,320,371},"SUB");
    panel(g,{35,463,400,157},"AMP ENV");
    panel(g,{441,463,390,157},"FILTER ENV");
    panel(g,{836,463,339,157},"LFO 1");
    panel(g,{1181,463,329,157},"LFO 2");
    panel(g,{35,627,1475,142},"MODULATION MATRIX");
    panel(g,{35,777,452,118},"FILTER");
    panel(g,{493,777,350,118},"DELAY");
    panel(g,{849,777,467,118},"CONVOLVER");
    panel(g,{1322,777,188,118},"REVERB");
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
    g.drawText("Volodymyr Gnatenko",juce::Rectangle<int>{57,75,240,12},juce::Justification::left);
    g.drawText(processor.sampleFileName(),juce::Rectangle<int>{342,103,206,21},juce::Justification::centredRight);
    g.drawText("SOURCE",juce::Rectangle<int>{210,638,65,16},juce::Justification::left);
    g.drawText("AMOUNT",juce::Rectangle<int>{570,638,75,16},juce::Justification::left);
    g.drawText("DESTINATION",juce::Rectangle<int>{785,638,145,16},juce::Justification::left);
#endif
}
void AerytherAudioProcessorEditor::resized(){
#if !defined(AERYTHER_FX)
    browse.setBounds(compact({58,102,102,28}));
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
    repaint(waveformRect);
}
void AerytherAudioProcessorEditor::mouseDown(const juce::MouseEvent& e){
    int x=juce::roundToInt(float(e.getPosition().x)/uiScale);
    int y=juce::roundToInt(float(e.getPosition().y)/uiScale);
    if(!waveformRect.contains(x,y))return;
    float frac=float(x-waveformRect.getX())/waveformRect.getWidth();
    float a=processor.state.getRawParameterValue("loopStart")->load(),
          b=processor.state.getRawParameterValue("loopEnd")->load();
    draggingLoopStart=std::abs(frac-a)<std::abs(frac-b);
    setLoopMarker(frac,draggingLoopStart);
}
void AerytherAudioProcessorEditor::mouseDrag(const juce::MouseEvent& e){
    int x=juce::roundToInt(float(e.getPosition().x)/uiScale);
    int y=juce::roundToInt(float(e.getPosition().y)/uiScale);
    if(!waveformRect.expanded(60).contains(x,y))return;
    setLoopMarker(float(x-waveformRect.getX())/waveformRect.getWidth(),draggingLoopStart);
}
juce::AudioProcessorEditor* AerytherAudioProcessor::createEditor(){return new AerytherAudioProcessorEditor(*this);}
