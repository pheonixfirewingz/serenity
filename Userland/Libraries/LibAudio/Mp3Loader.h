/*
 * Copyright (c) 2021, Luke Shore <Lukethemodded@gmail.com>
 * Copyright (c) 2021, kleines Filmröllchen <malu.bertsch@gmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include "PluginCommon.h"
#include "Buffer.h"
#include "Loader.h"
#include "Mp3Types.h"
#include <AK/BitStream.h>
#include <AK/Stream.h>
#include <AK/Types.h>
#include <AK/Variant.h>
#include <LibCore/FileStream.h>


namespace Audio {
class Mp3LoaderPlugin final : public LoaderPlugin {
public:
    Mp3LoaderPlugin(const StringView& path);
    Mp3LoaderPlugin(const ByteBuffer& buffer);

    virtual bool sniff() override;
    virtual bool has_error() override { return m_error_string.is_empty(); }
    virtual const String& error_string() override { return m_error_string; }

    virtual RefPtr<Buffer> get_more_samples(size_t max_bytes_to_read_from_input = 128 * KiB) override
    {
        (void)max_bytes_to_read_from_input;
        return nullptr;
    }

    virtual void reset() override;
    virtual void seek(const int sample_index) override;

    virtual int loaded_samples() override { return 0; }
    virtual int total_samples() override { return 0; }
    virtual u32 sample_rate() override { return 0; }
    virtual u16 num_channels() override { return 0; }

    virtual PcmSampleFormat pcm_format() override { return m_sample_format; }
    virtual RefPtr<Core::File> file() override { return m_file; }

private:
    void decode_mp3();
    void parse_global_header();
    bool decode_frame();
    void decode_frame_data();
    void decode_side_info();
    void decode_v2();
    void decode_scale_factor(int gr,int ch);
    void decode_huffman_code(int gr,int ch);

    const int mix_partition_table[6][3][4] = {
        {{ 6,  5, 5, 5}, {3, 3, 3, 3}, {6, 3, 3, 3}},
        {{ 6,  5, 7, 3}, {3, 3, 4, 2}, {6, 3, 4, 2}},
        {{11, 10, 0, 0}, {6, 6, 0, 0}, {6, 3, 6, 0}},
        {{ 7,  7, 7, 0}, {4, 4, 4, 0}, {6, 5, 4, 0}},
        {{ 6,  6, 6, 3}, {4, 3, 3, 2}, {6, 4, 3, 2}},
        {{ 8,  8, 5, 0}, {5, 4, 3, 0}, {6, 6, 3, 0}}
    };

    MP3frame m_current_frame;

    RefPtr<Core::File> m_file;
    OwnPtr<AudioInputStream> m_memory;
    OwnPtr<InputBitStream> m_stream;
    String m_error_string;
    PcmSampleFormat m_sample_format;
    bool m_valid { false };
};
}