#include <JuceHeader.h>

#include "ui/SpectrumComponent.h"

#include <cmath>

namespace
{
using spektrummer::ui::SpectrumComponent;

class SpectrumComponentTests final : public juce::UnitTest
{
public:
    SpectrumComponentTests() : juce::UnitTest ("Spectrum component") {}

    void runTest() override
    {
        beginTest ("Frequency mapping is logarithmic from 20 Hz to Nyquist");
        {
            constexpr auto sampleRate = 48000.0;
            expectWithinAbsoluteError (SpectrumComponent::frequencyToUnitX (20.0, sampleRate),
                                       0.0f,
                                       1.0e-6f);
            expectWithinAbsoluteError (SpectrumComponent::frequencyToUnitX (24000.0, sampleRate),
                                       1.0f,
                                       1.0e-6f);
            expectWithinAbsoluteError (
                SpectrumComponent::frequencyToUnitX (std::sqrt (20.0 * 24000.0), sampleRate),
                0.5f,
                1.0e-5f);
            expectEquals (SpectrumComponent::frequencyToUnitX (1.0, sampleRate), 0.0f);
            expectEquals (SpectrumComponent::frequencyToUnitX (30000.0, sampleRate), 1.0f);
        }

        beginTest ("Magnitude mapping is clamped from minus 96 to zero dBFS");
        {
            expectEquals (SpectrumComponent::decibelsToUnitY (0.0f), 0.0f);
            expectEquals (SpectrumComponent::decibelsToUnitY (-96.0f), 1.0f);
            expectEquals (SpectrumComponent::decibelsToUnitY (-120.0f), 1.0f);
            expectEquals (SpectrumComponent::decibelsToUnitY (12.0f), 0.0f);
            expectWithinAbsoluteError (SpectrumComponent::decibelsToUnitY (-48.0f),
                                       0.5f,
                                       1.0e-6f);
        }

        beginTest ("One-sided Hann FFT normalization treats endpoint bins once");
        {
            expectWithinAbsoluteError (SpectrumComponent::magnitudeNormalisationForBin (0),
                                       2.0f / SpectrumComponent::fftSize,
                                       1.0e-9f);
            expectWithinAbsoluteError (SpectrumComponent::magnitudeNormalisationForBin (1),
                                       4.0f / SpectrumComponent::fftSize,
                                       1.0e-9f);
            expectWithinAbsoluteError (
                SpectrumComponent::magnitudeNormalisationForBin (SpectrumComponent::fftSize / 2),
                2.0f / SpectrumComponent::fftSize,
                1.0e-9f);
        }
    }
};

SpectrumComponentTests spectrumComponentTests;
}
