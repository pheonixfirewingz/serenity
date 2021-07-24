#pragma once

#include <AK/BitStream.h>
#include <AK/Stream.h>
#include <AK/Types.h>
#include <AK/Variant.h>
#include <LibCore/FileStream.h>
#include <AK/MemoryStream.h>

namespace Audio {

class AudioInputStream : public Variant<Core::InputFileStream, InputMemoryStream> {

public:
    using Variant<Core::InputFileStream, InputMemoryStream>::Variant;

    void seek(size_t pos)
    {
        this->visit(
            [&](auto& stream) {
                stream.seek(pos);
            });
    }

    InputBitStream bit_stream()
    {
        return this->visit(
            [&](auto& stream) {
                return InputBitStream(stream);
            });
    }
};

}