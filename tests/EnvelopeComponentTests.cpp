#include <JuceHeader.h>

#include <array>
#include <cmath>

#include "ui/EnvelopeComponent.h"
#include "PluginProcessor.h"

namespace
{
class EnvelopeComponentTests final : public juce::UnitTest
{
public:
    EnvelopeComponentTests() : juce::UnitTest ("Envelope preview component") {}

    void runTest() override
    {
        beginTest ("Preview points are finite, ordered, and honour sustain level");
        {
            const auto bounds = juce::Rectangle<float> { 10.0f, 20.0f, 300.0f, 100.0f };
            const auto points = spektrummer::ui::EnvelopeComponent::makeEnvelopePoints (
                bounds, { 0.10f, 0.20f, 0.25f, 0.40f });

            for (const auto point : points)
            {
                expect (std::isfinite (point.x));
                expect (std::isfinite (point.y));
                expect (point.x >= bounds.getX() && point.x <= bounds.getRight());
                expect (point.y >= bounds.getY() && point.y <= bounds.getBottom());
            }

            for (auto index = std::size_t { 1 }; index < points.size(); ++index)
                expectGreaterOrEqual (points[index].x, points[index - 1].x);

            expectWithinAbsoluteError (points.front().y, bounds.getBottom(), 0.001f);
            expectWithinAbsoluteError (points[1].y, bounds.getY(), 0.001f);
            expectWithinAbsoluteError (points[2].y,
                                       bounds.getBottom() - bounds.getHeight() * 0.25f,
                                       0.001f);
            expectWithinAbsoluteError (points[3].y, points[2].y, 0.001f);
            expectWithinAbsoluteError (points.back().y, bounds.getBottom(), 0.001f);
        }

        beginTest ("Zero-time endpoints remain finite and ordered");
        {
            const auto points = spektrummer::ui::EnvelopeComponent::makeEnvelopePoints (
                { 0.0f, 0.0f, 160.0f, 80.0f }, { 0.0f, 0.0f, 1.0f, 0.0f });

            for (const auto point : points)
                expect (std::isfinite (point.x) && std::isfinite (point.y));

            for (auto index = std::size_t { 1 }; index < points.size(); ++index)
                expectGreaterOrEqual (points[index].x, points[index - 1].x);
        }

        beginTest ("Preview refresh reads current APVTS envelope parameters");
        {
            SpektrummerAudioProcessor processor;
            spektrummer::ui::EnvelopeComponent preview (processor.getValueTreeState());
            auto* attack = processor.getValueTreeState().getParameter ("attack");
            expect (attack != nullptr);

            if (attack != nullptr)
                attack->setValueNotifyingHost (attack->convertTo0to1 (2.5f));

            preview.refreshFromParameters();
            expectWithinAbsoluteError (preview.getDisplayedParameters().attackSeconds,
                                       2.5f,
                                       0.001f);
        }
    }
};

EnvelopeComponentTests envelopeComponentTests;
}
