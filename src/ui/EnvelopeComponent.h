#pragma once

#include <JuceHeader.h>

#include <array>
#include <atomic>

namespace spektrummer::ui
{
class EnvelopeComponent final : public juce::Component,
                                private juce::Timer
{
public:
    struct Parameters
    {
        float attackSeconds = 0.010f;
        float decaySeconds = 0.100f;
        float sustainLevel = 0.800f;
        float releaseSeconds = 0.080f;
    };

    explicit EnvelopeComponent (juce::AudioProcessorValueTreeState& state);
    ~EnvelopeComponent() override;

    void paint (juce::Graphics&) override;
    void refreshFromParameters();
    [[nodiscard]] Parameters getDisplayedParameters() const noexcept;

    [[nodiscard]] static std::array<juce::Point<float>, 5>
        makeEnvelopePoints (juce::Rectangle<float> bounds, Parameters parameters) noexcept;

private:
    void timerCallback() override;
    [[nodiscard]] Parameters readParameters() const noexcept;

    std::atomic<float>* attackParameter = nullptr;
    std::atomic<float>* decayParameter = nullptr;
    std::atomic<float>* sustainParameter = nullptr;
    std::atomic<float>* releaseParameter = nullptr;
    Parameters displayedParameters;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EnvelopeComponent)
};
}
