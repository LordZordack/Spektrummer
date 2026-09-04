#include "PluginProcessor.h"

#include "PluginEditor.h"
#include "dsp/SynthMidiEvent.h"

#include <array>
#include <charconv>
#include <cmath>
#include <optional>

namespace
{
constexpr auto parameterStateType = "PARAMETERS";
constexpr auto outputLevelId = "outputLevel";
constexpr auto maximumVoicesId = "maxVoices";
constexpr auto minimumOutputLevelDb = -60.0f;
constexpr auto outputGainRampSeconds = 0.02;
constexpr std::array maximumVoiceChoices { 2, 4, 8, 16 };

std::optional<double> parseFiniteNumber (const juce::var& value)
{
    const auto text = value.toString().toStdString();
    double result = 0.0;
    const auto [end, error] = std::from_chars (text.data(), text.data() + text.size(), result);

    if (error != std::errc {} || end != text.data() + text.size() || ! std::isfinite (result))
        return std::nullopt;

    return result;
}

bool isValidParameterState (const juce::ValueTree& state)
{
    if (! state.isValid() || ! state.hasType (parameterStateType) || state.getNumChildren() != 2)
        return false;

    auto sawOutputLevel = false;
    auto sawMaximumVoices = false;

    for (const auto child : state)
    {
        if (! child.hasType ("PARAM") || ! child.hasProperty ("id")
            || ! child.hasProperty ("value"))
            return false;

        const auto id = child.getProperty ("id").toString();
        const auto numericValue = parseFiniteNumber (child.getProperty ("value"));

        if (! numericValue.has_value())
            return false;

        if (id == outputLevelId)
        {
            if (sawOutputLevel || *numericValue < minimumOutputLevelDb || *numericValue > 0.0)
                return false;

            sawOutputLevel = true;
        }
        else if (id == maximumVoicesId)
        {
            if (sawMaximumVoices || *numericValue < 0.0 || *numericValue > 3.0
                || *numericValue != std::floor (*numericValue))
                return false;

            sawMaximumVoices = true;
        }
        else
        {
            return false;
        }
    }

    return sawOutputLevel && sawMaximumVoices;
}
}

SpektrummerAudioProcessor::SpektrummerAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      valueTreeState (*this, nullptr, parameterStateType, createParameterLayout())
{
    outputLevelParameter = valueTreeState.getRawParameterValue (outputLevelId);
    maximumVoicesParameter = valueTreeState.getRawParameterValue (maximumVoicesId);
    jassert (outputLevelParameter != nullptr);
    jassert (maximumVoicesParameter != nullptr);
}

void SpektrummerAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    spectralSynth.prepare (sampleRate, samplesPerBlock);
    spectralSynth.setMaximumVoices (getRequestedMaximumVoices());
    outputGain.reset (sampleRate, outputGainRampSeconds);
    outputGain.setCurrentAndTargetValue (getRequestedOutputGain());
    analyzerSampleFifo.beginNewGeneration();
}

void SpektrummerAudioProcessor::releaseResources()
{
    spectralSynth.reset();
    outputGain.setCurrentAndTargetValue (getRequestedOutputGain());
    analyzerSampleFifo.beginNewGeneration();
}

void SpektrummerAudioProcessor::reset()
{
    spectralSynth.reset();
    outputGain.reset (currentSampleRate, outputGainRampSeconds);
    outputGain.setCurrentAndTargetValue (getRequestedOutputGain());
    analyzerSampleFifo.beginNewGeneration();
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
                                               juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    spectralSynth.setMaximumVoices (getRequestedMaximumVoices());

    const auto applyMidiEvent = [this] (const auto& metadata)
    {
        const auto event = spektrummer::dsp::SynthMidiEvent::fromRawData (
            metadata.data, metadata.numBytes);

        switch (event.type)
        {
            case spektrummer::dsp::SynthMidiEvent::Type::noteOn:
                spectralSynth.noteOn (event.channel, event.note, event.velocity);
                break;
            case spektrummer::dsp::SynthMidiEvent::Type::noteOff:
                spectralSynth.noteOff (event.channel, event.note);
                break;
            case spektrummer::dsp::SynthMidiEvent::Type::allSoundOff:
                spectralSynth.allSoundOff (event.channel);
                break;
            case spektrummer::dsp::SynthMidiEvent::Type::allNotesOff:
                spectralSynth.allNotesOff (event.channel);
                break;
            case spektrummer::dsp::SynthMidiEvent::Type::none:
                break;
        }
    };

    if (buffer.getNumChannels() == 0 || buffer.getNumSamples() == 0)
    {
        for (const auto metadata : midi)
            applyMidiEvent (metadata);

        return;
    }

    auto* monoOutput = buffer.getWritePointer (0);
    auto renderedSamples = 0;

    const auto renderUntil = [&] (int endSample)
    {
        const auto clampedEnd = juce::jlimit (renderedSamples, buffer.getNumSamples(), endSample);
        spectralSynth.render (monoOutput + renderedSamples, clampedEnd - renderedSamples);
        renderedSamples = clampedEnd;
    };

    for (const auto metadata : midi)
    {
        renderUntil (metadata.samplePosition);
        applyMidiEvent (metadata);
    }

    renderUntil (buffer.getNumSamples());

    outputGain.setTargetValue (getRequestedOutputGain());

    for (auto sample = 0; sample < buffer.getNumSamples(); ++sample)
        monoOutput[sample] *= outputGain.getNextValue();

    analyzerSampleFifo.push (monoOutput, buffer.getNumSamples());

    for (auto channel = 1; channel < buffer.getNumChannels(); ++channel)
        buffer.copyFrom (channel, 0, buffer, 0, 0, buffer.getNumSamples());
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
    return spektrummer::dsp::SpectralSynthEngine::releaseSeconds
         + static_cast<double> (spektrummer::dsp::SpectralSynthEngine::fftSize)
             / std::max (1.0, currentSampleRate);
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

    if (const auto xml = valueTreeState.copyState().createXml())
        copyXmlToBinary (*xml, destinationData);
}

void SpektrummerAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (data == nullptr || sizeInBytes <= 0)
        return;

    const auto xml = getXmlFromBinary (data, sizeInBytes);

    if (xml == nullptr)
        return;

    const auto restoredState = juce::ValueTree::fromXml (*xml);

    if (isValidParameterState (restoredState))
        valueTreeState.replaceState (restoredState);
}

juce::AudioProcessorValueTreeState& SpektrummerAudioProcessor::getValueTreeState() noexcept
{
    return valueTreeState;
}

const juce::AudioProcessorValueTreeState& SpektrummerAudioProcessor::getValueTreeState() const noexcept
{
    return valueTreeState;
}

spektrummer::analyzer::AnalyzerSampleFifo&
SpektrummerAudioProcessor::getAnalyzerSampleFifo() noexcept
{
    return analyzerSampleFifo;
}

juce::AudioProcessorValueTreeState::ParameterLayout
SpektrummerAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { outputLevelId, 1 },
        "Output Level",
        juce::NormalisableRange<float> { minimumOutputLevelDb, 0.0f, 0.1f },
        -12.0f,
        juce::AudioParameterFloatAttributes {}.withLabel ("dB")));
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { maximumVoicesId, 1 },
        "Maximum Voices",
        juce::StringArray { "2", "4", "8", "16" },
        2));
    return layout;
}

float SpektrummerAudioProcessor::getRequestedOutputGain() const noexcept
{
    const auto decibels = outputLevelParameter != nullptr
                            ? outputLevelParameter->load (std::memory_order_relaxed)
                            : -12.0f;
    return decibels <= minimumOutputLevelDb
             ? 0.0f
             : juce::Decibels::decibelsToGain (decibels);
}

int SpektrummerAudioProcessor::getRequestedMaximumVoices() const noexcept
{
    const auto choice = maximumVoicesParameter != nullptr
                          ? static_cast<int> (std::lround (
                                maximumVoicesParameter->load (std::memory_order_relaxed)))
                          : 2;
    return maximumVoiceChoices[static_cast<std::size_t> (
        juce::jlimit (0, static_cast<int> (maximumVoiceChoices.size()) - 1, choice))];
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SpektrummerAudioProcessor();
}
