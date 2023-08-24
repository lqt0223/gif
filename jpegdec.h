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

// todo why bit field cannot get desired result here?
// typedef union  __attribute__((__packed__)) {
//   uint8_t raw;
//   uint8_t : 3; // unused
//   uint8_t type: 1;
//   uint8_t num: 4;
// } ht_info_t;

typedef struct {
  int offset;
  int length;
} segment_info_t;

class JpegDecoder {
  ifstream file;

  // bit offset, and byte offset for bitstream reading, byte offset is on file.tellg
  size_t bit_offset;
  string bitstream;

  // file offset for each segments
  map<segment_t, vector<segment_info_t>> segments;
  // frame component config
  frame_component_t fc[3];
  // internal methods
  void handle_dqts();
  void handle_hufs();
  void handle_huffman(int offset, int length);
  void handle_sos0();
  void reset_segments();
  void get_segments();
  uint32_t peek_bit_stream(uint8_t length);
  std::pair<huffman_code_t, char> read_bit_with_ht(huffman_table_t& ht);
public:
  JpegDecoder(const char* filename);
  void decode8x8(char* buffer);

  // quantization tables
  uint8_t qt_luma[64];
  uint8_t qt_chroma[64];

  // huffman tables
  huffman_table_t ht_dc_luma;
  huffman_table_t ht_dc_chroma;
  huffman_table_t ht_ac_luma;
  huffman_table_t ht_ac_chroma;

  // width, height
  uint16_t w, h;
};

#endif
