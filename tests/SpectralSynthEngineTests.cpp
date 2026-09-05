#include <JuceHeader.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <numeric>
#include <vector>

#include "dsp/SpectralSynthEngine.h"

namespace
{
using spektrummer::dsp::SpectralSynthEngine;

std::vector<float> renderNote (SpectralSynthEngine& engine,
                               int midiNote,
                               int numSamples,
                               const std::vector<int>& partitions = {})
{
    std::vector<float> output (static_cast<std::size_t> (numSamples));
    engine.noteOn (1, midiNote, 1.0f);

    if (partitions.empty())
    {
        engine.render (output.data(), numSamples);
        return output;
    }

    auto offset = 0;
    auto partitionIndex = std::size_t {};

    while (offset < numSamples)
    {
        const auto requested = partitions[partitionIndex % partitions.size()];
        const auto count = std::min (requested, numSamples - offset);
        engine.render (output.data() + offset, count);
        offset += count;
        ++partitionIndex;
    }

    return output;
}

float peakMagnitude (const std::vector<float>& samples)
{
    return std::accumulate (samples.begin(), samples.end(), 0.0f,
                            [] (float peak, float sample)
                            {
                                return std::max (peak, std::abs (sample));
                            });
}

float dominantFrequency (const std::vector<float>& samples, double sampleRate)
{
    constexpr auto analysisOrder = 12;
    constexpr auto analysisSize = 1 << analysisOrder;
    juce::dsp::FFT fft { analysisOrder };
    std::array<float, analysisSize * 2> data {};
    const auto sourceOffset = samples.size() - analysisSize;

    for (auto index = 0; index < analysisSize; ++index)
    {
        const auto phase = juce::MathConstants<double>::twoPi
                         * static_cast<double> (index)
                         / static_cast<double> (analysisSize);
        const auto window = static_cast<float> (0.5 - 0.5 * std::cos (phase));
        data[static_cast<std::size_t> (index)] = samples[sourceOffset + static_cast<std::size_t> (index)] * window;
    }

    fft.performFrequencyOnlyForwardTransform (data.data(), true);
    const auto firstBin = static_cast<int> (20.0 * analysisSize / sampleRate);
    const auto lastBin = static_cast<int> (2000.0 * analysisSize / sampleRate);
    const auto peak = std::max_element (data.begin() + firstBin, data.begin() + lastBin + 1);
    const auto peakBin = static_cast<int> (std::distance (data.begin(), peak));
    return static_cast<float> (static_cast<double> (peakBin) * sampleRate / analysisSize);
}

float measuredAmplitude (const std::vector<float>& samples,
                         int startSample,
                         int numSamples,
                         double frequency,
                         double sampleRate)
{
    auto real = 0.0;
    auto imaginary = 0.0;

    for (auto index = 0; index < numSamples; ++index)
    {
        const auto phase = juce::MathConstants<double>::twoPi
                         * frequency
                         * static_cast<double> (index)
                         / sampleRate;
        const auto sample = static_cast<double> (
            samples[static_cast<std::size_t> (startSample + index)]);
        real += sample * std::cos (phase);
        imaginary -= sample * std::sin (phase);
    }

    return static_cast<float> (2.0 * std::hypot (real, imaginary)
                               / static_cast<double> (numSamples));
}

float estimateFrequencyNear (const std::vector<float>& samples,
                             int startSample,
                             int numSamples,
                             double centreFrequency,
                             double sampleRate)
{
    auto bestFrequency = 0.0f;
    auto bestAmplitude = -1.0f;

    for (auto offset = -40; offset <= 40; ++offset)
    {
        const auto candidate = centreFrequency + static_cast<double> (offset) * 0.05;
        const auto amplitude = measuredAmplitude (samples,
                                                  startSample,
                                                  numSamples,
                                                  candidate,
                                                  sampleRate);

        if (amplitude > bestAmplitude)
        {
            bestAmplitude = amplitude;
            bestFrequency = static_cast<float> (candidate);
        }
    }

    return bestFrequency;
}

class SpectralSynthEngineTests final : public juce::UnitTest
{
public:
    SpectralSynthEngineTests() : juce::UnitTest ("Spectral synthesis engine") {}

    void runTest() override
    {
        beginTest ("A MIDI note produces audible finite inverse-FFT output");
        {
            SpectralSynthEngine engine;
            engine.prepare (48000.0, 257);
            const auto output = renderNote (engine, 69, 8192);

            expect (std::all_of (output.begin(), output.end(), [] (float value)
            {
                return std::isfinite (value);
            }));
            expectGreaterThan (peakMagnitude (output), 0.001f);
        }

        beginTest ("Voice limits are restricted to powers of two and steal oldest voices");
        {
            SpectralSynthEngine engine;
            engine.prepare (48000.0, 128);

            for (const auto voiceLimit : std::array { 2, 4, 8, 16 })
            {
                engine.reset();
                engine.setMaximumVoices (voiceLimit);

                for (auto note = 0; note < voiceLimit + 3; ++note)
                    engine.noteOn (1, 48 + note, 0.8f);

                expectEquals (engine.getMaximumVoices(), voiceLimit);
                expectEquals (engine.getActiveVoiceCount(), voiceLimit);
                expect (! engine.isNoteActive (1, 48));
                expect (! engine.isNoteActive (1, 49));
                expect (! engine.isNoteActive (1, 50));
                expect (engine.isNoteActive (1, 51));
                expect (engine.isNoteActive (1, 50 + voiceLimit));
            }

            engine.setMaximumVoices (3);
            expectEquals (engine.getMaximumVoices(), 2);
            expectEquals (engine.getActiveVoiceCount(), 2);
        }

        beginTest ("Velocity controls level and pitch follows the MIDI note");
        {
            SpectralSynthEngine quietEngine;
            SpectralSynthEngine loudEngine;
            quietEngine.prepare (48000.0, 128);
            loudEngine.prepare (48000.0, 128);
            quietEngine.noteOn (1, 69, 0.25f);
            loudEngine.noteOn (1, 69, 1.0f);

            std::vector<float> quiet (16384);
            std::vector<float> loud (16384);
            quietEngine.render (quiet.data(), static_cast<int> (quiet.size()));
            loudEngine.render (loud.data(), static_cast<int> (loud.size()));

            expectGreaterThan (peakMagnitude (loud), peakMagnitude (quiet) * 3.5f);
            expectWithinAbsoluteError (dominantFrequency (loud, 48000.0), 440.0f, 8.0f);
            expectWithinAbsoluteError (
                estimateFrequencyNear (loud, 4096, 12000, 440.0, 48000.0),
                440.0f,
                0.1f);
        }

        beginTest ("Same-note note-on retriggers one matching voice");
        {
            SpectralSynthEngine engine;
            engine.prepare (48000.0, 128);
            engine.noteOn (1, 60, 0.25f);
            engine.noteOn (1, 60, 1.0f);
            expectEquals (engine.getActiveVoiceCount(), 1);
            expect (engine.isNoteActive (1, 60));
        }

        beginTest ("ADSR envelope rises, sustains, and releases from its current level");
        {
            auto engine = std::make_unique<SpectralSynthEngine>();
            engine->prepare (48000.0, 128);
            engine->setEnvelopeParameters ({ 0.020f, 0.020f, 0.25f, 0.030f });
            engine->noteOn (1, 60, 1.0f);

            std::vector<float> attackAndSustain (8192);
            engine->render (attackAndSustain.data(), static_cast<int> (attackAndSustain.size()));
            expect (std::all_of (attackAndSustain.begin(),
                                 attackAndSustain.begin() + SpectralSynthEngine::hopSize,
                                 [] (float sample) { return sample == 0.0f; }));
            const auto onsetPeak = peakMagnitude (std::vector<float> (attackAndSustain.begin(),
                                                                        attackAndSustain.begin() + 512));
            const auto attackPeak = peakMagnitude (std::vector<float> (attackAndSustain.begin() + 512,
                                                                         attackAndSustain.begin() + 2048));
            const auto sustainPeak = peakMagnitude (std::vector<float> (attackAndSustain.begin() + 4096,
                                                                          attackAndSustain.end()));
            expectGreaterThan (attackPeak, onsetPeak);
            expectGreaterThan (attackPeak, sustainPeak * 2.0f);
            expectGreaterThan (sustainPeak, 0.0001f);

            engine->noteOff (1, 60);
            std::vector<float> release (8192);
            engine->render (release.data(), static_cast<int> (release.size()));
            expectGreaterThan (peakMagnitude (std::vector<float> (release.begin(),
                                                                    release.begin() + 1024)),
                               0.0001f);
            expectEquals (engine->getActiveVoiceCount(), 0);
            expect (std::all_of (release.end() - 512, release.end(), [] (float sample)
            {
                return sample == 0.0f;
            }));
        }

        beginTest ("Note-off releases deterministically from attack, decay, and sustain");
        {
            struct StageCase
            {
                SpectralSynthEngine::EnvelopeParameters parameters;
                int samplesBeforeNoteOff = 0;
            };

            for (const auto& stageCase : std::array {
                     StageCase { { 1.0f, 0.1f, 0.5f, 0.03f }, 512 },
                     StageCase { { 0.0f, 1.0f, 0.5f, 0.03f }, 512 },
                     StageCase { { 0.0f, 0.0f, 0.5f, 0.03f }, 1024 } })
            {
                auto engine = std::make_unique<SpectralSynthEngine>();
                engine->prepare (48000.0, 257);
                engine->setEnvelopeParameters (stageCase.parameters);
                engine->noteOn (1, 60, 1.0f);
                std::vector<float> beforeNoteOff (
                    static_cast<std::size_t> (stageCase.samplesBeforeNoteOff));
                engine->render (beforeNoteOff.data(), stageCase.samplesBeforeNoteOff);
                engine->noteOff (1, 60);
                std::vector<float> release (8192);
                engine->render (release.data(), static_cast<int> (release.size()));

                expectEquals (engine->getActiveVoiceCount(), 0);
                expect (std::all_of (release.begin(), release.end(), [] (float sample)
                {
                    return std::isfinite (sample);
                }));
            }
        }

        beginTest ("Zero-time ADSR stages have exact finite endpoint semantics");
        {
            auto engine = std::make_unique<SpectralSynthEngine>();
            engine->prepare (48000.0, 0);
            engine->setEnvelopeParameters ({ 0.0f, 0.0f, 1.0f, 0.0f });
            const auto output = renderNote (*engine, 69, 4096, { 1, 7, 257 });
            expectGreaterThan (peakMagnitude (output), 0.0001f);
            expect (std::all_of (output.begin(), output.end(), [] (float sample)
            {
                return std::isfinite (sample);
            }));

            engine->noteOff (1, 69);
            std::array<float, 1> afterNoteOff {};
            engine->render (afterNoteOff.data(), 1);
            expectEquals (engine->getActiveVoiceCount(), 0);
            expect (std::isfinite (afterNoteOff.front()));
        }

        beginTest ("Automated envelope parameters use a deterministic 20 ms ramp");
        {
            auto engine = std::make_unique<SpectralSynthEngine>();
            engine->prepare (48000.0, 257);
            engine->setEnvelopeParameters ({ 0.0f, 0.0f, 1.0f, 0.1f });
            engine->setEnvelopeParameterTargets ({ 0.0f, 0.0f, 0.0f, 0.1f });
            std::array<float, 1> firstSample {};
            engine->render (firstSample.data(), 1);
            const auto afterOneSample = engine->getEnvelopeParameters();
            expectGreaterThan (afterOneSample.sustainLevel, 0.0f);
            expectLessThan (afterOneSample.sustainLevel, 1.0f);

            std::array<float, 959> restOfRamp {};
            engine->render (restOfRamp.data(), static_cast<int> (restOfRamp.size()));
            expectWithinAbsoluteError (engine->getEnvelopeParameters().sustainLevel,
                                       0.0f,
                                       1.0e-6f);

            engine->reset();
            expectWithinAbsoluteError (engine->getEnvelopeParameters().sustainLevel,
                                       0.0f,
                                       1.0e-6f);
        }

        beginTest ("All-notes-off is channel selective and all-sound-off is global");
        {
            auto engine = std::make_unique<SpectralSynthEngine>();
            engine->prepare (48000.0, 128);
            engine->setEnvelopeParameters ({ 0.0f, 0.0f, 1.0f, 0.02f });
            engine->noteOn (1, 60, 1.0f);
            engine->noteOn (2, 67, 1.0f);
            engine->allNotesOff (1);
            std::array<float, 4096> release {};
            engine->render (release.data(), static_cast<int> (release.size()));
            expect (! engine->isNoteActive (1, 60));
            expect (engine->isNoteActive (2, 67));

            engine->allSoundOff (1);
            expectEquals (engine->getActiveVoiceCount(), 0);
        }

        beginTest ("Note-off releases audibly and all-sound-off is a global panic");
        {
            SpectralSynthEngine shortNoteEngine;
            shortNoteEngine.prepare (48000.0, 128);
            shortNoteEngine.noteOn (1, 60, 1.0f);
            std::array<float, 1024> attackOutput {};
            shortNoteEngine.render (attackOutput.data(), static_cast<int> (attackOutput.size()));
            shortNoteEngine.noteOff (1, 60);
            std::vector<float> shortRelease (8192);
            shortNoteEngine.render (shortRelease.data(), static_cast<int> (shortRelease.size()));
            expectGreaterThan (peakMagnitude (shortRelease), 0.0001f);

            SpectralSynthEngine engine;
            engine.prepare (48000.0, 128);
            engine.noteOn (1, 60, 1.0f);
            std::array<float, 4096> sustained {};
            engine.render (sustained.data(), static_cast<int> (sustained.size()));
            engine.noteOff (1, 60);

            std::vector<float> release (8192);
            engine.render (release.data(), static_cast<int> (release.size()));
            expectGreaterThan (peakMagnitude (
                                   std::vector<float> (release.begin(), release.begin() + 2048)),
                               0.001f);
            expectEquals (engine.getActiveVoiceCount(), 0);
            expect (std::all_of (release.end() - 512, release.end(), [] (float sample)
            {
                return sample == 0.0f;
            }));

            engine.noteOn (1, 64, 1.0f);
            engine.noteOn (1, 67, 1.0f);
            engine.allNotesOff (1);
            engine.render (release.data(), static_cast<int> (release.size()));
            expectEquals (engine.getActiveVoiceCount(), 0);

            engine.noteOn (1, 64, 1.0f);
            engine.noteOn (2, 67, 1.0f);
            std::array<float, 1024> activeOutput {};
            engine.render (activeOutput.data(), static_cast<int> (activeOutput.size()));
            engine.allSoundOff (1);
            expectEquals (engine.getActiveVoiceCount(), 0);

            std::array<float, 1024> killedOutput {};
            engine.render (killedOutput.data(), static_cast<int> (killedOutput.size()));
            expect (std::all_of (killedOutput.begin(), killedOutput.end(), [] (float sample)
            {
                return sample == 0.0f;
            }));
        }

        beginTest ("Overlap-add hop boundaries have no discontinuity spikes");
        {
            SpectralSynthEngine engine;
            engine.prepare (48000.0, 512);
            const auto output = renderNote (engine, 69, 16384);
            auto maximumDelta = 0.0f;
            auto maximumBoundaryDelta = 0.0f;

            for (auto index = SpectralSynthEngine::fftSize + 1;
                 index < static_cast<int> (output.size());
                 ++index)
            {
                const auto delta = std::abs (output[static_cast<std::size_t> (index)]
                                           - output[static_cast<std::size_t> (index - 1)]);
                maximumDelta = std::max (maximumDelta, delta);

                if (index % SpectralSynthEngine::hopSize == 0)
                    maximumBoundaryDelta = std::max (maximumBoundaryDelta, delta);
            }

            expectGreaterThan (maximumDelta, 0.0f);
            expectLessOrEqual (maximumBoundaryDelta, maximumDelta * 1.1f + 1.0e-6f);
        }

        beginTest ("Fractional-bin synthesis rejects hop-rate modulation sidebands");
        {
            constexpr auto sampleRate = 48000.0;
            constexpr auto fundamental = 440.0;
            constexpr auto hopRate = sampleRate / SpectralSynthEngine::hopSize;
            SpectralSynthEngine engine;
            engine.prepare (sampleRate, 512);
            const auto output = renderNote (engine, 69, 48000);
            constexpr auto analysisStart = 4096;
            constexpr auto analysisLength = 40000;
            const auto carrier = measuredAmplitude (output,
                                                    analysisStart,
                                                    analysisLength,
                                                    fundamental,
                                                    sampleRate);
            const auto lowerSideband = measuredAmplitude (output,
                                                          analysisStart,
                                                          analysisLength,
                                                          fundamental - hopRate,
                                                          sampleRate);
            const auto upperSideband = measuredAmplitude (output,
                                                          analysisStart,
                                                          analysisLength,
                                                          fundamental + hopRate,
                                                          sampleRate);
            expectGreaterThan (carrier, 0.001f);
            expectLessThan (lowerSideband, carrier * 0.003f);
            expectLessThan (upperSideband, carrier * 0.003f);
        }

        beginTest ("Reset and host block partitioning are deterministic");
        {
            SpectralSynthEngine contiguous;
            SpectralSynthEngine partitioned;
            contiguous.prepare (44100.0, 1024);
            partitioned.prepare (44100.0, 1024);

            const auto expected = renderNote (contiguous, 57, 10000);
            const auto actual = renderNote (partitioned, 57, 10000, { 1, 7, 64, 257, 1024 });
            expectEquals (expected.size(), actual.size());

            for (auto index = std::size_t {}; index < expected.size(); ++index)
                expectWithinAbsoluteError (actual[index], expected[index], 1.0e-7f);

            contiguous.reset();
            const auto afterReset = renderNote (contiguous, 57, 10000);

            for (auto index = std::size_t {}; index < expected.size(); ++index)
                expectWithinAbsoluteError (afterReset[index], expected[index], 1.0e-7f);
        }

        beginTest ("Zero-length rendering is harmless");
        {
            SpectralSynthEngine engine;
            engine.prepare (96000.0, 0);
            engine.noteOn (1, 72, 0.5f);
            engine.render (nullptr, 0);
            expectEquals (engine.getActiveVoiceCount(), 1);
        }

        beginTest ("Supported sample rates produce deterministic finite audio");
        {
            for (const auto sampleRate : std::array { 44100.0, 48000.0, 96000.0 })
            {
                SpectralSynthEngine first;
                SpectralSynthEngine second;
                first.prepare (sampleRate, 257);
                second.prepare (sampleRate, 1024);
                const auto expected = renderNote (first, 57, 8192, { 257 });
                const auto actual = renderNote (second, 57, 8192, { 1, 64, 511, 1024 });

                for (auto index = std::size_t {}; index < expected.size(); ++index)
                {
                    expect (std::isfinite (expected[index]));
                    expectWithinAbsoluteError (actual[index], expected[index], 1.0e-7f);
                }
            }
        }

        beginTest ("Repeated prepare matches a fresh engine at the new sample rate");
        {
            SpectralSynthEngine reused;
            reused.prepare (44100.0, 128);
            const auto discarded = renderNote (reused, 48, 4096);
            expectGreaterThan (peakMagnitude (discarded), 0.0001f);
            reused.prepare (96000.0, 257);

            SpectralSynthEngine fresh;
            fresh.prepare (96000.0, 257);
            const auto actual = renderNote (reused, 72, 8192, { 7, 257, 1024 });
            const auto expected = renderNote (fresh, 72, 8192, { 7, 257, 1024 });

            for (auto index = std::size_t {}; index < expected.size(); ++index)
                expectWithinAbsoluteError (actual[index], expected[index], 1.0e-7f);
        }
    }
};

SpectralSynthEngineTests spectralSynthEngineTests;
}
