#pragma once
#include "PluginProcessor.h"
#include <memory>
#include <vector>

class AerytherAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                           public juce::FileDragAndDropTarget,
                                           private juce::Timer {
public:
    explicit AerytherAudioProcessorEditor(AerytherAudioProcessor&);
    ~AerytherAudioProcessorEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    bool isInterestedInFileDrag(const juce::StringArray&) override;
    void filesDropped(const juce::StringArray&,int,int) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
private:
    struct LightLook final : juce::LookAndFeel_V4 {
        LightLook();
        void drawRotarySlider(juce::Graphics&,int,int,int,int,float,float,float,juce::Slider&) override;
        void drawLinearSlider(juce::Graphics&,int,int,int,int,float,float,float,
                              const juce::Slider::SliderStyle,juce::Slider&) override;
        void drawButtonBackground(juce::Graphics&,juce::Button&,const juce::Colour&,bool,bool) override;
        void drawToggleButton(juce::Graphics&,juce::ToggleButton&,bool,bool) override;
    } look;
    using SlideAttach=juce::AudioProcessorValueTreeState::SliderAttachment;
    using BoolAttach=juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ChoiceAttach=juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    struct Dial {
        juce::Slider slider;
        juce::Label label;
        juce::Label valueLabel; // persistent, editable SAMPLE BPM/ROOT readout
        juce::String parameterID;
        std::unique_ptr<SlideAttach> attachment;
        juce::String name;
    };
    struct Switch {
        juce::ToggleButton button;
        std::unique_ptr<BoolAttach> attachment;
    };
    struct Choice {
        juce::ComboBox box;
        std::unique_ptr<ChoiceAttach> attachment;
    };
    AerytherAudioProcessor& processor;
    std::vector<std::unique_ptr<Dial>> dials;
    std::vector<std::unique_ptr<Switch>> switches;
    std::vector<std::unique_ptr<Choice>> choices;
    juce::TextButton browse{"LOAD AUDIO"};
    std::unique_ptr<juce::FileChooser> chooser;
    juce::Rectangle<int> waveformRect;
    std::vector<float> cachedPeaks;
    juce::String cachedSampleFile;
    bool draggingLoopStart=false;
    void addDial(const char* id,const char* title,juce::Rectangle<int> bounds);
    void addSwitch(const char* id,const char* title,juce::Rectangle<int> bounds);
    void addChoice(const char* id,juce::Rectangle<int> bounds);
    void addLabel(juce::Graphics&,const juce::String&,juce::Rectangle<float> r,int size=15) const;
    void panel(juce::Graphics&,juce::Rectangle<int>,const char*) const;
    void paintWaveform(juce::Graphics&);
    void browseForAudio();
    void setLoopMarker(float frac,bool start);
    void refreshParameterReadouts();
    void timerCallback() override;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AerytherAudioProcessorEditor)
};
