#include <JuceHeader.h>

#include "PluginProcessor.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

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

juce::RangedAudioParameter* findParameter (juce::AudioProcessor& processor,
                                           const juce::String& parameterId)
{
    for (auto* parameter : processor.getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (parameter);
            ranged != nullptr && ranged->paramID == parameterId)
            return ranged;

    return nullptr;
}

void setParameterValue (juce::AudioProcessor& processor,
                        const juce::String& parameterId,
                        float denormalisedValue)
{
    if (auto* parameter = findParameter (processor, parameterId))
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (denormalisedValue));
}

float renderPeakAtOutputLevel (juce::UnitTest& test, float outputLevelDb)
{
    auto processor = makeProcessor();
    setParameterValue (*processor, "outputLevel", outputLevelDb);
    processor->prepareToPlay (48000.0, 257);
    juce::AudioBuffer<float> buffer (1, 8192);
    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (1, 69, 1.0f), 0);
    processor->processBlock (buffer, midi);

    auto peak = 0.0f;
    for (auto sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        const auto value = buffer.getSample (0, sample);
        test.expect (std::isfinite (value));
        peak = std::max (peak, std::abs (value));
    }

    return peak;
}

std::vector<float> renderProcessorSequence (SpektrummerAudioProcessor& processor,
                                            int channels,
                                            int totalSamples,
                                            const std::vector<int>& partitions)
{
    const auto output = channels == 1 ? juce::AudioChannelSet::mono()
                                      : juce::AudioChannelSet::stereo();
    processor.setBusesLayout (makeOutputLayout (output));
    processor.prepareToPlay (48000.0, 1024);
    std::vector<float> rendered (static_cast<std::size_t> (channels * totalSamples));
    auto globalPosition = 0;
    auto partitionIndex = std::size_t {};

    while (globalPosition < totalSamples)
    {
        const auto blockSize = std::min (partitions[partitionIndex % partitions.size()],
                                         totalSamples - globalPosition);
        juce::AudioBuffer<float> block (channels, blockSize);
        juce::MidiBuffer midi;

        if (globalPosition == 0)
            midi.addEvent (juce::MidiMessage::noteOn (1, 69, 0.8f), 0);

        constexpr auto noteOffPosition = 6000;
        if (globalPosition <= noteOffPosition
            && noteOffPosition < globalPosition + blockSize)
            midi.addEvent (juce::MidiMessage::noteOff (1, 69), noteOffPosition - globalPosition);

        processor.processBlock (block, midi);

        for (auto channel = 0; channel < channels; ++channel)
            for (auto sample = 0; sample < blockSize; ++sample)
                rendered[static_cast<std::size_t> (channel * totalSamples + globalPosition + sample)]
                    = block.getSample (channel, sample);

        globalPosition += blockSize;
        ++partitionIndex;
    }

    return rendered;
}

template <typename Processor>
void expectAnalyzerPublication (juce::UnitTest& test, Processor& processor)
{
    if constexpr (requires (Processor& candidate) { candidate.getAnalyzerSampleFifo(); })
    {
        const auto generationBeforePrepare = processor.getAnalyzerSampleFifo().getGeneration();
        processor.prepareToPlay (48000.0, 257);
        test.expect (processor.getAnalyzerSampleFifo().getGeneration() > generationBeforePrepare);

        juce::AudioBuffer<float> buffer (1, 8192);
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 69, 1.0f), 0);
        processor.processBlock (buffer, midi);
        test.expectEquals (processor.getAnalyzerSampleFifo().getNumReady(), buffer.getNumSamples());

        std::vector<float> published (static_cast<std::size_t> (buffer.getNumSamples()));
        const auto popped = processor.getAnalyzerSampleFifo().pop (
            published.data(), static_cast<int> (published.size()));
        test.expectEquals (popped, buffer.getNumSamples());

        for (auto sample = 0; sample < buffer.getNumSamples(); ++sample)
            test.expectEquals (published[static_cast<std::size_t> (sample)],
                               buffer.getSample (0, sample));

        const auto generationBeforeReset = processor.getAnalyzerSampleFifo().getGeneration();
        processor.reset();
        test.expect (processor.getAnalyzerSampleFifo().getGeneration() > generationBeforeReset);
    }
    else
    {
        test.expect (false, "Processor does not expose analyzer sample transport");
    }
}

void expectFiniteOutputForBlockSize (juce::UnitTest& test, int channels, int samples)
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
        }

    processor->releaseResources();
}

void expectMidiProducesAudio (juce::UnitTest& test, int channels)
{
    auto processor = makeProcessor();
    const auto output = channels == 1 ? juce::AudioChannelSet::mono()
                                      : juce::AudioChannelSet::stereo();
    test.expect (processor->setBusesLayout (makeOutputLayout (output)));
    processor->prepareToPlay (48000.0, 257);

    juce::AudioBuffer<float> buffer (channels, 8192);
    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (1, 69, 0.8f), 0);
    processor->processBlock (buffer, midi);

    auto peak = 0.0f;

    for (auto channel = 0; channel < channels; ++channel)
        for (auto sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            const auto value = buffer.getSample (channel, sample);
            test.expect (std::isfinite (value));
            peak = std::max (peak, std::abs (value));
        }

    test.expectGreaterThan (peak, 0.0001f);
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
            expectGreaterOrEqual (processor->getTailLengthSeconds(), 0.08);
            processor->prepareToPlay (22050.0, 128);
            expectGreaterOrEqual (processor->getTailLengthSeconds(), 0.172);
            expectEquals (processor->getNumPrograms(), 1);
            expectEquals (processor->getCurrentProgram(), 0);
        }

        beginTest ("Editor construction");
        {
            auto processor = makeProcessor();
            std::unique_ptr<juce::AudioProcessorEditor> editor { processor->createEditor() };
            expect (editor != nullptr);

            if (editor != nullptr)
            {
                expect (editor->getAudioProcessor() == processor.get());
                expectEquals (editor->getWidth(), 900);
                expectEquals (editor->getHeight(), 560);
                expect (editor->findChildWithID ("outputLevel") != nullptr);
                expect (editor->findChildWithID ("maxVoices") != nullptr);
                expect (editor->findChildWithID ("spectrum") != nullptr);
                expect (editor->findChildWithID ("attack") != nullptr);
                expect (editor->findChildWithID ("decay") != nullptr);
                expect (editor->findChildWithID ("sustain") != nullptr);
                expect (editor->findChildWithID ("release") != nullptr);
                expect (editor->findChildWithID ("envelopePreview") != nullptr);
            }
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

        beginTest ("MIDI produces audible finite mono and stereo output");
        expectMidiProducesAudio (*this, 1);
        expectMidiProducesAudio (*this, 2);

        beginTest ("Output level and maximum voices have stable parameter contracts");
        {
            auto processor = makeProcessor();
            auto* outputLevel = findParameter (*processor, "outputLevel");
            auto* maximumVoices = findParameter (*processor, "maxVoices");
            expect (outputLevel != nullptr);
            expect (maximumVoices != nullptr);

            if (outputLevel != nullptr)
            {
                expectWithinAbsoluteError (outputLevel->getNormalisableRange().start, -60.0f, 0.001f);
                expectWithinAbsoluteError (outputLevel->getNormalisableRange().end, 0.0f, 0.001f);
                expectWithinAbsoluteError (outputLevel->convertFrom0to1 (outputLevel->getDefaultValue()),
                                           -12.0f,
                                           0.001f);
            }

            if (auto* choices = dynamic_cast<juce::AudioParameterChoice*> (maximumVoices))
            {
                expectEquals (choices->choices.size(), 4);
                expectEquals (choices->choices[0], juce::String { "2" });
                expectEquals (choices->choices[1], juce::String { "4" });
                expectEquals (choices->choices[2], juce::String { "8" });
                expectEquals (choices->choices[3], juce::String { "16" });
                expectEquals (choices->getIndex(), 2);
            }
            else
            {
                expect (false, "maxVoices must be an AudioParameterChoice");
            }
        }

        beginTest ("ADSR parameters have stable finite contracts");
        {
            auto processor = makeProcessor();
            const auto expectFloatParameter = [&] (const char* id,
                                                   float minimum,
                                                   float maximum,
                                                   float defaultValue)
            {
                auto* parameter = findParameter (*processor, id);
                expect (parameter != nullptr);

                if (parameter != nullptr)
                {
                    expectWithinAbsoluteError (parameter->getNormalisableRange().start,
                                               minimum,
                                               0.001f);
                    expectWithinAbsoluteError (parameter->getNormalisableRange().end,
                                               maximum,
                                               0.001f);
                    expectWithinAbsoluteError (parameter->convertFrom0to1 (
                                                   parameter->getDefaultValue()),
                                               defaultValue,
                                               0.001f);
                    expect (std::isfinite (parameter->getDefaultValue()));
                }
            };

            expectFloatParameter ("attack", 0.0f, 5.0f, 0.01f);
            expectFloatParameter ("decay", 0.0f, 5.0f, 0.10f);
            expectFloatParameter ("sustain", 0.0f, 1.0f, 0.80f);
            expectFloatParameter ("release", 0.0f, 10.0f, 0.08f);
        }

        beginTest ("Output-level endpoints are exact mute and finite full level");
        expectEquals (renderPeakAtOutputLevel (*this, -60.0f), 0.0f);
        expectGreaterThan (renderPeakAtOutputLevel (*this, 0.0f), 0.001f);

        beginTest ("Output-level automation reaches exact mute after its smoothing ramp");
        {
            auto processor = makeProcessor();
            setParameterValue (*processor, "outputLevel", 0.0f);
            processor->prepareToPlay (48000.0, 257);

            juce::AudioBuffer<float> warmup (1, 4096);
            juce::MidiBuffer noteOn;
            noteOn.addEvent (juce::MidiMessage::noteOn (1, 69, 1.0f), 0);
            processor->processBlock (warmup, noteOn);

            setParameterValue (*processor, "outputLevel", -60.0f);
            juce::AudioBuffer<float> ramp (1, 2048);
            juce::MidiBuffer noMidi;
            processor->processBlock (ramp, noMidi);

            expectGreaterThan (ramp.getMagnitude (0, 0, 512), 0.0001f);
            expectEquals (ramp.getMagnitude (0, 1536, 512), 0.0f);
        }

        beginTest ("Post-gain audio is published through bounded analyzer transport");
        {
            auto processor = makeProcessor();
            expectAnalyzerPublication (*this, *processor);
        }

        beginTest ("MIDI sample offsets do not produce pre-event audio");
        {
            auto processor = makeProcessor();
            processor->prepareToPlay (48000.0, 1024);
            juce::AudioBuffer<float> buffer (2, 4096);
            juce::MidiBuffer midi;
            midi.addEvent (juce::MidiMessage::noteOn (1, 69, 1.0f), 137);
            processor->processBlock (buffer, midi);

            for (auto sample = 0; sample < 137; ++sample)
                expectEquals (buffer.getSample (0, sample), 0.0f);

            expectGreaterThan (buffer.getMagnitude (0, 137, buffer.getNumSamples() - 137),
                               0.0001f);
        }

        beginTest ("Zero-sample callbacks still apply MIDI panic events");
        {
            auto processor = makeProcessor();
            processor->prepareToPlay (48000.0, 257);

            juce::AudioBuffer<float> sounding (1, 4096);
            juce::MidiBuffer noteOn;
            noteOn.addEvent (juce::MidiMessage::noteOn (1, 69, 1.0f), 0);
            processor->processBlock (sounding, noteOn);
            expectGreaterThan (sounding.getMagnitude (0, 0, sounding.getNumSamples()), 0.0001f);

            juce::AudioBuffer<float> empty (1, 0);
            juce::MidiBuffer panic;
            panic.addEvent (juce::MidiMessage::controllerEvent (1, 120, 0), 0);
            processor->processBlock (empty, panic);

            juce::AudioBuffer<float> afterPanic (1, 2048);
            juce::MidiBuffer noMidi;
            processor->processBlock (afterPanic, noMidi);
            expectEquals (afterPanic.getMagnitude (0, 0, afterPanic.getNumSamples()), 0.0f);
        }

        beginTest ("Mono and stereo synthesis match and stereo channels remain equal");
        {
            auto monoProcessor = makeProcessor();
            auto stereoProcessor = makeProcessor();
            const auto mono = renderProcessorSequence (*monoProcessor, 1, 10000, { 257 });
            const auto stereo = renderProcessorSequence (*stereoProcessor, 2, 10000, { 257 });

            for (auto sample = 0; sample < 10000; ++sample)
            {
                const auto index = static_cast<std::size_t> (sample);
                expectWithinAbsoluteError (mono[index], stereo[index], 1.0e-7f);
                expectWithinAbsoluteError (stereo[index], stereo[index + 10000], 1.0e-7f);
            }
        }

        beginTest ("Processor output is independent of host block partitioning");
        {
            auto contiguousProcessor = makeProcessor();
            auto partitionedProcessor = makeProcessor();
            const auto contiguous = renderProcessorSequence (*contiguousProcessor, 1, 10000, { 10000 });
            const auto partitioned = renderProcessorSequence (*partitionedProcessor,
                                                               1,
                                                               10000,
                                                               { 1, 7, 64, 257, 1024 });

            for (auto index = std::size_t {}; index < contiguous.size(); ++index)
                expectWithinAbsoluteError (contiguous[index], partitioned[index], 1.0e-7f);
        }

        beginTest ("Processor reset matches a fresh prepared instance");
        {
            auto reused = makeProcessor();
            renderProcessorSequence (*reused, 1, 4096, { 257 });
            reused->reset();

            juce::AudioBuffer<float> resetBuffer (1, 8192);
            juce::MidiBuffer resetMidi;
            resetMidi.addEvent (juce::MidiMessage::noteOn (1, 69, 0.8f), 0);
            resetMidi.addEvent (juce::MidiMessage::noteOff (1, 69), 6000);
            reused->processBlock (resetBuffer, resetMidi);

            auto fresh = makeProcessor();
            const auto expected = renderProcessorSequence (*fresh, 1, 8192, { 8192 });

            auto maximumError = 0.0f;
            for (auto sample = 0; sample < resetBuffer.getNumSamples(); ++sample)
                maximumError = std::max (maximumError,
                                         std::abs (resetBuffer.getSample (0, sample)
                                                   - expected[static_cast<std::size_t> (sample)]));

            expectLessOrEqual (maximumError, 1.0e-7f);
        }

        beginTest ("Mono output is finite for zero and varying block sizes");
        for (const auto samples : std::array { 0, 1, 7, 257 })
            expectFiniteOutputForBlockSize (*this, 1, samples);

        beginTest ("Stereo output is finite for zero and varying block sizes");
        for (const auto samples : std::array { 0, 1, 7, 257 })
            expectFiniteOutputForBlockSize (*this, 2, samples);

        beginTest ("Parameter state round-trips and arbitrary state is ignored");
        {
            auto processor = makeProcessor();
            setParameterValue (*processor, "outputLevel", -6.0f);
            setParameterValue (*processor, "maxVoices", 3.0f);
            setParameterValue (*processor, "attack", 0.25f);
            setParameterValue (*processor, "decay", 0.35f);
            setParameterValue (*processor, "sustain", 0.45f);
            setParameterValue (*processor, "release", 1.25f);
            juce::MemoryBlock state;
            processor->getStateInformation (state);
            expectGreaterThan (static_cast<int> (state.getSize()), 0);

            auto restored = makeProcessor();
            restored->setStateInformation (state.getData(), static_cast<int> (state.getSize()));
            auto* restoredOutput = findParameter (*restored, "outputLevel");
            auto* restoredVoices = findParameter (*restored, "maxVoices");
            auto* restoredAttack = findParameter (*restored, "attack");
            auto* restoredDecay = findParameter (*restored, "decay");
            auto* restoredSustain = findParameter (*restored, "sustain");
            auto* restoredRelease = findParameter (*restored, "release");
            expect (restoredOutput != nullptr);
            expect (restoredVoices != nullptr);
            expect (restoredAttack != nullptr);
            expect (restoredDecay != nullptr);
            expect (restoredSustain != nullptr);
            expect (restoredRelease != nullptr);

            if (restoredOutput != nullptr)
                expectWithinAbsoluteError (restoredOutput->convertFrom0to1 (restoredOutput->getValue()),
                                           -6.0f,
                                           0.001f);

            if (restoredVoices != nullptr)
                expectWithinAbsoluteError (restoredVoices->convertFrom0to1 (restoredVoices->getValue()),
                                           3.0f,
                                           0.001f);

            if (restoredAttack != nullptr)
                expectWithinAbsoluteError (restoredAttack->convertFrom0to1 (restoredAttack->getValue()),
                                           0.25f,
                                           0.001f);

            if (restoredDecay != nullptr)
                expectWithinAbsoluteError (restoredDecay->convertFrom0to1 (restoredDecay->getValue()),
                                           0.35f,
                                           0.001f);

            if (restoredSustain != nullptr)
                expectWithinAbsoluteError (restoredSustain->convertFrom0to1 (restoredSustain->getValue()),
                                           0.45f,
                                           0.001f);

            if (restoredRelease != nullptr)
                expectWithinAbsoluteError (restoredRelease->convertFrom0to1 (restoredRelease->getValue()),
                                           1.25f,
                                           0.001f);

            processor->setStateInformation (nullptr, 0);
            const std::array<std::byte, 4> arbitraryState {
                std::byte { 0x10 }, std::byte { 0x20 },
                std::byte { 0x30 }, std::byte { 0x40 }
            };
            processor->setStateInformation (arbitraryState.data(),
                                            static_cast<int> (arbitraryState.size()));

            if (auto* outputAfterInvalidState = findParameter (*processor, "outputLevel"))
                expectWithinAbsoluteError (outputAfterInvalidState->convertFrom0to1 (
                                               outputAfterInvalidState->getValue()),
                                           -6.0f,
                                           0.001f);

            processor->prepareToPlay (48000.0, 257);
            const auto validXml = juce::AudioProcessor::getXmlFromBinary (
                state.getData(), static_cast<int> (state.getSize()));
            expect (validXml != nullptr);

            if (validXml != nullptr)
            {
                const auto expectRejected = [&] (juce::ValueTree malformed)
                {
                    juce::MemoryBlock malformedState;
                    if (const auto xml = malformed.createXml())
                        juce::AudioProcessor::copyXmlToBinary (*xml, malformedState);

                    processor->setStateInformation (malformedState.getData(),
                                                    static_cast<int> (malformedState.getSize()));
                    for (const auto& expected : std::array {
                             std::pair { "outputLevel", -6.0f },
                             std::pair { "maxVoices", 3.0f },
                             std::pair { "attack", 0.25f },
                             std::pair { "decay", 0.35f },
                             std::pair { "sustain", 0.45f },
                             std::pair { "release", 1.25f } })
                    {
                        if (auto* parameter = findParameter (*processor, expected.first))
                            expectWithinAbsoluteError (parameter->convertFrom0to1 (
                                                           parameter->getValue()),
                                                       expected.second,
                                                       0.001f);
                    }
                };

                auto nonFinite = juce::ValueTree::fromXml (*validXml);
                nonFinite.getChildWithProperty ("id", "outputLevel")
                         .setProperty ("value", std::numeric_limits<double>::quiet_NaN(), nullptr);
                expectRejected (nonFinite);

                auto nonFiniteAttack = juce::ValueTree::fromXml (*validXml);
                nonFiniteAttack.getChildWithProperty ("id", "attack")
                               .setProperty ("value", std::numeric_limits<double>::infinity(), nullptr);
                expectRejected (nonFiniteAttack);

                auto missing = juce::ValueTree::fromXml (*validXml);
                missing.removeChild (missing.getChildWithProperty ("id", "maxVoices"), nullptr);
                expectRejected (missing);

                auto duplicate = juce::ValueTree::fromXml (*validXml);
                duplicate.appendChild (duplicate.getChildWithProperty ("id", "outputLevel").createCopy(),
                                       nullptr);
                expectRejected (duplicate);

                auto outOfRange = juce::ValueTree::fromXml (*validXml);
                outOfRange.getChildWithProperty ("id", "outputLevel")
                          .setProperty ("value", 12.0, nullptr);
                expectRejected (outOfRange);

                auto invalidSustain = juce::ValueTree::fromXml (*validXml);
                invalidSustain.getChildWithProperty ("id", "sustain")
                              .setProperty ("value", 2.0, nullptr);
                expectRejected (invalidSustain);

                juce::AudioBuffer<float> finiteBuffer (1, 4096);
                juce::MidiBuffer finiteMidi;
                finiteMidi.addEvent (juce::MidiMessage::noteOn (1, 69, 1.0f), 0);
                processor->processBlock (finiteBuffer, finiteMidi);
                for (auto sample = 0; sample < finiteBuffer.getNumSamples(); ++sample)
                    expect (std::isfinite (finiteBuffer.getSample (0, sample)));
            }
        }

        beginTest ("Valid state restoration while prepared retargets audio safely");
        {
            auto donor = makeProcessor();
            setParameterValue (*donor, "outputLevel", -60.0f);
            setParameterValue (*donor, "maxVoices", 0.0f);
            juce::MemoryBlock mutedState;
            donor->getStateInformation (mutedState);

            auto processor = makeProcessor();
            setParameterValue (*processor, "outputLevel", 0.0f);
            processor->prepareToPlay (48000.0, 257);
            juce::AudioBuffer<float> warmup (1, 4096);
            juce::MidiBuffer noteOn;
            noteOn.addEvent (juce::MidiMessage::noteOn (1, 69, 1.0f), 0);
            processor->processBlock (warmup, noteOn);

            processor->setStateInformation (mutedState.getData(),
                                            static_cast<int> (mutedState.getSize()));
            juce::AudioBuffer<float> afterRestore (1, 2048);
            juce::MidiBuffer noMidi;
            processor->processBlock (afterRestore, noMidi);

            expectGreaterThan (afterRestore.getMagnitude (0, 0, 512), 0.0001f);
            expectEquals (afterRestore.getMagnitude (0, 1536, 512), 0.0f);
            if (auto* voices = findParameter (*processor, "maxVoices"))
                expectWithinAbsoluteError (voices->convertFrom0to1 (voices->getValue()),
                                           0.0f,
                                           0.001f);
        }
    }
};

PluginShellTests pluginShellTests;
}

int main (int, char**)
{
    juce::ScopedJuceInitialiser_GUI guiInitialiser;
    juce::UnitTestRunner runner;
    runner.setAssertOnFailure (false);
    runner.runAllTests();

    auto failures = 0;
    for (auto index = 0; index < runner.getNumResults(); ++index)
        if (const auto* result = runner.getResult (index))
        {
            failures += result->failures;

            for (const auto& message : result->messages)
                std::cerr << result->unitTestName << " / " << result->subcategoryName
                          << ": " << message << '\n';
        }

    return failures == 0 ? 0 : 1;
}
