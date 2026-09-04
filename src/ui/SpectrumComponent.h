#pragma once

#include <JuceHeader.h>

#include "analyzer/AnalyzerSampleFifo.h"

#include <array>
#include <cstdint>

namespace spektrummer::ui
{
class SpectrumComponent final : public juce::Component,
                                private juce::Timer
{
public:
    static constexpr int fftOrder = 11;
    static constexpr int fftSize = 1 << fftOrder;
    static constexpr float minimumDecibels = -96.0f;

    SpectrumComponent (analyzer::AnalyzerSampleFifo& sampleFifo,
                       const juce::AudioProcessor& audioProcessor);
    ~SpectrumComponent() override;

    void paint (juce::Graphics& graphics) override;

    [[nodiscard]] static float frequencyToUnitX (double frequency,
                                                  double sampleRate) noexcept;
    [[nodiscard]] static float decibelsToUnitY (float decibels) noexcept;
    [[nodiscard]] static float magnitudeNormalisationForBin (int bin) noexcept;

private:
    void timerCallback() override;
    void resetDisplay() noexcept;
    void calculateSpectrum() noexcept;
    [[nodiscard]] juce::Rectangle<float> getGraphBounds() const noexcept;

    analyzer::AnalyzerSampleFifo& fifo;
    const juce::AudioProcessor& processor;
    juce::dsp::FFT forwardFft { fftOrder };
    juce::dsp::WindowingFunction<float> analysisWindow {
        static_cast<std::size_t> (fftSize),
        juce::dsp::WindowingFunction<float>::hann,
        false
    };
    std::array<float, fftSize> sampleHistory {};
    std::array<float, fftSize * 2> fftData {};
    std::array<float, fftSize / 2 + 1> smoothedDecibels {};
    std::array<float, 2048> drainBuffer {};
    std::uint64_t observedGeneration = 0;
    int historyWriteIndex = 0;
    int samplesCollected = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpectrumComponent)
};
}
