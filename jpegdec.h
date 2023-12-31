#ifndef JPEG_DEC
#define JPEG_DEC

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
    uint8_t qt_destination;
  };
} frame_component_t;

typedef enum {
  UNDEFINED = 0,
  YUV_DEFAULT, // 444
  YUV221111,
  YUV211111,
  YUV121111,
  YUV212121,
} sampling_t;

typedef struct {
  size_t x;
  size_t y;
} point_t;

// the function pointer that map an input point to another point(for up/down sampling)
typedef point_t (*point_sampler_t)(point_t input);

typedef struct {
  uint8_t t_dc: 4;
  uint8_t t_ac: 4;
} table_destinations_t;

typedef union {
  uint8_t raw[2];
  struct {
    uint8_t id;
    table_destinations_t table_destinations_packed;
  };
} scan_component_t;

typedef enum {
  APP0 = 0,
  DQT, // Define Quantization Table  LUMA AND CHROMA
  SOF0, // Start of Frame - 0
  DHT, // Define Huffman Table  DC AND AC, LUMA AND CHROMA
  SOS, // Start of Scan
  COMMENT,
  DRI, // Define Restart Interval
} segment_t;

typedef enum {
  Y = 0,
  Cb,
  Cr,
} component_t;

typedef struct {
  long long offset;
  int length;
} segment_info_t;

class JpegDecoder {
  ifstream file;

  // buffers
  vector<int*> y_bufs;
  vector<int*> cb_bufs;
  vector<int*> cr_bufs;
  int buf_temp[64];
  int buf_temp2[64];

  size_t bit_offset;
  string bitstream;

  // file offset for each segments
  map<segment_t, vector<segment_info_t>> segments;
  // frame component config
  frame_component_t frame_components[3];
  // derived from frame component config
  uint8_t mcu_w, mcu_h;
  sampling_t sampl;
  // scan component_config
  scan_component_t scan_components[3];
  // restart_info
  size_t restart_interval;
  // internal methods
  void init_bitstream();
  void handle_define_quantization_tables();
  void handle_define_quantization_table(long long offset, int length);
  void handle_huffman_tables();
  void handle_huffman(long long offset, int length);
  void handle_sof0();
  void handle_sos();
  void handle_restart();
  void reset_segments();
  void get_segments();
  uint32_t read_bitstream_with_length(uint8_t length);
  char read_bitstream_with_ht(HuffmanTree& ht);
  int decode_8x8_per_component(int* dst, int old_dc, uint8_t nth_component);
public:
  explicit JpegDecoder(const char* filename);
  ~JpegDecoder();
  void decode();

  // quantization tables map<table destination, 64(8bit) or 128(16bit) byte data>
  map<uint8_t, string> quantization_tables;

  // huffman trees
  map<uint8_t, HuffmanTree> dc_hts;
  map<uint8_t, HuffmanTree> ac_hts;

  // width, height
  uint16_t w, h;
};

#endif
