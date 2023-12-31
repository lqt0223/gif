#include "jpegdec.h"
#include "common.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <ostream>
#include <stdexcept>
#include <utility>
#include <vector>
#include "huffman.h"
#include "node.h"

char buf[16];

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

// IDCT（逆离散余弦变换），从频域到时域
void inverse_dct_8x8(const int* freq_domain_input, int* time_domain_output) {
  int i, j, u, v; // i, j: 时域的坐标; u, v: 频域的坐标
  float s;

  for (i = 0; i < 8; i++) {
    for (j = 0; j < 8; j++) {
      s = 0;

      for (u = 0; u < 8; u++) {
        for (v = 0; v < 8; v++) {
          auto freq = (float)freq_domain_input[u*8+v];
          auto basis1 = (float)cos((2.0*i+1.0)*u*M_PI/16.0);
          auto basis2 = (float)cos((2.0*j+1.0)*v*M_PI/16.0);
          float cu = u == 0.0 ? 1.0 / sqrt(2.0) : 1.0;
          float cv = v == 0.0 ? 1.0 / sqrt(2.0) : 1.0;
          s += freq * basis1 * basis2 * cu * cv;
        }
      }

      time_domain_output[i*8+j] = (int)(s / 4.0);
    }
  }
}

// void print_64(int* buffer) {
//   for (int i =0 ; i < 64; i++) {
//     printf("%d ", buffer[i]);
//   }
//   printf("\n");
// }
//
// void print_8x8(int* buffer) {
//   for (int u = 0; u < 8; u++) {
//     for (int v = 0; v < 8; v++) {
//       int i = u*8+v;
//       printf("%d\t", buffer[i]);
//     }
//     printf("\n");
//   }
//   printf("\n");
// }

// YCbCr to rgb
void yuv2rgb(
  float y, float cb, float cr,
  float* r, float* g, float* b
) {
    *r =  (y + 128.0 + 1.40200 * cr);
    *g =  (y + 128.0 - 0.34414 * cb - 0.71414 * cr);
    *b =  (y + 128.0 + 1.77200 * cb);
    
    *r = (float)fmin(*r, 255.0); *r = (float)fmax(*r, 0.0);
    *g = (float)fmin(*g, 255.0); *g = (float)fmax(*g, 0.0);
    *b = (float)fmin(*b, 255.0); *b = (float)fmax(*b, 0.0);
}

void output_rgb_8x8_to_buffer(
  uint8_t* dst, const int* Y, const int* Cb, const int* Cr,
  auto& y_sampler,
  auto& cb_sampler,
  auto& cr_sampler,
  size_t x, size_t y, size_t stride
) {
  for (size_t yy = 0; yy < 8; yy++) {
    for (size_t xx = 0; xx < 8; xx++) {
      point_t _xy_y = y_sampler({ xx, yy });
      auto _y = (float)Y[_xy_y.y + _xy_y.x * 8];
      point_t _xy_cb = cb_sampler({ xx, yy });
      auto _cb = (float)Cb[_xy_cb.y + _xy_cb.x * 8];
      point_t _xy_cr = cr_sampler({ xx, yy });
      auto _cr = (float)Cr[_xy_cr.y + _xy_cr.x * 8];
      
      float r,g,b;
      yuv2rgb(_y, _cb, _cr, &r, &g, &b);
      
      size_t ir = ((x + xx) * stride + y + yy) * 3, ig = ir + 1, ib = ir + 2;
      dst[ir] = (uint8_t)r;
      dst[ig] = (uint8_t)g;
      dst[ib] = (uint8_t)b;
    }
  }
}

// scan through image file, and extract bitstream for image data (with ff00 and restart markers removed)
void JpegDecoder::init_bitstream() {
  char byte, cur, next;
  // move to end of "start of scan" data part
  this->file.seekg(this->segments[segment_t::SOS][0].offset + this->segments[segment_t::SOS][0].length, ios::beg);

  while(true) {
    cur = (char)this->file.peek();
    if (cur == '\xff') {
      this->file.seekg(1, ios::cur);
      next = (char)this->file.peek();
      // when ff00 is met, push ff only
      if (next == '\x00') {
        this->bitstream += '\xff';
        this->file.seekg(1, ios::cur);
        // end of image
      // when ffd9 is met, end of file
      } else if (next == '\xd9') {
        break;
      // restart markers
      } else if (next >= '\xd0' && next <= '\xd7') {
        this->file.seekg(1, ios::cur);
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
}

JpegDecoder::~JpegDecoder() {
  for (auto ptr: this->y_bufs) {
    delete[] ptr;
  }
  for (auto ptr: this->cb_bufs) {
    delete[] ptr;
  }
  for (auto ptr: this->cr_bufs) {
    delete[] ptr;
  }
}
JpegDecoder::JpegDecoder(const char* filename):
  file(filename), bit_offset(0), w(0), h(0), restart_interval(0)
{
  assert(this->file.is_open() && "open file error");
  this->reset_segments();
  this->get_segments();

  this->handle_define_quantization_tables();
  this->handle_huffman_tables();
  this->handle_restart();
  this->handle_sof0();
  this->handle_sos();

  this->init_bitstream();
  this->file.close();
  // this->parse_context();
}

void JpegDecoder::reset_segments() {
  this->segments[segment_t::APP0] = vector<segment_info_t>();
  this->segments[segment_t::SOF0] = vector<segment_info_t>();
  this->segments[segment_t::SOS] = vector<segment_info_t>();
  this->segments[segment_t::DQT] = vector<segment_info_t>();
  this->segments[segment_t::DHT] = vector<segment_info_t>();
}

void JpegDecoder::handle_restart() {
  if (this->segments.contains(segment_t::DRI)) {
    segment_info_t info = this->segments[segment_t::DRI][0]; // only one sof0 segment
    this->file.seekg(info.offset, ios::beg);
    this->restart_interval = read_u16_be(this->file);
  }
}

// get image width, height frame component config etc..
void JpegDecoder::handle_sof0() {
  segment_info_t info = this->segments[segment_t::SOF0][0]; // only one sof0 segment
  this->file.seekg(info.offset, ios::beg);
  read_assert_str_equal(this->file, buf, "\x08", "data precision not 8");
  this->h = read_u16_be(this->file);
  this->w = read_u16_be(this->file);
  read_assert_str_equal(this->file, buf, "\x03", "image component not 3");
  this->file.read((char*)&this->frame_components[0], 9);

  int* buf_ptr;
  auto [yh, yv] = this->frame_components[0].sampling_factor_packed;
  auto [cbh, cbv] = this->frame_components[1].sampling_factor_packed;
  auto [crh, crv] = this->frame_components[2].sampling_factor_packed;
  fprintf(stderr, "%d %d %d %d %d %d\n", yh, yv, cbh, cbv, crh, crv);
  if (
    yh == 1 && yv == 1 &&
    cbh == 1 && cbv == 1 &&
    crh == 1 && crv == 1
  ) {
    // todo crop to original dimension at output
    this->w = pad_8x(this->w);
    this->h = pad_8x(this->h);
    this->mcu_w = 8; this->mcu_h = 8;
    this->sampl = sampling_t::YUV_DEFAULT;

    buf_ptr = new int[64];
    this->y_bufs.push_back(buf_ptr);
    buf_ptr = new int[64];
    this->cb_bufs.push_back(buf_ptr);
    buf_ptr = new int[64];
    this->cr_bufs.push_back(buf_ptr);
  } else if (
    yh == 2 && yv == 2 &&
    cbh == 1 && cbv == 1 &&
    crh == 1 && crv == 1
  )  {
    this->w = pad_16x(this->w);
    this->h = pad_16x(this->h);
    this->mcu_w = 16; this->mcu_h = 16;
    this->sampl = sampling_t::YUV221111;

    // four y buffers
    for (int i = 0; i < 4; i++) {
      buf_ptr = new int[64];
      this->y_bufs.push_back(buf_ptr);
    } 
    buf_ptr = new int[64];
    this->cb_bufs.push_back(buf_ptr);
    buf_ptr = new int[64];
    this->cr_bufs.push_back(buf_ptr);
  } else if (
    yh == 2 && yv == 1 &&
    cbh == 1 && cbv == 1 &&
    crh == 1 && crv == 1
  )  {
    this->w = pad_8x(this->w);
    this->h = pad_16x(this->h);
    this->mcu_w = 8; this->mcu_h = 16;
    this->sampl = sampling_t::YUV211111;

    // two y buffers
    for (int i = 0; i < 2; i++) {
      buf_ptr = new int[64];
      this->y_bufs.push_back(buf_ptr);
    } 
    buf_ptr = new int[64];
    this->cb_bufs.push_back(buf_ptr);
    buf_ptr = new int[64];
    this->cr_bufs.push_back(buf_ptr);
  } else if (
    yh == 1 && yv == 2 &&
    cbh == 1 && cbv == 1 &&
    crh == 1 && crv == 1
  )  {
    this->w = pad_16x(this->w);
    this->h = pad_8x(this->h);
    this->mcu_w = 16; this->mcu_h = 8;
    this->sampl = sampling_t::YUV121111;

    // two y buffers
    for (int i = 0; i < 2; i++) {
      buf_ptr = new int[64];
      this->y_bufs.push_back(buf_ptr);
    } 
    buf_ptr = new int[64];
    this->cb_bufs.push_back(buf_ptr);
    buf_ptr = new int[64];
    this->cr_bufs.push_back(buf_ptr);
  } else if (
    yh == 2 && yv == 1 &&
    cbh == 2 && cbv == 1 &&
    crh == 2 && crv == 1
  )  {
    this->w = pad_8x(this->w);
    this->h = pad_16x(this->h);
    this->mcu_w = 8; this->mcu_h = 16;
    this->sampl = sampling_t::YUV212121;

    // two y buffers
    for (int i = 0; i < 2; i++) {
      buf_ptr = new int[64];
      this->y_bufs.push_back(buf_ptr);
    } 
    for (int i = 0; i < 2; i++) {
      buf_ptr = new int[64];
      this->cb_bufs.push_back(buf_ptr);
    } 
    for (int i = 0; i < 2; i++) {
      buf_ptr = new int[64];
      this->cr_bufs.push_back(buf_ptr);
    } 
  } else {
    throw runtime_error("not supported");
  }
}

void JpegDecoder::handle_sos() {
  segment_info_t info = this->segments[segment_t::SOS][0]; // only one sos segment
  this->file.seekg(info.offset, ios::beg);
  read_assert_str_equal(this->file, buf, "\x03", "image component not 3");
  this->file.read((char*)&this->scan_components[0], 6);
  // remaining data in sos segment is not for baseline dct - ignored
}

// extract each huffman table from dht segment
void JpegDecoder::handle_huffman(long long offset, int length) {
  this->file.seekg(offset, ios::beg);
  while (length > 0) {
    char ht_info; // 1 byte of packed huffman table info
    this->file.read(&ht_info, 1);
    length -= 1;

    char nb_sym[16];
    this->file.read(nb_sym, 16);
    length -= 16;
    int sum = 0;
    for (char i : nb_sym) {
      sum += i;
    }
    char* symbols = new char[sum];

    this->file.read(symbols, sum);
    length -= sum;

    bool is_ac = !!(ht_info >> 4);
    uint8_t destination = ht_info & 0x0f;

    HuffmanTree ht(nb_sym, symbols);
    if (is_ac) {
      this->ac_hts.insert({ destination, ht });
    } else {
      this->dc_hts.insert({ destination, ht });
    }

    delete[] symbols;
  }
}

// get 4 huffman tables
void JpegDecoder::handle_huffman_tables() {
  vector<segment_info_t> vec = this->segments[segment_t::DHT];

  for (segment_info_t info: vec) {
    this->handle_huffman(info.offset, info.length);
  }
}

void JpegDecoder::handle_define_quantization_tables() {
  vector<segment_info_t> vec = this->segments[segment_t::DQT];
  for (segment_info_t info: vec) {
    this->handle_define_quantization_table(info.offset, info.length);

  }
}

// extract each quantization table from dqt segment
void JpegDecoder::handle_define_quantization_table(long long offset, int length) {
  this->file.seekg(offset, ios::beg);
  while (length > 0) {
    char qt_info;
    string qt_data(64, 0); // make sure init qt_data memory
    this->file.read(&qt_info, 1);
    length -= 1;
    bool is_16_bit = !!(qt_info >> 4);
    uint8_t destination = qt_info & 0x0f;
    this->file.read(&qt_data[0], 64);
    this->quantization_tables[destination] = qt_data;
    length -= 64;
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

    long long cur = this->file.tellg();
    segment_info_t info = { cur, seg_length };
    if (!memcmp(marker, "\xff\xe0", 2)) {
      this->segments[segment_t::APP0].push_back(info);
    } else if (!memcmp(marker, "\xff\xdb", 2)) {
      this->segments[segment_t::DQT].push_back(info);
    } else if (!memcmp(marker, "\xff\xc4", 2)) {
      this->segments[segment_t::DHT].push_back(info);
    } else if (!memcmp(marker, "\xff\xc0", 2)) {
      this->segments[segment_t::SOF0].push_back(info);
    } else if (!memcmp(marker, "\xff\xfe", 2)) {
      this->segments[segment_t::COMMENT].push_back(info);
    } else if (!memcmp(marker, "\xff\xdd", 2)) {
      this->segments[segment_t::DRI].push_back(info);
    } else if (!memcmp(marker, "\xff\xda", 2)) {
      this->segments[segment_t::SOS].push_back(info); // start of scan length is always next to end of jpeg
      break;
    }

    this->file.seekg(seg_length, ios::cur); // skip to next segment
  }
}
// 从当前bitstream已读位置，再读出code_size个位，得到一个值
uint32_t JpegDecoder::read_bitstream_with_length(uint8_t code_size) {
  // 每8位是1个字节，end_shift值的是读取的最后1个位的index在1个字节中的偏移量
  size_t end_shift = (this->bit_offset + code_size) % 8;
  // 将要读取的位，所覆盖的起始字节和结束字节的index
  size_t start_index = this->bit_offset / 8;
  size_t end_index = (this->bit_offset + code_size) / 8;

  uint32_t code = 0;
  // 从起始字节直到结束字节进行读取，对结束进行 与 操作
  for (uint32_t j = 0; j <= end_index - start_index; j++) {
    code |= (unsigned char)this->bitstream[start_index + j] << (8 * (end_index-start_index-j));
  }
  // 右移，去掉低位
  code >>= (8 - end_shift);
  // 使用mask操作去掉高位
  uint32_t mask = (1 << (code_size)) - 1;
  code &= mask;

  this->bit_offset += code_size;
  return code;
}

// decode MCUs and output
void JpegDecoder::decode() {
  int dc_y = 0;
  int dc_cr = 0;
  int dc_cb = 0;

  auto* output = new uint8_t[this->w*this->h*3];

  auto identical = [&](point_t input) { return input; };
  auto upsample_top_left = [&](point_t input) { return point_t { input.x/2, input.y/2 }; };
  auto upsample_top_right = [&](point_t input) { return point_t { input.x/2+4, input.y/2 }; };
  auto upsample_bottom_left = [&](point_t input) { return point_t { input.x/2, input.y/2+4 }; };
  auto upsample_bottom_right = [&](point_t input) { return point_t { input.x/2+4, input.y/2+4 }; };
  auto upsample_left = [&](point_t input) { return point_t { input.x/2,  input.y }; };
  auto upsample_right = [&](point_t input) { return point_t { input.x/2+4,  input.y }; };
  auto upsample_top = [&](point_t input) { return point_t { input.x,  input.y/2 }; };
  auto upsample_bottom = [&](point_t input) { return point_t { input.x,  input.y/2+4 }; };

  size_t restart_count = this->restart_interval;

  // for mcu8x8(data is YCbCr packed, no chroma subsampling)
  if (this->sampl == sampling_t::YUV_DEFAULT) {
    for (int y_mcu = 0; y_mcu < this->h / this->mcu_h; y_mcu++) {
      for (int x_mcu = 0; x_mcu < this->w / this->mcu_w; x_mcu++) {
        dc_y = this->decode_8x8_per_component(this->y_bufs[0], dc_y, 0);
        // printf("mcu no. %d %d %d Y\n", x_mcu, y_mcu, y_mcu * this->w / 8 + x_mcu); print_64(this->buf_y);
        dc_cb = this->decode_8x8_per_component(this->cb_bufs[0], dc_cb, 1);
        // printf("mcu no. %d %d %d Cr\n", x_mcu, y_mcu, y_mcu * this->w / 8 + x_mcu); print_64(this->buf_cr);
        dc_cr = this->decode_8x8_per_component(this->cr_bufs[0], dc_cr, 2);
        // printf("mcu no. %d %d %d Cb\n", x_mcu, y_mcu, y_mcu * this->w / 8 + x_mcu); print_64(this->buf_cb);
        output_rgb_8x8_to_buffer(output, this->y_bufs[0], this->cb_bufs[0], this->cr_bufs[0], identical, identical, identical, y_mcu*this->mcu_h, x_mcu*this->mcu_w, this->w);

        // restart interval 
        if (this->restart_interval > 0) {
          restart_count--;
          if (restart_count == 0) {
            restart_count = this->restart_interval;
            dc_y = dc_cb = dc_cr = 0;
            if (this->bit_offset & 7) {
              this->bit_offset += (8 - this->bit_offset & 7); //align to byte
            }
          }
        }
      }
    }
  } else if (this->sampl == sampling_t::YUV221111) {
    for (int y_mcu = 0; y_mcu < this->h / this->mcu_h; y_mcu++) {
      for (int x_mcu = 0; x_mcu < this->w / this->mcu_w; x_mcu++) {
        dc_y = this->decode_8x8_per_component(this->y_bufs[0], dc_y, 0);
        dc_y = this->decode_8x8_per_component(this->y_bufs[1], dc_y, 0);
        dc_y = this->decode_8x8_per_component(this->y_bufs[2], dc_y, 0);
        dc_y = this->decode_8x8_per_component(this->y_bufs[3], dc_y, 0);
        dc_cb = this->decode_8x8_per_component(this->cb_bufs[0], dc_cb, 1);
        dc_cr = this->decode_8x8_per_component(this->cr_bufs[0], dc_cr, 2);
        output_rgb_8x8_to_buffer(output, this->y_bufs[0], this->cb_bufs[0], this->cr_bufs[0], identical, upsample_top_left, upsample_top_left, y_mcu*this->mcu_h, x_mcu*this->mcu_w, this->w);
        output_rgb_8x8_to_buffer(output, this->y_bufs[1], this->cb_bufs[0], this->cr_bufs[0], identical, upsample_bottom_left, upsample_bottom_left, y_mcu*this->mcu_h, x_mcu*this->mcu_w + 8, this->w);
        output_rgb_8x8_to_buffer(output, this->y_bufs[2], this->cb_bufs[0], this->cr_bufs[0], identical, upsample_top_right, upsample_top_right, y_mcu*this->mcu_h + 8, x_mcu*this->mcu_w, this->w);
        output_rgb_8x8_to_buffer(output, this->y_bufs[3], this->cb_bufs[0], this->cr_bufs[0], identical, upsample_bottom_right, upsample_bottom_right, y_mcu*this->mcu_h + 8, x_mcu*this->mcu_w + 8, this->w);

        // restart interval 
        if (this->restart_interval > 0) {
          restart_count--;
          if (restart_count == 0) {
            restart_count = this->restart_interval;
            dc_y = dc_cb = dc_cr = 0;
            if (this->bit_offset & 7) {
              this->bit_offset += (8 - this->bit_offset & 7); //align to byte
            }
          }

        }
      }
    }
  } else if (this->sampl == sampling_t::YUV211111) {
    for (int y_mcu = 0; y_mcu < this->h / this->mcu_h; y_mcu++) {
      for (int x_mcu = 0; x_mcu < this->w / this->mcu_w; x_mcu++) {
        dc_y = this->decode_8x8_per_component(this->y_bufs[0], dc_y, 0);
        dc_y = this->decode_8x8_per_component(this->y_bufs[1], dc_y, 0);
        dc_cb = this->decode_8x8_per_component(this->cb_bufs[0], dc_cb, 1);
        dc_cr = this->decode_8x8_per_component(this->cr_bufs[0], dc_cr, 2);
        output_rgb_8x8_to_buffer(output, this->y_bufs[0], this->cb_bufs[0], this->cr_bufs[0], identical, upsample_left, upsample_left, y_mcu*this->mcu_h, x_mcu*this->mcu_w, this->w);
        output_rgb_8x8_to_buffer(output, this->y_bufs[1], this->cb_bufs[0], this->cr_bufs[0], identical, upsample_right, upsample_right, y_mcu*this->mcu_h+8, x_mcu*this->mcu_w, this->w);

        // restart interval 
        if (this->restart_interval > 0) {
          restart_count--;
          if (restart_count == 0) {
            restart_count = this->restart_interval;
            dc_y = dc_cb = dc_cr = 0;
            if (this->bit_offset & 7) {
              this->bit_offset += (8 - this->bit_offset & 7); //align to byte
            }
          }
        }
      }
    }
  } else if (this->sampl == sampling_t::YUV121111) {
    for (int y_mcu = 0; y_mcu < this->h / this->mcu_h; y_mcu++) {
      for (int x_mcu = 0; x_mcu < this->w / this->mcu_w; x_mcu++) {
        dc_y = this->decode_8x8_per_component(this->y_bufs[0], dc_y, 0);
        dc_y = this->decode_8x8_per_component(this->y_bufs[1], dc_y, 0);
        dc_cb = this->decode_8x8_per_component(this->cb_bufs[0], dc_cb, 1);
        dc_cr = this->decode_8x8_per_component(this->cr_bufs[0], dc_cr, 2);
        output_rgb_8x8_to_buffer(output, this->y_bufs[0], this->cb_bufs[0], this->cr_bufs[0], identical, upsample_top, upsample_top, y_mcu*this->mcu_h, x_mcu*this->mcu_w, this->w);
        output_rgb_8x8_to_buffer(output, this->y_bufs[1], this->cb_bufs[0], this->cr_bufs[0], identical, upsample_bottom, upsample_bottom, y_mcu*this->mcu_h, x_mcu*this->mcu_w+8, this->w);

        // restart interval 
        if (this->restart_interval > 0) {
          restart_count--;
          if (restart_count == 0) {
            restart_count = this->restart_interval;
            dc_y = dc_cb = dc_cr = 0;
            if (this->bit_offset & 7) {
              this->bit_offset += (8 - this->bit_offset & 7); //align to byte
            }
          }
        }
      }
    }
  } else if (this->sampl == sampling_t::YUV212121) {
    for (int y_mcu = 0; y_mcu < this->h / this->mcu_h; y_mcu++) {
      for (int x_mcu = 0; x_mcu < this->w / this->mcu_w; x_mcu++) {
        dc_y = this->decode_8x8_per_component(this->y_bufs[0], dc_y, 0);
        dc_y = this->decode_8x8_per_component(this->y_bufs[1], dc_y, 0);
        dc_cb = this->decode_8x8_per_component(this->cb_bufs[0], dc_cb, 1);
        dc_cb = this->decode_8x8_per_component(this->cb_bufs[1], dc_cb, 1);
        dc_cr = this->decode_8x8_per_component(this->cr_bufs[0], dc_cr, 2);
        dc_cr = this->decode_8x8_per_component(this->cr_bufs[1], dc_cr, 2);
        output_rgb_8x8_to_buffer(output, this->y_bufs[0], this->cb_bufs[0], this->cr_bufs[0], identical, identical, identical, y_mcu*this->mcu_h, x_mcu*this->mcu_w, this->w);
        output_rgb_8x8_to_buffer(output, this->y_bufs[1], this->cb_bufs[1], this->cr_bufs[1], identical, identical, identical, y_mcu*this->mcu_h+8, x_mcu*this->mcu_w, this->w);

        // restart interval 
        if (this->restart_interval > 0) {
          restart_count--;
          if (restart_count == 0) {
            restart_count = this->restart_interval;
            dc_y = dc_cb = dc_cr = 0;
            if (this->bit_offset & 7) {
              this->bit_offset += (8 - this->bit_offset & 7); //align to byte
            }
          }
        }
      }
    }
  } else {
    throw "not supported";
  }
  // output to stderr
  printf("P6\n%d %d\n255\n", this->w, this->h);
  fwrite(output, 1, this->w*this->h*3, stdout);
  
  delete[] output;
}

// 根据某Huffman table（二分查找树）从bitstream中找到一个编码
char JpegDecoder::read_bitstream_with_ht(HuffmanTree& ht) {
  Node* cur = ht.root.get();
  while (true) {
    // 不断从bitstream中读取一位，遇到1向右查找，遇到0向左查找。直到出现叶子节点
    if (cur->left == nullptr && cur->right == nullptr && cur->letter != -1) {
      return cur->letter;
    }
    bool bit = this->read_bitstream_with_length(1);
    if (bit == 1) {
      cur = cur->right.get();
    } else {
      cur = cur->left.get();
    }
  }
}

// 在某个category内，根据值的二进制表示 解码出原值
int get_coefficient(uint8_t category, int bits) {
  int l = 1 << (category - 1);
  if (bits >= l) {
    return bits;
  } else {
    return bits - 2 * l + 1;
  }
}

int JpegDecoder::decode_8x8_per_component(int* dst, int old_dc, uint8_t nth_component) {
  // 8x8的buffer全部清零
  memset(dst, 0, sizeof(int) * 64);
  // 已解码出来的coefficients，到达64个时则表示此数据单元已解码完成
  int decoded_coeffs = 0;
  string* qt;
  // 根据当前是哪个通道，选择对应的量化表
  uint8_t qt_destination = this->frame_components[nth_component].qt_destination;
  qt = &this->quantization_tables[qt_destination];

  // dc解码
  // 根据当前是哪个通道，选择对应的Huffman表
  uint8_t ht_dc_destination = this->scan_components[nth_component].table_destinations_packed.t_dc;
  // 读取图片数据bitstream，直到取出一个编码，作为dc category值
  uint8_t dc_category = this->read_bitstream_with_ht(this->dc_hts.at(ht_dc_destination));
  // 再从图片数据bitstream中读取dc_category位
  uint32_t dc_code = this->read_bitstream_with_length(dc_category);
  // 得到dc的差值和新值
  int new_dc = old_dc + get_coefficient(dc_category, dc_code);
  // 进行反量化
  dst[decoded_coeffs] = new_dc * (*qt)[decoded_coeffs];
  decoded_coeffs++;

  // ac解码
  // 根据当前是哪个通道，选择对应的Huffman表
  uint8_t ht_ac_destination = this->scan_components[nth_component].table_destinations_packed.t_ac;
  while (decoded_coeffs < 64) {
    // 读取图片数据bitstream，直到取出一个编码，作为AC RRRRSSSS值
    uint8_t rrrrssss = this->read_bitstream_with_ht(this->ac_hts.at(ht_ac_destination));
    // 此值为0，表示在游程编码中，接下来都是0了
    if (rrrrssss == 0) {
      break;
    }
    // 前4位为前置0的个数，后4位为N个0后紧随的那个数的ac category
    uint8_t zero_count = rrrrssss >> 4;
    uint8_t ac_category = rrrrssss & 0x0f;
    // 在解码结果中填入N个前置0（这里是用跳过index来实现的）
    decoded_coeffs += zero_count;
    // 再从图片数据bitstream中读取ac_category位
    uint32_t ac_code = this->read_bitstream_with_length(ac_category);
    // 得到ac的值
    int ac_coefficient = get_coefficient(ac_category, ac_code);
    // 进行反量化
    dst[decoded_coeffs] = ac_coefficient * (*qt)[decoded_coeffs];
    decoded_coeffs++;
  }

  // z字扫描和IDCT变换
  zigzag_rearrange_8x8(dst, this->buf_temp);
  inverse_dct_8x8(this->buf_temp, dst);
  return new_dc;
}
