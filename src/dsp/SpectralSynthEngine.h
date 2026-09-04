#pragma once

#include "LockFreeFft.h"

#include <array>
#include <complex>
#include <cstdint>
#include <memory>

namespace spektrummer::dsp
{
class SpectralSynthEngine final
{
public:
    static constexpr int fftOrder = 11;
    static constexpr int fftSize = 1 << fftOrder;
    static constexpr int hopSize = fftSize / 4;
    static constexpr int maximumVoiceCapacity = 16;
    static constexpr double releaseSeconds = 0.080;

    void prepare (double newSampleRate, int maximumBlockSize) noexcept;
    void reset() noexcept;

    void setMaximumVoices (int requestedVoices) noexcept;
    [[nodiscard]] int getMaximumVoices() const noexcept;
    [[nodiscard]] int getActiveVoiceCount() const noexcept;
    [[nodiscard]] bool isNoteActive (int midiChannel, int midiNote) const noexcept;

    void noteOn (int midiChannel, int midiNote, float velocity) noexcept;
    void noteOff (int midiChannel, int midiNote) noexcept;
    void allNotesOff (int midiChannel = 0) noexcept;
    void allSoundOff (int midiChannel = 0) noexcept;

    void render (float* output, int numSamples) noexcept;

private:
    static constexpr int harmonicCount = 8;
    static constexpr int kernelRadius = 16;
    static constexpr int maximumKernelBins = kernelRadius * 2 + 2;

    struct KernelBin
    {
        int bin = 0;
        std::complex<float> positiveResponse {};
        std::complex<float> negativeResponse {};
    };

    struct PartialKernel
    {
        double frequency = 0.0;
        int numBins = 0;
        std::array<KernelBin, maximumKernelBins> bins {};
    };

    using NoteKernelTable = std::array<std::array<PartialKernel, harmonicCount>, 128>;

    enum class EnvelopeStage
    {
        inactive,
        sustain,
        release
    };

    struct Voice
    {
        int channel = 0;
        int note = 0;
        float velocity = 0.0f;
        float envelope = 0.0f;
        std::uint64_t age = 0;
        EnvelopeStage stage = EnvelopeStage::inactive;
        std::array<double, harmonicCount> phases {};
    };

    [[nodiscard]] static int normaliseVoiceLimit (int requestedVoices) noexcept;
    [[nodiscard]] Voice* findMatchingVoice (int midiChannel, int midiNote) noexcept;
    [[nodiscard]] Voice* findVoiceForNoteOn() noexcept;
    void enforceVoiceLimit() noexcept;
    void advanceEnvelope (Voice& voice) noexcept;
    void prepareSpectralKernels() noexcept;
    [[nodiscard]] static std::complex<double> dirichletKernel (double binOffset) noexcept;
    [[nodiscard]] static std::complex<double> hannKernel (double binOffset) noexcept;
    void addPartialToSpectrum (const PartialKernel& kernel,
                               double phase,
                               float amplitude) noexcept;
    void generateFrame() noexcept;

    LockFreeFft<fftOrder> inverseFft;
    std::array<Voice, maximumVoiceCapacity> voices {};
    std::unique_ptr<NoteKernelTable> noteKernels = std::make_unique<NoteKernelTable>();
    std::array<std::complex<float>, fftSize> spectrumData {};
    std::array<float, fftSize> overlapBuffer {};

    double sampleRate = 44100.0;
    std::uint64_t nextVoiceAge = 0;
    int maximumVoices = 8;
    int outputIndex = 0;
    int samplesUntilNextFrame = 0;
};
}
