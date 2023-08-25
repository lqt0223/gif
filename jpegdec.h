#ifndef JPEGDEC
#define JPEGDEC

#include <cstdint>
#include <fstream>
#include <map>
#include <unordered_map>
#include <vector>
#include "huffman.h"

using namespace std;
typedef struct {
  uint8_t vertical: 4;
  uint8_t horizontal: 4;
} sampling_factor_t;

typedef union {
  uint8_t raw[3];
  struct {
    uint8_t id;
    sampling_factor_t sampling_factor_packed;
    uint8_t quantization_mapping;
  };
} frame_component_t;

typedef enum {
  APP0 = 0,
  DQT, // LUMA AND CHROMA
  SOF0,
  DHT, // DC AND AC, LUMA AND CHROMA
  SOS
} segment_t;

typedef enum {
  Y = 0,
  Cr,
  Cb
} component_t;

typedef struct {
  int offset;
  int length;
} segment_info_t;

class JpegDecoder {
  ifstream file;

  // buffers
  int buf_y[64];
  int buf_cr[64];
  int buf_cb[64];
  int buf_temp[64];

  // bit offset, and byte offset for bitstream reading, byte offset is on file.tellg
  size_t bit_offset;
  string bitstream;

  // file offset for each segments
  map<segment_t, vector<segment_info_t>> segments;
  // frame component config
  frame_component_t fc[3];
  // internal methods
  void init_bitstream();
  void handle_dqts();
  void handle_hufs();
  void handle_huffman(int offset, int length);
  void handle_sos0();
  void reset_segments();
  void get_segments();
  uint32_t peek_bit_stream(uint8_t length);
  char get_code_with_ht(HuffmanTree& ht);
  int decode_8x8_per_component(int* dst, component_t component, int old_dc);
public:
  JpegDecoder(const char* filename);
  void decode();

  // quantization tables
  uint8_t qt_luma[64];
  uint8_t qt_chroma[64];

  // huffman trees
  HuffmanTree ht_dc_luma;
  HuffmanTree ht_dc_chroma;
  HuffmanTree ht_ac_luma;
  HuffmanTree ht_ac_chroma;

  // width, height
  uint16_t w, h;
};

#endif
