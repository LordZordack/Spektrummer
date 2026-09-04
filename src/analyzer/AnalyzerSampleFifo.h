#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

namespace spektrummer::analyzer
{
class AnalyzerSampleFifo final
{
public:
    static constexpr std::size_t capacity = 32768;

    int push (const float* source, int numSamples) noexcept
    {
        if (source == nullptr || numSamples <= 0)
            return 0;

        const auto write = writePosition.load (std::memory_order_relaxed);
        const auto read = readPosition.load (std::memory_order_acquire);
        const auto occupied = static_cast<std::size_t> (write - read);
        const auto available = capacity - std::min (capacity, occupied);
        const auto requested = static_cast<std::size_t> (numSamples);
        const auto toWrite = std::min (requested, available);

        for (auto index = std::size_t {}; index < toWrite; ++index)
            samples[(static_cast<std::size_t> (write) + index) % capacity] = source[index];

        writePosition.store (write + static_cast<std::uint64_t> (toWrite), std::memory_order_release);
        droppedSamples.fetch_add (static_cast<std::uint64_t> (requested - toWrite),
                                  std::memory_order_relaxed);
        return static_cast<int> (toWrite);
    }

    int pop (float* destination, int maximumSamples) noexcept
    {
        if (destination == nullptr || maximumSamples <= 0)
            return 0;

        const auto read = readPosition.load (std::memory_order_relaxed);
        const auto write = writePosition.load (std::memory_order_acquire);
        const auto ready = static_cast<std::size_t> (write - read);
        const auto toRead = std::min (static_cast<std::size_t> (maximumSamples), ready);

        for (auto index = std::size_t {}; index < toRead; ++index)
            destination[index] = samples[(static_cast<std::size_t> (read) + index) % capacity];

        readPosition.store (read + static_cast<std::uint64_t> (toRead), std::memory_order_release);
        return static_cast<int> (toRead);
    }

    [[nodiscard]] int getNumReady() const noexcept
    {
        const auto write = writePosition.load (std::memory_order_acquire);
        const auto read = readPosition.load (std::memory_order_acquire);
        return static_cast<int> (std::min (capacity, static_cast<std::size_t> (write - read)));
    }

    [[nodiscard]] std::uint64_t getDroppedSampleCount() const noexcept
    {
        return droppedSamples.load (std::memory_order_relaxed);
    }

    void beginNewGeneration() noexcept
    {
        generation.fetch_add (1, std::memory_order_acq_rel);
    }

    [[nodiscard]] std::uint64_t getGeneration() const noexcept
    {
        return generation.load (std::memory_order_acquire);
    }

    void discardAll() noexcept
    {
        readPosition.store (writePosition.load (std::memory_order_acquire),
                            std::memory_order_release);
    }

private:
    std::array<float, capacity> samples {};
    std::atomic<std::uint64_t> writePosition { 0 };
    std::atomic<std::uint64_t> readPosition { 0 };
    std::atomic<std::uint64_t> droppedSamples { 0 };
    std::atomic<std::uint64_t> generation { 1 };
};
}
