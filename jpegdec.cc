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

char buf[16];
double buf_8x8[64];

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
  for (int u = 0; u < 8; u++) {
    for (int v = 0; v < 8; v++) {
      // int zigzag_i = u * 8 + v;
      int i = u*8+v;
      int zigzag_i = zigzag[i];
      output[i] = input[zigzag_i];
    }
  }
}

// inverse DCT transform
void idct_8x8(double* freq_domain_input, double* time_domain_output) {
  int i, j, u, v; // i, j: coord in time domain; u, v: coord in freq domain
  double s;

  for (i = 0; i < 8; i++) {
    for (j = 0; j < 8; j++) {
      s = 0;

      for (u = 0; u < 8; u++) {
        for (v = 0; v < 8; v++) {
          auto freq = freq_domain_input[u*8+v];
          auto basis1 = cos((2*i+1)*u*M_PI/16);
          auto basis2 = cos((2*j+1)*v*M_PI/16);
          auto cu = u == 0.0 ? 1.0 / sqrt(2.0) : 1.0;
          auto cv = v == 0.0 ? 1.0 / sqrt(2.0) : 1.0;
          s += freq * basis1 * basis2 * cu * cv;
        }
      }

      time_domain_output[i*8+j] = floor(s / 4); // todo can floor be eliminated for precision
    }
  }
}

void print_64(double* buffer) {
  for (int i =0 ; i < 64; i++) {
    printf("%d ", (int)buffer[i]);
  }
  printf("\n");
}

void print_8x8(double* buffer) {
  for (int u = 0; u < 8; u++) {
    for (int v = 0; v < 8; v++) {
      int i = u*8+v;
      printf("%d\t", (int)buffer[i]);
    }
    printf("\n");
  }
  printf("\n");
}

void output_rgb_8x8_to_buffer(uint8_t* dst, double* y, double* cr, double* cb, size_t x_mcu, size_t y_mcu, size_t stride) {
  for (size_t i = 0; i < 64; i++) {
    double _y = y[i];
    double _cr = cr[i];
    double _cb = cb[i];
    
    double r = _cb*(2-2*.114) + _y;
    // r = _y;
    double b = _cr*(2-2*.299) + _y;
    // b = _y;
    double g = (_y - .114*b - .299*r)/.587;
    // g = _y;
    r += 128.0; r = fmin(r, 255.0); r = fmax(r, 0.0);
    g += 128.0; g = fmin(g, 255.0); g = fmax(g, 0.0);
    b += 128.0; b = fmin(b, 255.0); b = fmax(b, 0.0);

    size_t xx = i / 8, yy = i % 8;
    size_t x = x_mcu * 8 + xx, y = y_mcu * 8 + yy;
    size_t ir = (x * stride + y) * 3, ig = ir + 1, ib = ir + 2;
    dst[ir] = (uint8_t)r;
    dst[ig] = (uint8_t)g;
    dst[ib] = (uint8_t)b;
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

  char byte, cur, next;
  // move to end of "start of scan" data part
  this->file.seekg(this->segments[segment_t::SOS][0].offset + this->segments[segment_t::SOS][0].length, ios::beg);

  // get string from file content, with ff00 removed
  // todo use a method
  while(true) {
    cur = this->file.peek();
    if (cur == '\xff') {
      this->file.seekg(1, ios::cur);
      next = this->file.peek();
      // when ff00 is met, push ff only
      if (next == '\x00') {
        this->bitstream += '\xff';
        this->file.seekg(1, ios::cur);
        // end of image
      // when ffd9 is met, end of file
      } else if (next == '\xd9') {
        break;
      // when ff other is met, push normally
      } else {
        this->bitstream += '\xff';
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
  read_assert_str_equal(this->file, buf, "\x08", "data precision not 8");
  this->h = read_u16_be(this->file);
  this->w = read_u16_be(this->file);
  read_assert_str_equal(this->file, buf, "\x03", "image component not 3");
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
  read_assert_str_equal(this->file, buf, "\xff\xd8", "start of image header error");

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
  return code;
}

// decode 8x8 mcus and output
void JpegDecoder::decode() {
  double dc_y = 0.0;
  double dc_cr = 0.0;
  double dc_cb = 0.0;

  uint8_t* output = new uint8_t[this->w*this->h*3];

  int i = 0;
  for (int y_mcu = 0; y_mcu < this->h / 8; y_mcu++) {
    for (int x_mcu = 0; x_mcu < this->w / 8; x_mcu++) {
      dc_y = this->decode_8x8_per_component(this->buf_y, component_t::Y, dc_y);
      // printf("mcu no. %d %d %d Y\n", x_mcu, y_mcu, y_mcu * this->w / 8 + x_mcu);
      // print_64(this->buf_y);
      dc_cr = this->decode_8x8_per_component(this->buf_cr, component_t::Cr, dc_cr);
      // printf("mcu no. %d %d %d Cr\n", x_mcu, y_mcu, y_mcu * this->w / 8 + x_mcu);
      // print_64(this->buf_cr);
      dc_cb = this->decode_8x8_per_component(this->buf_cb, component_t::Cb, dc_cb);
      // printf("mcu no. %d %d %d Cb\n", x_mcu, y_mcu, y_mcu * this->w / 8 + x_mcu);
      // print_64(this->buf_cb);
      output_rgb_8x8_to_buffer(output, this->buf_y, this->buf_cr, this->buf_cb, y_mcu, x_mcu, this->w);
      i++;
      if (false) { // todo
        goto end;
      }
    }
  }
  // output to stderr
  printf("P3\n%d %d\n255\n", this->w, this->h);
  for (size_t i = 0; i < this->w*this->h*3; i++) {
    printf("%d ", output[i]);
  }
  
end:
  delete[] output;
}

// todo try to use huffman tree for faster matching
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

double get_coeff(char category, int bits) {
  int l = 1 << (category - 1);
  if (bits >= l) {
    return (double)bits;
  } else {
    return (double)(bits - 2 * l + 1);
  }
}

// todo can integer buffer be used here?
double JpegDecoder::decode_8x8_per_component(double* dst, component_t component, double old_dc) {
  memset(dst, 0, sizeof(double) * 64);
  int i = 0;
  huffman_table_t* ht;
  uint8_t* qt;

  qt = component == component_t::Y ? this->qt_luma : this->qt_chroma;

  // dc
  // find a huffman entry in table for dc
  ht = component == component_t::Y ? &this->ht_dc_luma : &this->ht_dc_chroma;
  auto dc_entry = this->read_bit_with_ht(*ht);
  // use the symbol corresponding to code as code_size for next read
  // symbol is also used as value category here
  char category = dc_entry.second;
  int code = this->peek_bit_stream(category);
  double new_dc = old_dc + get_coeff(category, code);
  // double new_dc = old_dc + (double)bits; // do the dequantization
  // printf("%f %f %f %f %f\n", old_dc, (double)category, (double)code, (double)bits, new_dc);
  // only one dc_coeff, add it to buffer directly
  dst[i] = new_dc * qt[i];
  // buf[i] = dc_coeff; // do the dequantization
  i++;
  this->bit_offset += category;

  // ac
  ht = component == component_t::Y ? &this->ht_ac_luma : &this->ht_ac_chroma;
  while (i < 64) {
    // find a huffman entry in table for ac
    auto ac_entry = this->read_bit_with_ht(*ht);
    uint8_t rrrrssss = ac_entry.second;
    if (rrrrssss == 0) { // end of block in run-length coding
      break;
    }
    uint8_t zero_count = rrrrssss >> 4;
    uint8_t category = rrrrssss & 0x0F;
    // since the buffer is inited with zero, skip zero_counts
    i += zero_count;
    int code = this->peek_bit_stream(category);
    // printf("%d %d %d %d\n", rrrrssss, zero_count, category, code);
    double ac_coeff = get_coeff(category, code);
    dst[i] = (double)ac_coeff * (double)qt[i];
    // buf[i] = ac_coeff;
    i++;
    this->bit_offset += category;
  }

  zigzag_rearrange_8x8(dst, this->buf_temp);
  idct_8x8(this->buf_temp, dst);
  // upsampling todo
  return new_dc;
}
