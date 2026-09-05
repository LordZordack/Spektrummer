#include "EnvelopeComponent.h"

#include <algorithm>
#include <cmath>

namespace spektrummer::ui
{
EnvelopeComponent::EnvelopeComponent (juce::AudioProcessorValueTreeState& state)
    : attackParameter (state.getRawParameterValue ("attack")),
      decayParameter (state.getRawParameterValue ("decay")),
      sustainParameter (state.getRawParameterValue ("sustain")),
      releaseParameter (state.getRawParameterValue ("release")),
      displayedParameters (readParameters())
{
    setComponentID ("envelopePreview");
    setName ("ADSR envelope preview");
    setInterceptsMouseClicks (false, false);
    startTimerHz (60);
}

EnvelopeComponent::~EnvelopeComponent()
{
    stopTimer();
}

void EnvelopeComponent::paint (juce::Graphics& graphics)
{
    auto bounds = getLocalBounds().toFloat().reduced (8.0f);
    graphics.setColour (juce::Colour { 0xff202a35 });
    graphics.fillRoundedRectangle (bounds, 6.0f);

    auto graphBounds = bounds.reduced (12.0f, 18.0f);
    const auto points = makeEnvelopePoints (graphBounds, displayedParameters);
    juce::Path path;
    path.startNewSubPath (points.front());

    for (auto index = std::size_t { 1 }; index < points.size(); ++index)
        path.lineTo (points[index]);

    graphics.setColour (juce::Colour { 0xff6ad9ff });
    graphics.strokePath (path, juce::PathStrokeType { 2.0f });
    graphics.setColour (juce::Colour { 0xffa8bac7 });
    graphics.setFont (juce::FontOptions { 11.0f });
    graphics.drawText ("ADSR preview", bounds.removeFromTop (16.0f), juce::Justification::centred);
}

std::array<juce::Point<float>, 5> EnvelopeComponent::makeEnvelopePoints (
    juce::Rectangle<float> bounds,
    Parameters parameters) noexcept
{
    const auto attack = std::max (0.0f, std::isfinite (parameters.attackSeconds)
                                             ? parameters.attackSeconds : 0.0f);
    const auto decay = std::max (0.0f, std::isfinite (parameters.decaySeconds)
                                            ? parameters.decaySeconds : 0.0f);
    const auto release = std::max (0.0f, std::isfinite (parameters.releaseSeconds)
                                              ? parameters.releaseSeconds : 0.0f);
    const auto sustain = juce::jlimit (0.0f, 1.0f,
                                      std::isfinite (parameters.sustainLevel)
                                          ? parameters.sustainLevel : 0.0f);
    const auto timedTotal = attack + decay + release;
    const auto sustainWidth = std::max (0.1f, timedTotal * 0.25f);
    const auto total = std::max (0.1f, timedTotal + sustainWidth);
    const auto toX = [&] (float seconds) noexcept
    {
        return bounds.getX() + bounds.getWidth() * seconds / total;
    };
    const auto sustainY = bounds.getBottom() - bounds.getHeight() * sustain;
    const auto attackEnd = attack;
    const auto decayEnd = attackEnd + decay;
    const auto sustainEnd = decayEnd + sustainWidth;

    return {
        juce::Point<float> { bounds.getX(), bounds.getBottom() },
        juce::Point<float> { toX (attackEnd), bounds.getY() },
        juce::Point<float> { toX (decayEnd), sustainY },
        juce::Point<float> { toX (sustainEnd), sustainY },
        juce::Point<float> { bounds.getRight(), bounds.getBottom() }
    };
}

void EnvelopeComponent::timerCallback()
{
    refreshFromParameters();
}

void EnvelopeComponent::refreshFromParameters()
{
    const auto parameters = readParameters();

    if (parameters.attackSeconds != displayedParameters.attackSeconds
        || parameters.decaySeconds != displayedParameters.decaySeconds
        || parameters.sustainLevel != displayedParameters.sustainLevel
        || parameters.releaseSeconds != displayedParameters.releaseSeconds)
    {
        displayedParameters = parameters;
        repaint();
    }
}

EnvelopeComponent::Parameters EnvelopeComponent::getDisplayedParameters() const noexcept
{
    return displayedParameters;
}

EnvelopeComponent::Parameters EnvelopeComponent::readParameters() const noexcept
{
    const auto loadOr = [] (const std::atomic<float>* parameter, float fallback) noexcept
    {
        return parameter != nullptr ? parameter->load (std::memory_order_relaxed) : fallback;
    };

    return {
        loadOr (attackParameter, 0.010f),
        loadOr (decayParameter, 0.100f),
        loadOr (sustainParameter, 0.800f),
        loadOr (releaseParameter, 0.080f)
    };
}
}
