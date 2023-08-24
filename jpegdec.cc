#include "jpegdec.h"
#include "common.h"
#include <algorithm>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <ostream>
#include <utility>
#include <vector>
#include "huffman.h"

char buf[1024];

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
void zigzag_rearrange_8x8(T* buf) {
  T* temp = new T[64];
  memcpy(temp, buf, 64*sizeof(T));
  for (int u = 0; u < 8; u++) {
    for (int v = 0; v < 8; v++) {
      // int zigzag_i = u * 8 + v;
      int i = u*8+v;
      int zigzag_i = zigzag[i];
      buf[i] = temp[zigzag_i];
    }
  }
  delete[] temp;
}

// inverse DCT transform
void idct_8x8(const double in[64], double out[64])
{
  int i, j, u, v; // i, j: coord in time domain; u, v: coord in freq domain
  double s;

  for (i = 0; i < 8; i++)
    for (j = 0; j < 8; j++)
    {
      s = 0;

      for (u = 0; u < 8; u++)
        for (v = 0; v < 8; v++)
          s += in[u*8+v] * cos((2 * i + 1) * u * M_PI / 16) *
                          cos((2 * j + 1) * v * M_PI / 16) *
               ((u == 0) ? 1 / sqrt(2) : 1.) *
               ((v == 0) ? 1 / sqrt(2) : 1.);

      out[i*8+j] = s / 4;
    }
}

JpegDecoder::JpegDecoder(const char* filename):
  file(filename), bit_offset(0)
{
  assert(this->file.is_open() && "open file error");
  this->reset_segments();
  this->get_segments();

  this->handle_dqts();
  this->handle_sos0();
  this->handle_hufs();

  // for (auto table: tables) {
  //   if (table.is_ac()) {
  //     if (table.is_chroma()) {
  //       this->ht_ac_chroma = table;
  //     } else {
  //       this->ht_ac_luma = table;
  //     }
  //   } else {
  //     if (table.is_chroma()) {
  //       this->ht_dc_chroma = table;
  //     } else {
  //       this->ht_dc_luma = table;
  //     }
  //   }
  // }
  char byte, cur, next;
  // move to end of "start of scan" data part
  this->file.seekg(this->segments[segment_t::SOS][0].offset + this->segments[segment_t::SOS][0].length, ios::beg);

  while(true) {
    cur = this->file.peek();
    if (cur == '\xff') {
      this->file.seekg(1, ios::cur);
      next = this->file.peek();
      // skip ff00
      if (next == '\x00') {
        this->file.seekg(1, ios::cur);
        // end of image
      } else if (next == '\xd9') {
        break;
      } else {
        this->file.seekg(-1, ios::cur);
        this->file.read(&byte, 1);
        this->bitstream += byte;
      }
    } else {
      this->file.read(&byte, 1);
      this->bitstream += byte;
    }
  }

  // this->parse_context();
}

void JpegDecoder::reset_segments() {
  this->segments[segment_t::APP0] = vector<segment_info_t>();
  this->segments[segment_t::SOF0] = vector<segment_info_t>();
  this->segments[segment_t::SOS] = vector<segment_info_t>();
  this->segments[segment_t::DQT] = vector<segment_info_t>();
  this->segments[segment_t::DHT] = vector<segment_info_t>();
}

// get image width, height etc..
void JpegDecoder::handle_sos0() {
  segment_info_t info = this->segments[segment_t::SOF0][0]; // only one sos0 segment
  auto [offset, length] = info;
  this->file.seekg(offset, ios::beg); // skip section length
  read_assert_str_equal(this->file, "\x08", "data precision not 8");
  this->h = read_u16_be(this->file);
  this->w = read_u16_be(this->file);
  read_assert_str_equal(this->file, "\x03", "image component not 3");
  this->file.read((char*)&this->fc[0], 9);

  // printf("%d\n", this->fc[0].id);
  // printf("%d\n", this->fc[0].sampling_factor_packed.vertical);
  // printf("%d\n", this->fc[0].sampling_factor_packed.horizontal);
  // printf("%d\n", this->fc[0].quantization_mapping);
  // printf("%d\n", this->fc[1].id);
  // printf("%d\n", this->fc[1].sampling_factor_packed.vertical);
  // printf("%d\n", this->fc[1].sampling_factor_packed.horizontal);
  // printf("%d\n", this->fc[1].quantization_mapping);
  // printf("%d\n", this->fc[2].id);
  // printf("%d\n", this->fc[2].sampling_factor_packed.vertical);
  // printf("%d\n", this->fc[2].sampling_factor_packed.horizontal);
  // printf("%d\n", this->fc[2].quantization_mapping);
}

// handle each huffman table
void JpegDecoder::handle_huffman(int offset, int length) {
  this->file.seekg(offset, ios::beg);
  char ht_info; // 1 byte of packed huffman table info
  this->file.read(&ht_info, 1);

  char nb_sym[16];
  this->file.read(nb_sym, 16);
  int sum = 0;
  for (int i = 0; i < 16; i++) {
    sum += nb_sym[i];
  }
  char* symbols = new char[sum];

  this->file.read(symbols, sum);
  HuffmanTree tree(nb_sym, symbols);

  bool is_chroma = !!(ht_info & 1);
  bool is_ac = !!((ht_info & 0b10000) >> 4);

  if (is_ac) {
    if (is_chroma) {
      tree.fill_table(this->ht_ac_chroma);
    } else {
      tree.fill_table(this->ht_ac_luma);
    }
  } else {
    if (is_chroma) {
      tree.fill_table(this->ht_dc_chroma);
    } else {
      tree.fill_table(this->ht_dc_luma);
    }
  }


  delete[] symbols;
}

// get 4 huffman tables
void JpegDecoder::handle_hufs() {
  vector<segment_info_t> vec = this->segments[segment_t::DHT];

  for (segment_info_t info: vec) {
    auto [offset, length] = info;
    this->handle_huffman(offset, length);
  }
}

// get 2 quantization tables
void JpegDecoder::handle_dqts() {
  vector<segment_info_t> vec = this->segments[segment_t::DQT];
  for (segment_info_t info: vec) {
    auto [offset, length] = info;
    this->file.seekg(offset, ios::beg);
    // temp
    char num_of_qt;
    this->file.read(&num_of_qt, 1);
    if (num_of_qt == 0) { // luma qt
      this->file.read((char*)&this->qt_luma[0], length - 1);
    } else if (num_of_qt == 1) { // chroma qt
      this->file.read((char*)&this->qt_chroma[0], length - 1);
    }
  }
}

void JpegDecoder::get_segments() {
  // start of image
  read_assert_str_equal(this->file, "\xff\xd8", "start of image header error");

  char marker[2];
  int seg_length;
  // while not end of image
  while (true) {
    this->file.read(marker, 2);
    seg_length = read_u16_be(this->file) - 2;

    int cur = this->file.tellg();
    segment_info_t info = { cur, seg_length };
    if (!memcmp(marker, "\xff\xe0", 2)) {
      this->segments[segment_t::APP0].push_back(info);
    } else if (!memcmp(marker, "\xff\xdb", 2)) {
      this->segments[segment_t::DQT].push_back(info);
    } else if (!memcmp(marker, "\xff\xc4", 2)) {
      this->segments[segment_t::DHT].push_back(info);
    } else if (!memcmp(marker, "\xff\xc0", 2)) {
      this->segments[segment_t::SOF0].push_back(info);
    } else if (!memcmp(marker, "\xff\xda", 2)) {
      this->segments[segment_t::SOS].push_back(info); // start of scan length is always next to end of jpeg
      break;
    }

    this->file.seekg(seg_length, ios::cur); // skip to next segment
  }
}
// huffman code length is not greater than 16
uint32_t JpegDecoder::peek_bit_stream(uint8_t code_size) {
  // the bit position - amount to shift left
  size_t start_shift = this->bit_offset % 8;
  size_t end_shift = (this->bit_offset + code_size) % 8;
  // the index position of bytestream
  size_t start_index = this->bit_offset / 8;
  size_t end_index = (this->bit_offset + code_size) / 8;

  uint32_t code = 0;
  for (uint32_t j = 0; j <= end_index - start_index; j++) {
    code |= (unsigned char)this->bitstream[start_index + j] << (8 * (end_index-start_index-j));
  }
  // right shift to remove lsb
  code >>= (8 - end_shift);
  // mask to remove msb
  uint32_t mask = (1 << (code_size)) - 1;
  code &= mask;
  // this->bit_offset += code_size;
  return code;
}
#define BYTE_TO_BINARY_PATTERN "%c%c%c%c%c%c%c%c"
#define BYTE_TO_BINARY(byte)  \
((byte) & 0x80 ? '1' : '0'), \
((byte) & 0x40 ? '1' : '0'), \
((byte) & 0x20 ? '1' : '0'), \
((byte) & 0x10 ? '1' : '0'), \
((byte) & 0x08 ? '1' : '0'), \
((byte) & 0x04 ? '1' : '0'), \
((byte) & 0x02 ? '1' : '0'), \
((byte) & 0x01 ? '1' : '0') 

// decode a 8x8 mcu and output to buffer
void JpegDecoder::decode8x8(char* buffer) {
  int* y_buffer = new int[64];
  int* cb_buffer = new int[64];
  int* cr_buffer = new int[64];
  int i = 0;

  // luma component
  // dc

  // find a huffman entry in table for dc
  auto dc_entry = this->read_bit_with_ht(this->ht_dc_luma);
  HuffmanCode code_info = dc_entry.first;
  int prefix = this->peek_bit_stream(code_info.length);
  // use the symbol corresponding to code as code_size for next read
  // symbol is also used as value category here
  char category = dc_entry.second;
  auto a = 1 << (category - 1);
  int code = this->peek_bit_stream(category);
  char dc_coeff = code >= a ? code : code - (2 * a - 1); // value category and coefficient
  // only one dc_coeff, add it to buffer directly
  y_buffer[i] = dc_coeff * this->qt_luma[i]; // do the dequantization
  // buf[i] = dc_coeff; // do the dequantization
  i++;
  this->bit_offset += category;

  while (i < 64) {
    // find a huffman entry in table for ac
    auto ac_entry = this->read_bit_with_ht(this->ht_ac_luma);
    HuffmanCode code_info = ac_entry.first;
    int prefix = this->peek_bit_stream(code_info.length);
    char rrrrssss = ac_entry.second;
    if (rrrrssss == 0) { // end of block in run-length coding
      break;
    }
    uint8_t zero_count = rrrrssss >> 4;
    uint8_t category = rrrrssss & 0x0F;
    auto a = 1 << (category - 1);
    // add zero_count zeros into ac coeffs
    for (uint8_t k = 0; k < zero_count; k++) {
      y_buffer[i+k] = 0;
    }
    i += zero_count;
    int code = this->peek_bit_stream(category);
    char ac_coeff = code >= a ? code : code - (2 * a - 1); // value category and coefficient
    y_buffer[i] = ac_coeff * this->qt_luma[i];
    // buf[i] = ac_coeff;
    i++;
    this->bit_offset += category;
  }

  zigzag_rearrange_8x8(y_buffer);

  for (int u = 0; u < 8; u++) {
    for (int v = 0; v < 8; v++) {
      int i = u*8+v;
      printf("%d\t", y_buffer[i]);
    }
    printf("\n");
  }
  // undo zig-zag
}

std::pair<huffman_code_t, char> JpegDecoder::read_bit_with_ht(huffman_table_t& ht) {
  for (auto huffman_table_entry: ht) {
    HuffmanCode code_info = huffman_table_entry.first;
    int prefix = this->peek_bit_stream(code_info.length);
    if (prefix == code_info.code) {
      this->bit_offset += code_info.length;
      return huffman_table_entry;
    }
  }
  throw "code not found while reading bitstream with huffman table";
}
