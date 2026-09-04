#include "SpectralSynthEngine.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include <algorithm>
#include <cmath>
#include <complex>

namespace spektrummer::dsp
{
namespace
{
constexpr auto maximumCombinedAmplitude = 0.8f;

constexpr float harmonicWeightSum() noexcept
{
    auto sum = 0.0f;

    for (auto harmonic = 1; harmonic <= 8; ++harmonic)
        sum += 1.0f / static_cast<float> (harmonic);

    return sum;
}

constexpr auto harmonicNormalisation = harmonicWeightSum();
}

void SpectralSynthEngine::prepare (double newSampleRate, int) noexcept
{
    jassert (newSampleRate > 0.0);
    sampleRate = std::max (1.0, newSampleRate);
    prepareSpectralKernels();
    reset();
}

void SpectralSynthEngine::reset() noexcept
{
    for (auto& voice : voices)
        voice = {};

    spectrumData.fill ({});
    overlapBuffer.fill (0.0f);
    nextVoiceAge = 0;
    outputIndex = 0;
    samplesUntilNextFrame = 0;
}

void SpectralSynthEngine::setMaximumVoices (int requestedVoices) noexcept
{
    maximumVoices = normaliseVoiceLimit (requestedVoices);
    enforceVoiceLimit();
}

int SpectralSynthEngine::getMaximumVoices() const noexcept
{
    return maximumVoices;
}

int SpectralSynthEngine::getActiveVoiceCount() const noexcept
{
    return static_cast<int> (std::count_if (voices.begin(), voices.end(), [] (const Voice& voice)
    {
        return voice.stage != EnvelopeStage::inactive;
    }));
}

bool SpectralSynthEngine::isNoteActive (int midiChannel, int midiNote) const noexcept
{
    return std::any_of (voices.begin(), voices.end(), [midiChannel, midiNote] (const Voice& voice)
    {
        return voice.stage != EnvelopeStage::inactive
            && voice.channel == midiChannel
            && voice.note == midiNote;
    });
}

void SpectralSynthEngine::noteOn (int midiChannel, int midiNote, float velocity) noexcept
{
    if (velocity <= 0.0f)
    {
        noteOff (midiChannel, midiNote);
        return;
    }

    auto* voice = findMatchingVoice (midiChannel, midiNote);

    if (voice == nullptr)
        voice = findVoiceForNoteOn();

    *voice = {};
    voice->channel = midiChannel;
    voice->note = juce::jlimit (0, 127, midiNote);
    voice->velocity = juce::jlimit (0.0f, 1.0f, velocity);
    voice->envelope = 1.0f;
    voice->age = ++nextVoiceAge;
    voice->stage = EnvelopeStage::sustain;
}

void SpectralSynthEngine::noteOff (int midiChannel, int midiNote) noexcept
{
    for (auto& voice : voices)
        if (voice.stage != EnvelopeStage::inactive
            && voice.channel == midiChannel
            && voice.note == midiNote)
            voice.stage = EnvelopeStage::release;
}

void SpectralSynthEngine::allNotesOff (int midiChannel) noexcept
{
    for (auto& voice : voices)
        if (voice.stage != EnvelopeStage::inactive
            && (midiChannel == 0 || voice.channel == midiChannel))
            voice.stage = EnvelopeStage::release;
}

void SpectralSynthEngine::allSoundOff (int) noexcept
{
    for (auto& voice : voices)
        voice = {};

    spectrumData.fill ({});
    overlapBuffer.fill (0.0f);
    samplesUntilNextFrame = 0;
}

void SpectralSynthEngine::render (float* output, int numSamples) noexcept
{
    if (output == nullptr || numSamples <= 0)
        return;

    for (auto sample = 0; sample < numSamples; ++sample)
    {
        if (samplesUntilNextFrame == 0)
            generateFrame();

        auto& nextOutput = overlapBuffer[static_cast<std::size_t> (outputIndex)];
        output[sample] = nextOutput;
        nextOutput = 0.0f;
        outputIndex = (outputIndex + 1) % fftSize;
        --samplesUntilNextFrame;
    }
}

int SpectralSynthEngine::normaliseVoiceLimit (int requestedVoices) noexcept
{
    constexpr std::array limits { 2, 4, 8, 16 };
    auto result = limits.front();
    auto smallestDistance = std::abs (requestedVoices - result);

    for (const auto limit : limits)
    {
        const auto distance = std::abs (requestedVoices - limit);

        if (distance < smallestDistance)
        {
            result = limit;
            smallestDistance = distance;
        }
    }

    return result;
}

SpectralSynthEngine::Voice* SpectralSynthEngine::findMatchingVoice (int midiChannel,
                                                                    int midiNote) noexcept
{
    const auto match = std::find_if (voices.begin(), voices.end(), [midiChannel, midiNote] (const Voice& voice)
    {
        return voice.stage != EnvelopeStage::inactive
            && voice.channel == midiChannel
            && voice.note == midiNote;
    });

    return match == voices.end() ? nullptr : &*match;
}

SpectralSynthEngine::Voice* SpectralSynthEngine::findVoiceForNoteOn() noexcept
{
    if (getActiveVoiceCount() < maximumVoices)
    {
        const auto inactive = std::find_if (voices.begin(), voices.end(), [] (const Voice& voice)
        {
            return voice.stage == EnvelopeStage::inactive;
        });

        if (inactive != voices.end())
            return &*inactive;
    }

    auto* oldest = static_cast<Voice*> (nullptr);

    for (auto& voice : voices)
        if (voice.stage != EnvelopeStage::inactive
            && (oldest == nullptr || voice.age < oldest->age))
            oldest = &voice;

    jassert (oldest != nullptr);
    return oldest != nullptr ? oldest : &voices.front();
}

void SpectralSynthEngine::enforceVoiceLimit() noexcept
{
    while (getActiveVoiceCount() > maximumVoices)
    {
        auto* oldest = static_cast<Voice*> (nullptr);

        for (auto& voice : voices)
            if (voice.stage != EnvelopeStage::inactive
                && (oldest == nullptr || voice.age < oldest->age))
                oldest = &voice;

        if (oldest == nullptr)
            break;

        *oldest = {};
    }
}

void SpectralSynthEngine::advanceEnvelope (Voice& voice) noexcept
{
    if (voice.stage == EnvelopeStage::release)
    {
        voice.envelope -= static_cast<float> (static_cast<double> (hopSize)
                                               / (SpectralSynthEngine::releaseSeconds * sampleRate));

        if (voice.envelope <= 0.0f)
            voice = {};
    }
}

void SpectralSynthEngine::prepareSpectralKernels() noexcept
{
    const auto nyquist = sampleRate * 0.5;

    for (auto midiNote = 0; midiNote < static_cast<int> (noteKernels->size()); ++midiNote)
    {
        const auto fundamental = juce::MidiMessage::getMidiNoteInHertz (midiNote);

        for (auto harmonic = 1; harmonic <= harmonicCount; ++harmonic)
        {
            auto& kernel = (*noteKernels)[static_cast<std::size_t> (midiNote)]
                                         [static_cast<std::size_t> (harmonic - 1)];
            kernel = {};
            kernel.frequency = fundamental * static_cast<double> (harmonic);

            if (kernel.frequency >= nyquist)
                continue;

            const auto binPosition = kernel.frequency * static_cast<double> (fftSize) / sampleRate;
            const auto firstBin = std::max (0, static_cast<int> (std::floor (binPosition)) - kernelRadius);
            const auto lastBin = std::min (fftSize / 2,
                                           static_cast<int> (std::ceil (binPosition)) + kernelRadius);

            for (auto bin = firstBin; bin <= lastBin; ++bin)
            {
                auto& entry = kernel.bins[static_cast<std::size_t> (kernel.numBins++)];
                entry.bin = bin;
                entry.positiveResponse = static_cast<std::complex<float>> (
                    hannKernel (binPosition - static_cast<double> (bin)));
                entry.negativeResponse = static_cast<std::complex<float>> (
                    hannKernel (-binPosition - static_cast<double> (bin)));
            }
        }
    }
}

std::complex<double> SpectralSynthEngine::dirichletKernel (double binOffset) noexcept
{
    const auto wrappedOffset = std::remainder (binOffset, static_cast<double> (fftSize));
    const auto denominator = std::sin (juce::MathConstants<double>::pi
                                      * wrappedOffset
                                      / static_cast<double> (fftSize));

    if (std::abs (denominator) < 1.0e-12)
        return { static_cast<double> (fftSize), 0.0 };

    const auto magnitude = std::sin (juce::MathConstants<double>::pi * wrappedOffset)
                         / denominator;
    const auto phase = juce::MathConstants<double>::pi
                     * wrappedOffset
                     * static_cast<double> (fftSize - 1)
                     / static_cast<double> (fftSize);
    return magnitude * std::complex<double> { std::cos (phase), std::sin (phase) };
}

std::complex<double> SpectralSynthEngine::hannKernel (double binOffset) noexcept
{
    return 0.5 * dirichletKernel (binOffset)
         - 0.25 * dirichletKernel (binOffset + 1.0)
         - 0.25 * dirichletKernel (binOffset - 1.0);
}

void SpectralSynthEngine::addPartialToSpectrum (const PartialKernel& kernel,
                                                 double phase,
                                                 float amplitude) noexcept
{
    const auto positivePhase = std::complex<float> {
        static_cast<float> (std::cos (phase)),
        static_cast<float> (std::sin (phase))
    };
    const auto negativePhase = std::conj (positivePhase);
    const auto scale = amplitude * 0.5f;

    for (auto index = 0; index < kernel.numBins; ++index)
    {
        const auto& entry = kernel.bins[static_cast<std::size_t> (index)];
        auto coefficient = scale * (positivePhase * entry.positiveResponse
                                  + negativePhase * entry.negativeResponse);

        if (entry.bin == 0 || entry.bin == fftSize / 2)
            coefficient = { coefficient.real(), 0.0f };

        spectrumData[static_cast<std::size_t> (entry.bin)] += coefficient;
    }
}

void SpectralSynthEngine::generateFrame() noexcept
{
    spectrumData.fill ({});
    const auto voiceAmplitude = maximumCombinedAmplitude / static_cast<float> (maximumVoices);
    const auto nyquist = sampleRate * 0.5;

    for (auto& voice : voices)
    {
        if (voice.stage == EnvelopeStage::inactive)
            continue;

        advanceEnvelope (voice);

        if (voice.stage == EnvelopeStage::inactive)
            continue;

        for (auto harmonic = 1; harmonic <= harmonicCount; ++harmonic)
        {
            const auto& kernel = (*noteKernels)[static_cast<std::size_t> (voice.note)]
                                               [static_cast<std::size_t> (harmonic - 1)];

            if (kernel.frequency >= nyquist || kernel.numBins == 0)
                break;

            const auto harmonicWeight = (1.0f / static_cast<float> (harmonic)) / harmonicNormalisation;
            const auto amplitude = voiceAmplitude * voice.velocity * voice.envelope * harmonicWeight;
            auto& phase = voice.phases[static_cast<std::size_t> (harmonic - 1)];
            addPartialToSpectrum (kernel, phase, amplitude);
            phase = std::remainder (phase + juce::MathConstants<double>::twoPi
                                          * kernel.frequency
                                          * static_cast<double> (hopSize)
                                          / sampleRate,
                                    juce::MathConstants<double>::twoPi);
        }
    }

    for (auto bin = 1; bin < fftSize / 2; ++bin)
        spectrumData[static_cast<std::size_t> (fftSize - bin)]
            = std::conj (spectrumData[static_cast<std::size_t> (bin)]);

    inverseFft.inverse (spectrumData);

    for (auto index = 0; index < fftSize; ++index)
    {
        const auto destination = static_cast<std::size_t> ((outputIndex + index) % fftSize);
        overlapBuffer[destination] += spectrumData[static_cast<std::size_t> (index)].real() * 0.5f;
    }

    samplesUntilNextFrame = hopSize;
}
}
