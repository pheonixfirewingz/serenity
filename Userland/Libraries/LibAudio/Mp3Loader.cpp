/*
 * Copyright (c) 2018-2021, the SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */
#include "Mp3Loader.h"

namespace Audio {
Mp3LoaderPlugin::Mp3LoaderPlugin(const StringView& path)
    : m_file(Core::File::construct(path))
{
    if (!m_file->open(Core::OpenMode::ReadOnly)) {
        m_error_string = String::formatted("Can't open file: {}", m_file->error_string());
        return;
    }
    m_memory = make<AudioInputStream>(Core::InputFileStream(*m_file));
    m_stream = make<InputBitStream>(m_memory->bit_stream());
    decode_mp3();
}

Mp3LoaderPlugin::Mp3LoaderPlugin(const ByteBuffer& buffer)
{
    m_memory = make<AudioInputStream>(InputMemoryStream(buffer));
    m_stream = make<InputBitStream>(m_memory->bit_stream());
    if (!m_stream) {
        m_error_string = String::formatted("Can't open memory stream");
        return;
    }
    decode_mp3();
}

bool Mp3LoaderPlugin::sniff()
{
    return m_valid;
}

void Mp3LoaderPlugin::seek(const int sample_index)
{
    (void)sample_index;
}

void Mp3LoaderPlugin::reset()
{
}

void Mp3LoaderPlugin::parse_global_header()
{
    // Magic number
    u32 mp3 = m_stream->read_bits_big_endian(24);
    (void)m_stream->read_bits_big_endian(8);
    if (mp3 == 0x494433) {
        m_valid = true;
        //we need to skip the meta data and find first frame
        while (true) {
            m_current_frame.frame_header.header = m_stream->read_bits_big_endian(32);
            if (m_current_frame.frame_header.header_split.high == 0xFFFB)
                break;
        }
        if (m_current_frame.frame_header.content.protection_bit == 0)
                TODO();
        decode_side_info();
        decode_frame_data();
        dbgln("MP3: decoded first frame");
        TODO();
    } else
        m_valid = false;
}

void Mp3LoaderPlugin::decode_mp3()
{
    parse_global_header();
    if (!m_valid)
        return;
    //while (true)
    //if (!decode_frame())
    //break;
}

bool Mp3LoaderPlugin::decode_frame()
{
    m_current_frame.frame_header.header = m_stream->read_bits_big_endian(32);
    if (m_current_frame.frame_header.header_split.high == 0xFFFB)
        return false;
    if (m_current_frame.frame_header.content.protection_bit == 0) {
        outln("TODO: MP3 handling protected bit not implemented");
        TODO();
    }

    decode_side_info();
    decode_frame_data();
    auto samples = convert_current_mp3frame_to_samples();
    samples_cache.extend(samples);
    return true;
}

void Mp3LoaderPlugin::decode_frame_data()
{
    switch (m_current_frame.frame_header.content.layer) {
    case 0:
        //FIXME: what to do when reserved
        outln("TODO: MP3 layers bit set as reserved not implemented");
        TODO();
        break;
    case 1:
        //FIXME: add verstion 1 decodeing
        outln("TODO: MP3 V1 decoding not implemented");
        TODO();
        break;
    case 2:
    case 3:
        decode_v2();
        break;
    default:
        outln("ERROR: MP3 invaled layer id");
        VERIFY_NOT_REACHED();
    }
}

void Mp3LoaderPlugin::decode_side_info()
{
    if (m_current_frame.frame_header.content.audio_id == MPEGV1) {
        m_current_frame.side_info.main_Data_begin = m_stream->read_bits_big_endian(9);
        m_current_frame.side_info.private_bits = m_stream->read_bits_big_endian((m_current_frame.frame_header.content.mode == 3 ? 5 : 3));
        //fixme: there is something missing but idk what the spec is saying
        TODO();
    } else {
        m_current_frame.side_info.main_Data_begin = m_stream->read_bits_big_endian(8);
        m_current_frame.side_info.private_bits = m_stream->read_bits_big_endian((m_current_frame.frame_header.content.mode == 3 ? 1 : 2));
    }

    for (int gr = 0; gr < (m_current_frame.frame_header.content.audio_id == MPEGV1 ? 2 : 1); gr++) {
        for (int ch = 0; ch < (m_current_frame.frame_header.content.mode == 3 ? 1 : 2); ch++) {
            auto* sis = &m_current_frame.side_info.data[gr][ch];
            sis->part_two_three_length = m_stream->read_bits_big_endian(12);
            sis->n_big_values = m_stream->read_bits_big_endian(9);
            sis->global_gain = m_stream->read_bits_big_endian(8);
            sis->sf_compress = m_stream->read_bits_big_endian((m_current_frame.frame_header.content.audio_id == MPEGV1 ? 4 : 9));
            sis->win_switch_flag = m_stream->read_bits_big_endian(1);
            if (sis->win_switch_flag) {
                sis->block_type = m_stream->read_bits_big_endian(2);
                sis->mixed_block = m_stream->read_bits_big_endian(1);
                sis->table_select[0] = m_stream->read_bits_big_endian(5);
                sis->table_select[1] = m_stream->read_bits_big_endian(5);
                sis->table_select[2] = 0; /* unused */
                sis->sub_block_gain[0] = m_stream->read_bits_big_endian(3);
                sis->sub_block_gain[1] = m_stream->read_bits_big_endian(3);
                sis->sub_block_gain[2] = m_stream->read_bits_big_endian(3);
                if (sis->block_type == 0) {
                    /* this should not be allowed, according to spec */
                    sis->n_big_values = 0;
                    sis->part_two_three_length = 0;
                    sis->sf_compress = 0;
                } else if (sis->block_type == 2 && sis->mixed_block == 0) {
                    /* short block, not mixed */
                    sis->region_zero_Count = 8;
                } else {
                    /* start, stop, or short-mixed */
                    sis->region_zero_Count = 7;
                }
                sis->region_one_Count = 20 - sis->region_zero_Count;
            } else {
                /* this is a normal block */
                sis->block_type = 0;
                sis->mixed_block = 0;
                sis->table_select[0] = m_stream->read_bits_big_endian(5);
                sis->table_select[1] = m_stream->read_bits_big_endian(5);
                sis->table_select[2] = m_stream->read_bits_big_endian(5);
                sis->region_zero_Count = m_stream->read_bits_big_endian(4);
                sis->region_one_Count = m_stream->read_bits_big_endian(3);
            }
            sis->pre_flag = (m_current_frame.frame_header.content.audio_id == MPEGV1 ? m_stream->read_bits_big_endian(1) : 0);
            sis->s_fact_scale = m_stream->read_bits_big_endian(1);
            sis->count_one_table_select = m_stream->read_bits_big_endian(1);
        }
    }
}

void Mp3LoaderPlugin::decode_v2()
{
    for (int gr = 0; gr < (m_current_frame.frame_header.content.audio_id == MPEGV1 ? 2 : 1); gr++) {
        for (int ch = 0; ch < (m_current_frame.frame_header.content.mode == 3 ? 1 : 2); ch++) {
            decode_scale_factor(gr, ch);
            decode_huffman_code(gr, ch);
        }
    }
}

void Mp3LoaderPlugin::decode_scale_factor(int gr, int ch)
{
    int sfcIdx = 0;
    int size_lenght[4] { 0, 0, 0, 0 }, nr[4] { 0, 0, 0, 0 };
    int preFlag = 0, intensity_scale = 0;

    if (!((m_current_frame.frame_header.content.mode_extension & 0x01) && (ch == 1))) {
        int sf_comp = m_current_frame.side_info.data[gr][ch].sf_compress;
        if (m_current_frame.side_info.data[gr][ch].sf_compress < 400) {
            size_lenght[0] = (sf_comp >> 4) / 5;
            size_lenght[1] = (sf_comp >> 4) % 5;
            size_lenght[2] = (sf_comp & 0x0f) >> 2;
            size_lenght[3] = (sf_comp & 0x03);
        } else if (m_current_frame.side_info.data[gr][ch].sf_compress < 500) {
            sf_comp -= 400;
            size_lenght[0] = (sf_comp >> 2) / 5;
            size_lenght[1] = (sf_comp >> 2) % 5;
            size_lenght[2] = (sf_comp & 0x03);
            sfcIdx = 1;
        } else {
            sf_comp -= 500;
            size_lenght[0] = sf_comp / 3;
            size_lenght[1] = sf_comp % 3;
            if (m_current_frame.side_info.data[gr][ch].mixed_block) {
                size_lenght[2] = size_lenght[1];
                size_lenght[1] = size_lenght[0];
            }
            preFlag = 1;
            sfcIdx = 2;
        }
    } else {
        intensity_scale = m_current_frame.side_info.data[gr][ch].sf_compress & 0x01;
        int sf_comp = m_current_frame.side_info.data[gr][ch].sf_compress;
        sf_comp >>= 1;
        if (sf_comp < 180) {
            size_lenght[0] = (sf_comp / 36);
            size_lenght[1] = (sf_comp % 36) / 6;
            size_lenght[2] = (sf_comp % 36) % 6;
            sfcIdx = 3;
        } else if (sf_comp < 244) {
            sf_comp -= 180;
            size_lenght[0] = (sf_comp & 0x3f) >> 4;
            size_lenght[1] = (sf_comp & 0x0f) >> 2;
            size_lenght[2] = (sf_comp & 0x03);
            sfcIdx = 4;
        } else {
            sf_comp -= 244;
            size_lenght[0] = (sf_comp / 3);
            size_lenght[1] = (sf_comp % 3);
            sfcIdx = 5;
        }
    }
    int btIdx = 0;

    if (m_current_frame.side_info.data[gr][ch].block_type == 2)
        btIdx = (m_current_frame.side_info.data[gr][ch].mixed_block ? 2 : 1);

    for (int i = 0; i < 4; i++)
        nr[i] = mix_partition_table[sfcIdx][btIdx][i];

    if ((m_current_frame.frame_header.content.mode_extension & 0x01) && (ch == 1)) {
        for (int i = 0; i < 4; i++) {
            m_current_frame.sf.size_lengh[i] = size_lenght[i];
            m_current_frame.sf.nr[i] = nr[i];
        }
        m_current_frame.sf.intensity_scale = intensity_scale;
    }
    m_current_frame.side_info.data[gr][ch].pre_flag = preFlag;

    int index = 0, sfb = 0;
    if (m_current_frame.side_info.data[gr][ch].block_type == 2) {
        if (m_current_frame.side_info.data[gr][ch].mixed_block) {
            for (; sfb < 6; sfb++)
                m_current_frame.sfi.l[sfb] = m_stream->read_bits_big_endian(size_lenght[0]);

            sfb = 3;
            index = 1;
        }

        for (; index <= 3; index++) {
            for (int h = 0; h < nr[index]; h++, sfb++) {
                m_current_frame.sfi.s[sfb][0] = m_stream->read_bits_big_endian(size_lenght[index]);
                m_current_frame.sfi.s[sfb][1] = m_stream->read_bits_big_endian(size_lenght[index]);
                m_current_frame.sfi.s[sfb][2] = m_stream->read_bits_big_endian(size_lenght[index]);
            }
        }
        m_current_frame.sfi.s[12][0] = m_current_frame.sfi.s[12][1] = m_current_frame.sfi.s[12][2] = 0;
    } else {
        for (; index <= 3; index++) {
            for (int i = 0; i < nr[index]; i++, sfb++)
                m_current_frame.sfi.l[sfb] = m_stream->read_bits_big_endian(size_lenght[index]);
        }
        m_current_frame.sfi.l[21] = m_current_frame.sfi.l[22] = 0;
    }
}

void Mp3LoaderPlugin::decode_huffman_code(int gr, int ch)
{
    (void)gr;
    (void)ch;
    TODO();
}

Vector<i32> Mp3LoaderPlugin::convert_current_mp3frame_to_samples()
{
    return Vector<i32>();
}

};