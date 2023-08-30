#include "jpegenc.h"
#include "jpeg_tables.h"
#include "common.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ios>
#include <iostream>
#include <string>
#include <vector>

float temp1[64];
float temp2[64];
std::vector<std::pair<uint8_t, int>> rle_results;

const uint8_t zigzag[] = {
	0,	1,	5,	6,	14,	15,	27,	28,
 	2,	4,	7,	13,	16,	26,	29,	42,
        3,	8,	12,	17,	25,	30,	41,	43,   
        9,	11,	18,	24,	31,	40,	44,	53,   
        10,	19,	23,	32,	39,	45,	52,	54,   
        20,	22,	33,	38,	46,	51,	55,	60,   
        21,	34,	37,	47,	50,	56,	59,	61,   
        35,	36,	48,	49,	57,	58,	62,	63,
};

template <typename T>
void zigzag_rearrange_8x8(T* input, T* output) {
  for (int i = 0; i < 64; i++) {
    int zigzag_i = zigzag[i];
    output[zigzag_i] = input[i];
  }
}

// DCT transform
void dct_8x8(const float* time_domain_input, float* freq_domain_output) {
  int i, j, u, v; // i, j: coord in time domain; u, v: coord in freq domain
  float s;

  for (u = 0; u < 8; u++) {
    for (v = 0; v < 8; v++) {
      s = 0;

      for (i = 0; i < 8; i++) {
        for (j = 0; j < 8; j++) {
          auto freq = (float)time_domain_input[i*8+j];
          auto basis1 = (float)cos((2.0*i+1.0)*u*M_PI/16.0);
          auto basis2 = (float)cos((2.0*j+1.0)*v*M_PI/16.0);
          float cu = u == 0.0 ? 1.0 / sqrt(2.0) : 1.0;
          float cv = v == 0.0 ? 1.0 / sqrt(2.0) : 1.0;
          s += freq * basis1 * basis2 * cu * cv;
        }
      }

      *(freq_domain_output+u*8+v) = s / 4.0;
    }
  }
}

JpegEncoder::JpegEncoder(const char* filename): file(filename) {
  assert(this->file.is_open() && "open file error");
  this->read_as_ppm();
  this->encode();
}

JpegEncoder::~JpegEncoder() {
    delete[] this->r;
    delete[] this->g;
    delete[] this->b;
}

void rgb2yuv(
  float r, float g, float b,
  float* y, float* cb, float* cr
) {
    *y =  0.299 * r + 0.587 * g + 0.114 * b - 128.0;
    *cb = -0.16874 * r - 0.33126 * g + 0.5 * b;
    *cr = 0.5 * r - 0.41869 * g - 0.08131 * b;
}

void JpegEncoder::read_as_ppm() {
    std::string header;
    size_t color_max;
    // >> operator can extract a value from stream, ignoring any preceding whitespaces
    this->file >> header >> this->w >> this->h >> color_max;
    assert(!memcmp(header.data(), "P6", 2) && "not a ppm binary file");
    assert(color_max == 255 && "max color not 255");

    this->r = new unsigned char[this->w*this->h];
    this->g = new unsigned char[this->w*this->h];
    this->b = new unsigned char[this->w*this->h];

    this->file.seekg(1, std::ios::cur);

    // unpack data into 3 buffers
    for (size_t i = 0; i < this->w*this->h;i++) {
        this->r[i] = this->file.get();
        this->g[i] = this->file.get();
        this->b[i] = this->file.get();
    }
}

void JpegEncoder::output_qt(bool is_chroma, const uint8_t* table) {
    printf("\xff\xdb");
    write_u16_be(67); // 64 + 1 + 2;
    printf("%c", is_chroma ? 1 : 0);
    fwrite(table, sizeof(unsigned char), 64, stdout);
}

// return ordered table symbols from table
std::string table_symbols(const huffman_table_t& input_table) {
    std::vector<huffman_table_entry_t> vec;
    for (huffman_table_entry_t entry: input_table) {
        vec.push_back({ entry.first, { entry.second.first, entry.second.second } });
    }
    sort(vec.begin(), vec.end(), [](huffman_table_entry_t a, huffman_table_entry_t b) {
        return a.second.second < b.second.second;
    });
    std::string output;
    for (huffman_table_entry_t entry: vec) {
        output += entry.first;
    }
    return output;
}

// get specification(coded bytes in file) of huffman table
std::string get_ht_spec(const huffman_table_t& table) {
    std::string code_lengths(16, 0);
    std::string symbols = table_symbols(table);
    for (auto [category, code_info]: table) {
        auto [code_length, code] = code_info;
        code_lengths[code_length - 1]++;
    }
    return code_lengths + symbols;
}


void JpegEncoder::output_ht(bool is_chroma, bool is_ac, const huffman_table_t& table) {
    printf("\xff\xc4");
    uint8_t dest = (is_ac << 4) | is_chroma;
    std::string ht_spec = get_ht_spec(table);
    write_u16_be(ht_spec.size() + 1 + 2); // 64 + 1 + 2;
    printf("%c", dest);
    std::cout << ht_spec;
    // fwrite(table, sizeof(unsigned char), 64, stdout);
}

void JpegEncoder::output_sof() {
    printf("\xff\xc0");
    write_u16_be(17); // for 3-component jpeg
    printf("\x08"); // 8-bit precision
    write_u16_be(this->h); // height first
    write_u16_be(this->w);
    printf("\x03"); // 3-component
    // 444 sampling
    fwrite("\x01\x11\x00", 1, 3, stdout); 
    fwrite("\x02\x11\x01", 1, 3, stdout);
    fwrite("\x03\x11\x01", 1, 3, stdout);
}

void JpegEncoder::output_sos() {
    printf("\xff\xda");
    write_u16_be(12); // for 3-component jpeg
    printf("\x03"); // 3-component
    // 444 sampling
    fwrite("\x01\x00", 1, 2, stdout); 
    printf("\x02\x11");
    printf("\x03\x11");
    fwrite("\x00\x3f\x00", 1, 3, stdout); // for baseline dct
}

void JpegEncoder::get_YCbCr_from_source(
    size_t x, size_t y, size_t w, size_t h, size_t stride,
    char* Y, char* Cb, char* Cr
) {
    float Yf, Cbf, Crf;
    for (size_t u = 0; u < h; u++) {
        for (size_t v = 0; v < w; v++) {
            size_t i = (y + u) * stride + x + v;
            uint8_t _r = this->r[i];
            uint8_t _g = this->g[i];
            uint8_t _b = this->b[i];
            size_t ii = u * w + v;
            rgb2yuv((float)_r, (float)_g, (float)_b, &Yf, &Cbf, &Crf);
            *(Y + ii) = round(Yf);
            *(Cb + ii) = round(Cbf);
            *(Cr + ii) = round(Crf);
        }
    }
}

// read from src, and save value to a 8x8 dst buffer
void fill_8x8(
    char* src, float* dst,
    uint8_t x,
    uint8_t y,
    uint8_t sample_h,
    uint8_t sample_v,
    size_t stride
) {
    for (size_t u = 0; u < 8; u++) {
        for (size_t v = 0; v < 8; v++) {
            size_t i = (y + u) * sample_v * stride + (x + v) * sample_h;
            size_t ii = u * 8 + v;
            *(dst + ii) = *(src + i);
        }
    }
}

void quantize_8x8(float* src, float* dst, const uint8_t* table) {
    for (size_t i = 0; i < 64; i++) {
        // round to integer
        dst[i] = round(src[i] / table[i]);
    }
}

inline uint8_t get_category(int coeff) {
    if (coeff == 0) return 0;
    else if (coeff == 1 || coeff == -1) return 1;
    return log2(abs(coeff)) + 1;
}

inline int get_bit(uint8_t category, int dc_diff) {
  int l = 1 << category;
  if (dc_diff < 0) {
    return dc_diff + l - 1;
  } else {
    return dc_diff;
  }
}

// ac coefficients as input, and output(array of rrrrssss) as output
void rle_63(float* input, std::vector<std::pair<uint8_t, int>>& output) {
    output.clear();
    uint8_t zero_count = 0;
    for (size_t i = 0; i < 63; i++) {
        if (input[i] == 0) {
            zero_count++;
        } else {
            output.push_back({zero_count, input[i]});
            zero_count = 0;
        }
    }
    // if any remaining zeros, add eob
    if (zero_count > 0) {
        output.push_back({ 0, 0 });
    }
}

int JpegEncoder::encode_8x8_per_component(
    char* src_buffer,
    size_t x, size_t y, size_t sample_h, size_t sample_v,
    bool is_chroma,
    int prev_dc
) {
    memset(temp1, 0, 64);
    memset(temp2, 0, 64);
    // 444 sampling
    uint8_t mcu_w = 8;
    fill_8x8(src_buffer, temp1, x, y, sample_h, sample_v, mcu_w);
    dct_8x8(temp1, temp2);
    quantize_8x8(temp2, temp1, is_chroma ? qt_chroma : qt_luma);
    zigzag_rearrange_8x8(temp1, temp2);
    // encode dc
    int dc_diff = temp2[0] - prev_dc;
    // when diff is zero, not need to append another bits
    if (dc_diff == 0) {
        auto dc_zero_diff_code_info = (is_chroma ? ht_chroma_dc : ht_luma_dc).at(0x00);
        this->bitstream.append_bit(dc_zero_diff_code_info.first, dc_zero_diff_code_info.second);
    } else {
        uint8_t category = get_category(dc_diff);
        uint8_t dc_bit = get_bit(category, dc_diff);
        
        auto category_code_info = (is_chroma ? ht_chroma_dc : ht_luma_dc).at(category);
        this->bitstream.append_bit(category_code_info.first, category_code_info.second);
        this->bitstream.append_bit(category, dc_bit);
    }

    // encode ac
    rle_63(temp2 + 1, rle_results);
    for (auto rle: rle_results) {
        auto [zero_length, ac_coeff] = rle;
        if (zero_length == 0 && ac_coeff == 0) {
            auto eob_code_info = (is_chroma ? ht_chroma_ac : ht_luma_ac).at(0x00);
            this->bitstream.append_bit(eob_code_info.first, eob_code_info.second);
            break;
        }

        while (zero_length > 15) {
            // 15, 0 is the zrl code
            auto zrl_code_info = (is_chroma ? ht_chroma_ac : ht_luma_ac).at(0xf0);
            this->bitstream.append_bit(zrl_code_info.first, zrl_code_info.second);
            zero_length -= 16;
        }
        uint8_t rrrr = zero_length;
        uint8_t ssss = get_category(ac_coeff);
        auto rle_code_info = (is_chroma ? ht_chroma_ac : ht_luma_ac).at((rrrr << 4) | ssss);
        uint8_t ac_bit = get_bit(ssss, ac_coeff);
        this->bitstream.append_bit(rle_code_info.first, rle_code_info.second);
        this->bitstream.append_bit(ssss, ac_bit);
    }
    return temp2[0];
}

void JpegEncoder::output_encoded_image_data() {
    // 444 sampling
    uint8_t mcu_w = 8, mcu_h = 8;

    char* Y_MCU = new char[mcu_w*mcu_h];
    char* Cb_MCU = new char[mcu_w*mcu_h];
    char* Cr_MCU = new char[mcu_w*mcu_h];

    int* temp1 = new int[8*8];
    int* temp2 = new int[8*8];

    std::vector<std::pair<uint8_t, int>> rle_results;

    int dc_y = 0;
    int dc_cr = 0;
    int dc_cb = 0;

    for (size_t mcu_y = 0; mcu_y < this->h / mcu_h; mcu_y++) {
        for (size_t mcu_x = 0; mcu_x < this->w / mcu_w; mcu_x++) {
            this->get_YCbCr_from_source(mcu_x * mcu_w, mcu_y * mcu_h, mcu_w, mcu_h, this->w, Y_MCU, Cb_MCU, Cr_MCU);

            // Y1 top left
            dc_y = this->encode_8x8_per_component(Y_MCU, 0, 0, 1, 1, false, dc_y);
            // Cb
            dc_cb = this->encode_8x8_per_component(Cb_MCU, 0, 0, 1, 1, true, dc_cb);
            // Cr
            dc_cr = this->encode_8x8_per_component(Cr_MCU, 0, 0, 1, 1, true, dc_cr);
        }
    }
    std::cout << this->bitstream.store;
}

void JpegEncoder::encode() {
    printf("\xff\xd8"); // start of image

    this->output_qt(false, qt_luma);
    this->output_qt(true, qt_chroma);

    this->output_ht(false, false, ht_luma_dc);
    this->output_ht(false, true, ht_luma_ac);
    this->output_ht(true, false, ht_chroma_dc);
    this->output_ht(true, true, ht_chroma_ac);
    this->output_sof();
    this->output_sos();
    this->output_encoded_image_data();
    printf("\xff\xd9"); // end of image
}
