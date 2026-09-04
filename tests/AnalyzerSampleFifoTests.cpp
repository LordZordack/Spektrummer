#include <JuceHeader.h>

#include <array>

#include "analyzer/AnalyzerSampleFifo.h"

namespace
{
using spektrummer::analyzer::AnalyzerSampleFifo;

class AnalyzerSampleFifoTests final : public juce::UnitTest
{
public:
    AnalyzerSampleFifoTests() : juce::UnitTest ("Analyzer sample FIFO") {}

    void runTest() override
    {
        beginTest ("Samples retain order through partial reads and wraparound");
        {
            AnalyzerSampleFifo fifo;
            std::array<float, AnalyzerSampleFifo::capacity> first {};

            for (auto index = std::size_t {}; index < first.size(); ++index)
                first[index] = static_cast<float> (index);

            expectEquals (fifo.push (first.data(), static_cast<int> (first.size())),
                          static_cast<int> (first.size()));

            std::array<float, 257> partial {};
            expectEquals (fifo.pop (partial.data(), static_cast<int> (partial.size())),
                          static_cast<int> (partial.size()));

            for (auto index = std::size_t {}; index < partial.size(); ++index)
                expectEquals (partial[index], static_cast<float> (index));

            std::array<float, 257> wrapped {};
            for (auto index = std::size_t {}; index < wrapped.size(); ++index)
                wrapped[index] = static_cast<float> (first.size() + index);

            expectEquals (fifo.push (wrapped.data(), static_cast<int> (wrapped.size())),
                          static_cast<int> (wrapped.size()));
            expectEquals (fifo.getNumReady(), static_cast<int> (first.size()));

            std::array<float, AnalyzerSampleFifo::capacity> output {};
            expectEquals (fifo.pop (output.data(), static_cast<int> (output.size())),
                          static_cast<int> (output.size()));

            for (auto index = std::size_t {}; index < output.size(); ++index)
                expectEquals (output[index], static_cast<float> (index + partial.size()));
        }

        beginTest ("Overflow drops newest samples and records the exact count");
        {
            AnalyzerSampleFifo fifo;
            std::array<float, AnalyzerSampleFifo::capacity + 11> input {};
            expectEquals (fifo.push (input.data(), static_cast<int> (input.size())),
                          static_cast<int> (AnalyzerSampleFifo::capacity));
            expectEquals (fifo.getNumReady(), static_cast<int> (AnalyzerSampleFifo::capacity));
            expectEquals (fifo.getDroppedSampleCount(), std::uint64_t { 11 });
            expectEquals (fifo.push (input.data(), 1), 0);
            expectEquals (fifo.getDroppedSampleCount(), std::uint64_t { 12 });
        }

        beginTest ("Empty and invalid operations are harmless");
        {
            AnalyzerSampleFifo fifo;
            float sample = 1.0f;
            expectEquals (fifo.pop (&sample, 1), 0);
            expectEquals (fifo.push (nullptr, 1), 0);
            expectEquals (fifo.pop (nullptr, 1), 0);
            expectEquals (fifo.push (&sample, 0), 0);
            expectEquals (fifo.pop (&sample, 0), 0);
        }

        beginTest ("A generation change lets the consumer discard stale samples");
        {
            AnalyzerSampleFifo fifo;
            const std::array oldSamples { 1.0f, 2.0f, 3.0f };
            const std::array newSamples { 4.0f, 5.0f };
            const auto initialGeneration = fifo.getGeneration();
            fifo.push (oldSamples.data(), static_cast<int> (oldSamples.size()));
            fifo.beginNewGeneration();
            expect (fifo.getGeneration() > initialGeneration);
            fifo.discardAll();
            fifo.push (newSamples.data(), static_cast<int> (newSamples.size()));

            std::array<float, 2> output {};
            expectEquals (fifo.pop (output.data(), static_cast<int> (output.size())), 2);
            expectEquals (output[0], 4.0f);
            expectEquals (output[1], 5.0f);
        }
    }
};

AnalyzerSampleFifoTests analyzerSampleFifoTests;
}
