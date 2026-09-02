#include <JuceHeader.h>

#include "PluginProcessor.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <memory>

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();

namespace
{
std::unique_ptr<SpektrummerAudioProcessor> makeProcessor()
{
    return std::make_unique<SpektrummerAudioProcessor>();
}

std::unique_ptr<juce::AudioProcessor> makePluginFromFactory()
{
    return std::unique_ptr<juce::AudioProcessor> { createPluginFilter() };
}

juce::AudioProcessor::BusesLayout makeOutputLayout (juce::AudioChannelSet output)
{
    juce::AudioProcessor::BusesLayout layout;
    layout.outputBuses.add (output);
    return layout;
}

void expectSilentOutput (juce::UnitTest& test, int channels, int samples)
{
    auto processor = makeProcessor();
    const auto output = channels == 1 ? juce::AudioChannelSet::mono()
                                      : juce::AudioChannelSet::stereo();
    test.expect (processor->setBusesLayout (makeOutputLayout (output)));
    processor->prepareToPlay (48000.0, 257);

    juce::AudioBuffer<float> buffer (channels, samples);
    buffer.clear();

    for (auto channel = 0; channel < channels; ++channel)
        for (auto sample = 0; sample < samples; ++sample)
            buffer.setSample (channel, sample, 0.75f);

    juce::MidiBuffer midi;
    if (samples > 0)
        midi.addEvent (juce::MidiMessage::noteOn (1, 69, 0.8f), 0);

    processor->processBlock (buffer, midi);

    for (auto channel = 0; channel < channels; ++channel)
        for (auto sample = 0; sample < samples; ++sample)
        {
            const auto value = buffer.getSample (channel, sample);
            test.expect (std::isfinite (value));
            test.expectEquals (value, 0.0f);
        }

    processor->releaseResources();
}

class PluginShellTests final : public juce::UnitTest
{
public:
    PluginShellTests() : juce::UnitTest ("Spektrummer processor shell") {}

    void runTest() override
    {
        beginTest ("Construction and synth identity");
        {
            auto processor = makePluginFromFactory();
            expect (processor != nullptr);
            expectEquals (processor->getName(), juce::String { "Spektrummer" });
            expect (processor->acceptsMidi());
            expect (! processor->producesMidi());
            expect (! processor->isMidiEffect());
            expect (processor->hasEditor());
            expectEquals (processor->getLatencySamples(), 0);
            expectEquals (processor->getTailLengthSeconds(), 0.0);
            expectEquals (processor->getNumPrograms(), 1);
            expectEquals (processor->getCurrentProgram(), 0);
        }

        beginTest ("Only output-only mono and stereo layouts are accepted");
        {
            auto processor = makeProcessor();
            expect (processor->isBusesLayoutSupported (makeOutputLayout (juce::AudioChannelSet::mono())));
            expect (processor->isBusesLayoutSupported (makeOutputLayout (juce::AudioChannelSet::stereo())));
            expect (! processor->isBusesLayoutSupported (makeOutputLayout (juce::AudioChannelSet::disabled())));
            expect (! processor->isBusesLayoutSupported (makeOutputLayout (juce::AudioChannelSet::create5point1())));

            auto inputLayout = makeOutputLayout (juce::AudioChannelSet::stereo());
            inputLayout.inputBuses.add (juce::AudioChannelSet::mono());
            expect (! processor->isBusesLayoutSupported (inputLayout));
        }

        beginTest ("Mono output is silent for zero and varying block sizes");
        for (const auto samples : std::array { 0, 1, 7, 257 })
            expectSilentOutput (*this, 1, samples);

        beginTest ("Stereo output is silent for zero and varying block sizes");
        for (const auto samples : std::array { 0, 1, 7, 257 })
            expectSilentOutput (*this, 2, samples);

        beginTest ("Empty and arbitrary state are safe");
        {
            auto processor = makeProcessor();
            juce::MemoryBlock state;
            processor->getStateInformation (state);
            expectEquals (static_cast<int> (state.getSize()), 0);

            processor->setStateInformation (nullptr, 0);
            const std::array<std::byte, 4> arbitraryState {
                std::byte { 0x10 }, std::byte { 0x20 },
                std::byte { 0x30 }, std::byte { 0x40 }
            };
            processor->setStateInformation (arbitraryState.data(),
                                            static_cast<int> (arbitraryState.size()));
        }
    }
};

PluginShellTests pluginShellTests;
}

int main (int, char**)
{
    juce::UnitTestRunner runner;
    runner.setAssertOnFailure (false);
    runner.runAllTests();

    auto failures = 0;
    for (auto index = 0; index < runner.getNumResults(); ++index)
        if (const auto* result = runner.getResult (index))
            failures += result->failures;

    return failures == 0 ? 0 : 1;
}
