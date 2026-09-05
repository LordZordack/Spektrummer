#pragma once

#include <JuceHeader.h>

#include "analyzer/AnalyzerSampleFifo.h"
#include "dsp/SpectralSynthEngine.h"

class SpektrummerAudioProcessor final : public juce::AudioProcessor
{
public:
    SpektrummerAudioProcessor();
    ~SpektrummerAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void reset() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    [[nodiscard]] juce::AudioProcessorEditor* createEditor() override;
    [[nodiscard]] bool hasEditor() const override;
    [[nodiscard]] const juce::String getName() const override;
    [[nodiscard]] bool acceptsMidi() const override;
    [[nodiscard]] bool producesMidi() const override;
    [[nodiscard]] bool isMidiEffect() const override;
    [[nodiscard]] double getTailLengthSeconds() const override;
    [[nodiscard]] int getNumPrograms() override;
    [[nodiscard]] int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    [[nodiscard]] const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;
    void getStateInformation (juce::MemoryBlock& destinationData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    [[nodiscard]] juce::AudioProcessorValueTreeState& getValueTreeState() noexcept;
    [[nodiscard]] const juce::AudioProcessorValueTreeState& getValueTreeState() const noexcept;
    [[nodiscard]] spektrummer::analyzer::AnalyzerSampleFifo& getAnalyzerSampleFifo() noexcept;

private:
    [[nodiscard]] static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    [[nodiscard]] float getRequestedOutputGain() const noexcept;
    [[nodiscard]] int getRequestedMaximumVoices() const noexcept;
    [[nodiscard]] spektrummer::dsp::SpectralSynthEngine::EnvelopeParameters
        getRequestedEnvelopeParameters() const noexcept;

    juce::AudioProcessorValueTreeState valueTreeState;
    std::atomic<float>* outputLevelParameter = nullptr;
    std::atomic<float>* maximumVoicesParameter = nullptr;
    std::atomic<float>* attackParameter = nullptr;
    std::atomic<float>* decayParameter = nullptr;
    std::atomic<float>* sustainParameter = nullptr;
    std::atomic<float>* releaseParameter = nullptr;
    spektrummer::dsp::SpectralSynthEngine spectralSynth;
    spektrummer::analyzer::AnalyzerSampleFifo analyzerSampleFifo;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> outputGain;
    double currentSampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpektrummerAudioProcessor)
};
