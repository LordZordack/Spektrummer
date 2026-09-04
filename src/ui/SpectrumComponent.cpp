#include "SpectrumComponent.h"

#include <algorithm>
#include <cmath>

namespace spektrummer::ui
{
SpectrumComponent::SpectrumComponent (analyzer::AnalyzerSampleFifo& sampleFifo,
                                      const juce::AudioProcessor& audioProcessor)
    : fifo (sampleFifo), processor (audioProcessor)
{
    setComponentID ("spectrum");
    setName ("Post-output spectrum");
    setAccessible (true);
    observedGeneration = fifo.getGeneration();
    fifo.discardAll();
    resetDisplay();
    startTimerHz (30);
}

SpectrumComponent::~SpectrumComponent()
{
    stopTimer();
}

void SpectrumComponent::paint (juce::Graphics& graphics)
{
    const auto graph = getGraphBounds();
    graphics.fillAll (juce::Colour { 0xff111821 });
    graphics.setColour (juce::Colour { 0xff263746 });
    graphics.fillRoundedRectangle (graph, 6.0f);

    graphics.setFont (juce::FontOptions { 11.0f });
    graphics.setColour (juce::Colour { 0xff78909c });

    for (const auto decibels : std::array { 0.0f, -24.0f, -48.0f, -72.0f, -96.0f })
    {
        const auto y = graph.getY() + decibelsToUnitY (decibels) * graph.getHeight();
        graphics.drawHorizontalLine (juce::roundToInt (y), graph.getX(), graph.getRight());
        graphics.drawText (juce::String (static_cast<int> (decibels)),
                           4,
                           juce::roundToInt (y) - 7,
                           juce::roundToInt (graph.getX()) - 8,
                           14,
                           juce::Justification::centredRight);
    }

    const auto sampleRate = processor.getSampleRate() > 40.0 ? processor.getSampleRate() : 44100.0;

    for (const auto frequency : std::array { 20.0, 100.0, 1000.0, 10000.0 })
    {
        if (frequency >= sampleRate * 0.5)
            continue;

        const auto x = graph.getX() + frequencyToUnitX (frequency, sampleRate) * graph.getWidth();
        graphics.drawVerticalLine (juce::roundToInt (x), graph.getY(), graph.getBottom());
        const auto label = frequency >= 1000.0
                             ? juce::String (frequency / 1000.0, frequency < 10000.0 ? 1 : 0) + "k"
                             : juce::String (static_cast<int> (frequency));
        graphics.drawText (label,
                           juce::roundToInt (x) - 24,
                           juce::roundToInt (graph.getBottom()) + 3,
                           48,
                           16,
                           juce::Justification::centred);
    }

    juce::Path path;
    auto pathStarted = false;

    for (auto bin = 1; bin <= fftSize / 2; ++bin)
    {
        const auto frequency = static_cast<double> (bin) * sampleRate / fftSize;

        if (frequency < 20.0)
            continue;

        const auto x = graph.getX() + frequencyToUnitX (frequency, sampleRate) * graph.getWidth();
        const auto y = graph.getY()
                     + decibelsToUnitY (smoothedDecibels[static_cast<std::size_t> (bin)])
                         * graph.getHeight();

        if (! pathStarted)
        {
            path.startNewSubPath (x, y);
            pathStarted = true;
        }
        else
        {
            path.lineTo (x, y);
        }
    }

    graphics.setColour (juce::Colour { 0xff65d6ff });
    graphics.strokePath (path, juce::PathStrokeType { 1.8f });
    graphics.setColour (juce::Colour { 0xffa8c8d8 });
    graphics.drawText ("dBFS", 4, 2, 42, 18, juce::Justification::centredLeft);
}

float SpectrumComponent::frequencyToUnitX (double frequency, double sampleRate) noexcept
{
    const auto nyquist = std::max (20.0, sampleRate * 0.5);
    const auto clamped = juce::jlimit (20.0, nyquist, frequency);

    if (nyquist <= 20.0)
        return 0.0f;

    return static_cast<float> (std::log (clamped / 20.0) / std::log (nyquist / 20.0));
}

float SpectrumComponent::decibelsToUnitY (float decibels) noexcept
{
    const auto clamped = juce::jlimit (minimumDecibels, 0.0f, decibels);
    return 1.0f - (clamped - minimumDecibels) / -minimumDecibels;
}

float SpectrumComponent::magnitudeNormalisationForBin (int bin) noexcept
{
    const auto oneSidedScale = bin == 0 || bin == fftSize / 2 ? 2.0f : 4.0f;
    return oneSidedScale / static_cast<float> (fftSize);
}

void SpectrumComponent::timerCallback()
{
    const auto generation = fifo.getGeneration();

    if (generation != observedGeneration)
    {
        observedGeneration = generation;
        fifo.discardAll();
        resetDisplay();
    }

    auto remaining = fifo.getNumReady();

    while (remaining > 0)
    {
        const auto requested = std::min (remaining, static_cast<int> (drainBuffer.size()));
        const auto popped = fifo.pop (drainBuffer.data(), requested);

        if (popped <= 0)
            break;

        for (auto index = 0; index < popped; ++index)
        {
            sampleHistory[static_cast<std::size_t> (historyWriteIndex)]
                = drainBuffer[static_cast<std::size_t> (index)];
            historyWriteIndex = (historyWriteIndex + 1) % fftSize;
            samplesCollected = std::min (fftSize, samplesCollected + 1);
        }

        remaining -= popped;
    }

    if (samplesCollected == fftSize)
        calculateSpectrum();

    repaint();
}

void SpectrumComponent::resetDisplay() noexcept
{
    sampleHistory.fill (0.0f);
    fftData.fill (0.0f);
    smoothedDecibels.fill (minimumDecibels);
    historyWriteIndex = 0;
    samplesCollected = 0;
}

void SpectrumComponent::calculateSpectrum() noexcept
{
    fftData.fill (0.0f);

    for (auto index = 0; index < fftSize; ++index)
        fftData[static_cast<std::size_t> (index)]
            = sampleHistory[static_cast<std::size_t> ((historyWriteIndex + index) % fftSize)];

    analysisWindow.multiplyWithWindowingTable (fftData.data(), static_cast<std::size_t> (fftSize));
    forwardFft.performFrequencyOnlyForwardTransform (fftData.data(), true);

    for (auto bin = 0; bin <= fftSize / 2; ++bin)
    {
        const auto magnitude = fftData[static_cast<std::size_t> (bin)]
                             * magnitudeNormalisationForBin (bin);
        const auto decibels = juce::Decibels::gainToDecibels (magnitude, minimumDecibels);
        auto& smoothed = smoothedDecibels[static_cast<std::size_t> (bin)];
        const auto coefficient = decibels > smoothed ? 0.55f : 0.15f;
        smoothed += coefficient * (juce::jlimit (minimumDecibels, 0.0f, decibels) - smoothed);
    }
}

juce::Rectangle<float> SpectrumComponent::getGraphBounds() const noexcept
{
    return getLocalBounds().toFloat().withTrimmedLeft (48.0f)
                                     .withTrimmedRight (12.0f)
                                     .withTrimmedTop (22.0f)
                                     .withTrimmedBottom (22.0f);
}
}
