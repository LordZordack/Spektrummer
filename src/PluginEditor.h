#pragma once

#include <JuceHeader.h>

class SpektrummerAudioProcessor;

class SpektrummerAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit SpektrummerAudioProcessorEditor (SpektrummerAudioProcessor&);
    ~SpektrummerAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpektrummerAudioProcessorEditor)
};
