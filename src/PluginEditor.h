#pragma once

#include <JuceHeader.h>

#include "ui/SpectrumComponent.h"

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
    juce::ComboBox maximumVoicesBox;
    juce::Label outputLevelLabel;
    juce::Label maximumVoicesLabel;
    spektrummer::ui::SpectrumComponent spectrum;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> outputLevelAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> maximumVoicesAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpektrummerAudioProcessorEditor)
};
