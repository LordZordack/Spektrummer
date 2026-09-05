#pragma once

#include <JuceHeader.h>

#include "ui/SpectrumComponent.h"
#include "ui/EnvelopeComponent.h"

class SpektrummerAudioProcessor;

class SpektrummerAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit SpektrummerAudioProcessorEditor (SpektrummerAudioProcessor&);
    ~SpektrummerAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    SpektrummerAudioProcessor& processor;
    juce::Slider outputLevelSlider;
    juce::Slider attackSlider;
    juce::Slider decaySlider;
    juce::Slider sustainSlider;
    juce::Slider releaseSlider;
    juce::ComboBox maximumVoicesBox;
    juce::Label outputLevelLabel;
    juce::Label attackLabel;
    juce::Label decayLabel;
    juce::Label sustainLabel;
    juce::Label releaseLabel;
    juce::Label maximumVoicesLabel;
    spektrummer::ui::SpectrumComponent spectrum;
    spektrummer::ui::EnvelopeComponent envelopePreview;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> outputLevelAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attackAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> decayAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sustainAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> releaseAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> maximumVoicesAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpektrummerAudioProcessorEditor)
};
