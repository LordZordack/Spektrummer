#include "PluginEditor.h"

#include "PluginProcessor.h"

SpektrummerAudioProcessorEditor::SpektrummerAudioProcessorEditor (
    SpektrummerAudioProcessor& processorToUse)
    : AudioProcessorEditor (&processorToUse),
      processor (processorToUse),
      spectrum (processorToUse.getAnalyzerSampleFifo(), processorToUse),
      envelopePreview (processorToUse.getValueTreeState())
{
    const auto configureKnob = [] (juce::Slider& slider,
                                   juce::Label& label,
                                   const char* id,
                                   const char* name,
                                   const char* suffix)
    {
        slider.setComponentID (id);
        slider.setName (name);
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 72, 20);
        slider.setTextValueSuffix (suffix);
        label.setText (name, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centred);
    };

    configureKnob (outputLevelSlider, outputLevelLabel, "outputLevel", "Output Level", " dB");
    configureKnob (attackSlider, attackLabel, "attack", "Attack", " s");
    configureKnob (decaySlider, decayLabel, "decay", "Decay", " s");
    configureKnob (sustainSlider, sustainLabel, "sustain", "Sustain", "");
    configureKnob (releaseSlider, releaseLabel, "release", "Release", " s");

    maximumVoicesBox.setComponentID ("maxVoices");
    maximumVoicesBox.setName ("Maximum voices");
    maximumVoicesBox.addItemList ({ "2", "4", "8", "16" }, 1);
    maximumVoicesLabel.setText ("Maximum Voices", juce::dontSendNotification);
    maximumVoicesLabel.setJustificationType (juce::Justification::centred);

    addAndMakeVisible (outputLevelSlider);
    addAndMakeVisible (attackSlider);
    addAndMakeVisible (decaySlider);
    addAndMakeVisible (sustainSlider);
    addAndMakeVisible (releaseSlider);
    addAndMakeVisible (maximumVoicesBox);
    addAndMakeVisible (outputLevelLabel);
    addAndMakeVisible (attackLabel);
    addAndMakeVisible (decayLabel);
    addAndMakeVisible (sustainLabel);
    addAndMakeVisible (releaseLabel);
    addAndMakeVisible (maximumVoicesLabel);
    addAndMakeVisible (spectrum);
    addAndMakeVisible (envelopePreview);

    outputLevelAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.getValueTreeState(), "outputLevel", outputLevelSlider);
    attackAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.getValueTreeState(), "attack", attackSlider);
    decayAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.getValueTreeState(), "decay", decaySlider);
    sustainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.getValueTreeState(), "sustain", sustainSlider);
    releaseAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.getValueTreeState(), "release", releaseSlider);
    maximumVoicesAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        processor.getValueTreeState(), "maxVoices", maximumVoicesBox);

    setResizable (false, false);
    setSize (900, 560);
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
    spectrum.setBounds (bounds.removeFromTop (280));
    bounds.removeFromTop (8);

    envelopePreview.setBounds (bounds.removeFromLeft (210));
    bounds.removeFromLeft (8);

    auto voicesArea = bounds.removeFromRight (132).reduced (10, 18);
    maximumVoicesLabel.setBounds (voicesArea.removeFromTop (24));
    maximumVoicesBox.setBounds (voicesArea.removeFromTop (28));

    std::array<juce::Slider*, 5> sliders {
        &outputLevelSlider, &attackSlider, &decaySlider, &sustainSlider, &releaseSlider
    };
    std::array<juce::Label*, 5> labels {
        &outputLevelLabel, &attackLabel, &decayLabel, &sustainLabel, &releaseLabel
    };
    const auto knobWidth = bounds.getWidth() / static_cast<int> (sliders.size());

    for (auto index = std::size_t {}; index < sliders.size(); ++index)
    {
        auto knobArea = bounds.removeFromLeft (knobWidth).reduced (4, 0);
        labels[index]->setBounds (knobArea.removeFromTop (22));
        sliders[index]->setBounds (knobArea);
    }
}
