#include "PluginEditor.h"

#include "PluginProcessor.h"

SpektrummerAudioProcessorEditor::SpektrummerAudioProcessorEditor (
    SpektrummerAudioProcessor& processorToUse)
    : AudioProcessorEditor (&processorToUse)
{
    setResizable (false, false);
    setSize (560, 280);
}

void SpektrummerAudioProcessorEditor::paint (juce::Graphics& graphics)
{
    graphics.fillAll (juce::Colour { 0xff10141a });
    graphics.setColour (juce::Colour { 0xffd9f5ff });
    graphics.setFont (juce::FontOptions { 30.0f, juce::Font::bold });
    graphics.drawFittedText ("Spektrummer",
                             getLocalBounds().removeFromTop (100),
                             juce::Justification::centred,
                             1);

    graphics.setColour (juce::Colour { 0xff8eb8c7 });
    graphics.setFont (juce::FontOptions { 16.0f });
    graphics.drawFittedText ("MIDI-ready spectral synthesizer foundation",
                             getLocalBounds().reduced (32).withTrimmedTop (108),
                             juce::Justification::centred,
                             2);
}

void SpektrummerAudioProcessorEditor::resized()
{
}
