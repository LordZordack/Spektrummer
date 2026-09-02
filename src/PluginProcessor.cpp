#include "PluginProcessor.h"

#include "PluginEditor.h"

SpektrummerAudioProcessor::SpektrummerAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
}

void SpektrummerAudioProcessor::prepareToPlay (double, int)
{
}

void SpektrummerAudioProcessor::releaseResources()
{
}

bool SpektrummerAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (! layouts.inputBuses.isEmpty() || layouts.outputBuses.size() != 1)
        return false;

    const auto mainOutput = layouts.getMainOutputChannelSet();
    return mainOutput == juce::AudioChannelSet::mono()
        || mainOutput == juce::AudioChannelSet::stereo();
}

void SpektrummerAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                               juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();
}

juce::AudioProcessorEditor* SpektrummerAudioProcessor::createEditor()
{
    return new SpektrummerAudioProcessorEditor (*this);
}

bool SpektrummerAudioProcessor::hasEditor() const
{
    return true;
}

const juce::String SpektrummerAudioProcessor::getName() const
{
    return "Spektrummer";
}

bool SpektrummerAudioProcessor::acceptsMidi() const
{
    return true;
}

bool SpektrummerAudioProcessor::producesMidi() const
{
    return false;
}

bool SpektrummerAudioProcessor::isMidiEffect() const
{
    return false;
}

double SpektrummerAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int SpektrummerAudioProcessor::getNumPrograms()
{
    return 1;
}

int SpektrummerAudioProcessor::getCurrentProgram()
{
    return 0;
}

void SpektrummerAudioProcessor::setCurrentProgram (int)
{
}

const juce::String SpektrummerAudioProcessor::getProgramName (int)
{
    return {};
}

void SpektrummerAudioProcessor::changeProgramName (int, const juce::String&)
{
}

void SpektrummerAudioProcessor::getStateInformation (juce::MemoryBlock& destinationData)
{
    destinationData.reset();
}

void SpektrummerAudioProcessor::setStateInformation (const void*, int)
{
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SpektrummerAudioProcessor();
}
