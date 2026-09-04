#include <JuceHeader.h>

#include <array>

#include "dsp/SynthMidiEvent.h"

namespace
{
using spektrummer::dsp::SynthMidiEvent;

class SynthMidiEventTests final : public juce::UnitTest
{
public:
    SynthMidiEventTests() : juce::UnitTest ("Non-owning synth MIDI parser") {}

    void runTest() override
    {
        beginTest ("Note messages are parsed without owning storage");
        {
            const std::array<juce::uint8, 3> noteOn { 0x92, 60, 96 };
            const auto on = SynthMidiEvent::fromRawData (noteOn.data(), 3);
            expect (on.type == SynthMidiEvent::Type::noteOn);
            expectEquals (on.channel, 3);
            expectEquals (on.note, 60);
            expectWithinAbsoluteError (on.velocity, 96.0f / 127.0f, 1.0e-6f);

            const std::array<juce::uint8, 3> zeroVelocity { 0x92, 60, 0 };
            expect (SynthMidiEvent::fromRawData (zeroVelocity.data(), 3).type
                    == SynthMidiEvent::Type::noteOff);

            const std::array<juce::uint8, 3> noteOff { 0x81, 62, 64 };
            const auto off = SynthMidiEvent::fromRawData (noteOff.data(), 3);
            expect (off.type == SynthMidiEvent::Type::noteOff);
            expectEquals (off.channel, 2);
            expectEquals (off.note, 62);
        }

        beginTest ("Supported channel controllers are distinguished");
        {
            const std::array<juce::uint8, 3> allSoundOff { 0xb4, 120, 0 };
            const std::array<juce::uint8, 3> allNotesOff { 0xbf, 123, 0 };
            expect (SynthMidiEvent::fromRawData (allSoundOff.data(), 3).type
                    == SynthMidiEvent::Type::allSoundOff);
            expect (SynthMidiEvent::fromRawData (allNotesOff.data(), 3).type
                    == SynthMidiEvent::Type::allNotesOff);
        }

        beginTest ("Long system messages and malformed data are ignored");
        {
            const std::array<juce::uint8, 8> systemExclusive { 0xf0, 1, 2, 3, 4, 5, 6, 0xf7 };
            expect (SynthMidiEvent::fromRawData (systemExclusive.data(),
                                                static_cast<int> (systemExclusive.size())).type
                    == SynthMidiEvent::Type::none);
            expect (SynthMidiEvent::fromRawData (nullptr, 3).type == SynthMidiEvent::Type::none);
            expect (SynthMidiEvent::fromRawData (systemExclusive.data(), 1).type
                    == SynthMidiEvent::Type::none);
        }
    }
};

SynthMidiEventTests synthMidiEventTests;
}
