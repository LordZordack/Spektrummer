#pragma once

#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <numbers>
#include <utility>

namespace spektrummer::dsp
{
template <int Order>
class LockFreeFft final
{
public:
    static_assert (Order > 0);
    static constexpr std::size_t size = std::size_t { 1 } << Order;
    using Complex = std::complex<float>;

    LockFreeFft() noexcept
    {
        for (auto index = std::size_t {}; index < size; ++index)
            bitReversed[index] = reverseBits (index);

        for (auto index = std::size_t {}; index < size / 2; ++index)
        {
            const auto phase = 2.0 * std::numbers::pi
                             * static_cast<double> (index)
                             / static_cast<double> (size);
            inverseTwiddles[index] = {
                static_cast<float> (std::cos (phase)),
                static_cast<float> (std::sin (phase))
            };
        }
    }

    void inverse (std::array<Complex, size>& data) const noexcept
    {
        for (auto index = std::size_t {}; index < size; ++index)
        {
            const auto reversed = bitReversed[index];

            if (reversed > index)
                std::swap (data[index], data[reversed]);
        }

        for (auto length = std::size_t { 2 }; length <= size; length <<= 1)
        {
            const auto halfLength = length / 2;
            const auto twiddleStep = size / length;

            for (auto block = std::size_t {}; block < size; block += length)
                for (auto offset = std::size_t {}; offset < halfLength; ++offset)
                {
                    const auto even = data[block + offset];
                    const auto odd = data[block + offset + halfLength]
                                   * inverseTwiddles[offset * twiddleStep];
                    data[block + offset] = even + odd;
                    data[block + offset + halfLength] = even - odd;
                }
        }

        const auto scale = 1.0f / static_cast<float> (size);
        for (auto& value : data)
            value *= scale;
    }

private:
    [[nodiscard]] static constexpr std::size_t reverseBits (std::size_t value) noexcept
    {
        auto reversed = std::size_t {};

        for (auto bit = 0; bit < Order; ++bit)
        {
            reversed = (reversed << 1) | (value & 1U);
            value >>= 1;
        }

        return reversed;
    }

    std::array<std::size_t, size> bitReversed {};
    std::array<Complex, size / 2> inverseTwiddles {};
};
}
