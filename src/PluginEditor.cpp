#include "PluginEditor.h"

#include "PluginProcessor.h"

SpektrummerAudioProcessorEditor::SpektrummerAudioProcessorEditor (
    SpektrummerAudioProcessor& processorToUse)
    : AudioProcessorEditor (&processorToUse),
      processor (processorToUse),
      spectrum (processorToUse.getAnalyzerSampleFifo(), processorToUse)
{
    outputLevelSlider.setComponentID ("outputLevel");
    outputLevelSlider.setName ("Output level");
    outputLevelSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    outputLevelSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 72, 20);
    outputLevelSlider.setTextValueSuffix (" dB");
    outputLevelLabel.setText ("Output Level", juce::dontSendNotification);
    outputLevelLabel.setJustificationType (juce::Justification::centred);

    maximumVoicesBox.setComponentID ("maxVoices");
    maximumVoicesBox.setName ("Maximum voices");
    maximumVoicesBox.addItemList ({ "2", "4", "8", "16" }, 1);
    maximumVoicesLabel.setText ("Maximum Voices", juce::dontSendNotification);
    maximumVoicesLabel.setJustificationType (juce::Justification::centred);

    addAndMakeVisible (outputLevelSlider);
    addAndMakeVisible (maximumVoicesBox);
    addAndMakeVisible (outputLevelLabel);
    addAndMakeVisible (maximumVoicesLabel);
    addAndMakeVisible (spectrum);

    outputLevelAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.getValueTreeState(), "outputLevel", outputLevelSlider);
    maximumVoicesAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        processor.getValueTreeState(), "maxVoices", maximumVoicesBox);

    setResizable (false, false);
    setSize (640, 420);
}

void SpektrummerAudioProcessorEditor::paint (juce::Graphics& graphics)
{
    graphics.fillAll (juce::Colour { 0xff10141a });
    graphics.setColour (juce::Colour { 0xffd9f5ff });
    graphics.setFont (juce::FontOptions { 26.0f, juce::Font::bold });
    graphics.drawFittedText ("Spektrummer",
                             getLocalBounds().removeFromTop (48),
                             juce::Justification::centred,
                             1);
}

void SpektrummerAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds().reduced (16);
    bounds.removeFromTop (42);
    spectrum.setBounds (bounds.removeFromTop (260));
    bounds.removeFromTop (8);

    auto outputArea = bounds.removeFromLeft (280).reduced (44, 0);
    outputLevelLabel.setBounds (outputArea.removeFromTop (22));
    outputLevelSlider.setBounds (outputArea);

    auto voicesArea = bounds.reduced (42, 12);
    maximumVoicesLabel.setBounds (voicesArea.removeFromTop (24));
    maximumVoicesBox.setBounds (voicesArea.removeFromTop (28));
}
