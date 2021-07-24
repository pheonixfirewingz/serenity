/*
 * Copyright (c) 2021, Luke Shore <Lukethemodded@gmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once
#include <AK/Types.h>

namespace Audio {
#define MPEGV1 3

union Mp3Header {
    u32 header;

    struct spit_header {
        u16 low;
        u16 high;
    } header_split;

    struct content {
        u16 sync_word : 12;
        u8 audio_id : 1;
        u8 layer : 2;
        u8 protection_bit : 1;
        u8 bitrate_index : 4;
        u8 sampling_frequency : 2;
        u8 padding_bit : 1;
        u8 private_bit : 1;
        u8 mode : 2;
        u8 mode_extension : 2;
        u8 copyright : 1;
        u8 original : 1;
        u8 emphasis : 2;
    } content;
};

struct MP3SideData {
    int part_two_three_length;
    int n_big_values;
    int global_gain;
    int sf_compress;
    int win_switch_flag;
    int block_type;
    int mixed_block;
    int table_select[3] {};
    int sub_block_gain[3] {};
    int region_zero_Count;
    int region_one_Count;
    int pre_flag;
    int s_fact_scale;
    int count_one_table_select;
};

struct MP3Side {
    int main_Data_begin;
    int private_bits;
    MP3SideData data[2][2] {};
};

struct MP3SFI {
    char l[23] {};
    char s[13][3] {};
};

struct MP3SF {
    int intensity_scale;
    int size_lengh[4] {};
    int nr[4] {};
};

struct MP3HuffmanCode {

};

struct MP3frame {
    Mp3Header frame_header;
    MP3Side side_info;
    MP3SFI sfi;
    MP3SF sf;
    MP3HuffmanCode huffmanCode;
};

}