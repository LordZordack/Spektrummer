#include <JuceHeader.h>

#include <array>
#include <cmath>
#include <complex>

#include "dsp/LockFreeFft.h"

namespace
{
class LockFreeFftTests final : public juce::UnitTest
{
public:
    LockFreeFftTests() : juce::UnitTest ("Lock-free inverse FFT") {}

    void runTest() override
    {
        beginTest ("Inverse transform reconstructs a unit cosine");
        {
            using Transform = spektrummer::dsp::LockFreeFft<11>;
            Transform transform;
            std::array<std::complex<float>, Transform::size> spectrum {};
            const auto coefficient = static_cast<float> (Transform::size) * 0.5f;
            spectrum[1] = { coefficient, 0.0f };
            spectrum[Transform::size - 1] = { coefficient, 0.0f };
            transform.inverse (spectrum);

            for (auto index = std::size_t {}; index < spectrum.size(); ++index)
            {
                const auto expected = static_cast<float> (std::cos (
                    juce::MathConstants<double>::twoPi * static_cast<double> (index)
                    / static_cast<double> (Transform::size)));
                expectWithinAbsoluteError (spectrum[index].real(), expected, 2.0e-5f);
                expectWithinAbsoluteError (spectrum[index].imag(), 0.0f, 2.0e-5f);
            }
        }

        beginTest ("Inverse transform reconstructs a constant from DC");
        {
            using Transform = spektrummer::dsp::LockFreeFft<5>;
            Transform transform;
            std::array<std::complex<float>, Transform::size> spectrum {};
            spectrum[0] = { static_cast<float> (Transform::size), 0.0f };
            transform.inverse (spectrum);

            for (const auto sample : spectrum)
            {
                expectWithinAbsoluteError (sample.real(), 1.0f, 1.0e-6f);
                expectWithinAbsoluteError (sample.imag(), 0.0f, 1.0e-6f);
            }
        }
    }
};

LockFreeFftTests lockFreeFftTests;
}
