#pragma once

#include <cstdint>

namespace spektrummer::dsp
{
struct SynthMidiEvent final
{
    enum class Type
    {
        none,
        noteOn,
        noteOff,
        allNotesOff,
        allSoundOff
    };

    [[nodiscard]] static SynthMidiEvent fromRawData (const std::uint8_t* data,
                                                      int numBytes) noexcept
    {
        if (data == nullptr || numBytes < 1)
            return {};

        const auto status = data[0];

        if (status < 0x80 || status >= 0xf0)
            return {};

        const auto messageType = static_cast<std::uint8_t> (status & 0xf0);
        const auto midiChannel = static_cast<int> (status & 0x0f) + 1;

        if ((messageType == 0x80 || messageType == 0x90)
            && numBytes >= 3
            && data[1] < 0x80
            && data[2] < 0x80)
        {
            const auto velocity = static_cast<float> (data[2]) / 127.0f;
            return {
                messageType == 0x90 && data[2] != 0 ? Type::noteOn : Type::noteOff,
                midiChannel,
                static_cast<int> (data[1]),
                velocity
            };
        }

        if (messageType == 0xb0 && numBytes >= 3 && data[1] < 0x80 && data[2] < 0x80)
        {
            if (data[1] == 120)
                return { Type::allSoundOff, midiChannel, 0, 0.0f };

            if (data[1] == 123)
                return { Type::allNotesOff, midiChannel, 0, 0.0f };
        }

        return {};
    }

    Type type = Type::none;
    int channel = 0;
    int note = 0;
    float velocity = 0.0f;
};
}
