/* encoding process of jpeg
 * - prepare all contexts
 *   - 2 quantization tables qt_luma, qt_chroma, use typical ones from spec
 *   - 4 huffman tables ht_luma_dc, ht_luma_ac, ht_chroma_dc, ht_chroma_ac
 *   - sof, width/height, and component sampling/quantization destination info
 *   - sos, component huffman table destination info
 * - encode image data
 *   - for each mcu
 *     - encode each component of the mcu (according to the pixel format, this may be YCbCr packed for 444, or YYYYCbCr packed for 420)
 *       
 * encoding process of mcu
 * - fetch a mcu-sized source image from big image
 * - convert to YCbCr
 * - do the encoding in YYYYCbCr packed order
 *   - for luma component, iterate through each 8x8 data unit in the mcu, and fetch the data continuously
 *   - for chroma component (that needs subsampling), fetch data according to the sampling factor
 *   - when a 64-sized vector is fetched
 *     - forward dct
 *     - do the quantization with corresponding table
 *     - zigzag reordering
 *     - for dc
 *       - dc values are diff-ed from previous dc value and the huffman code is emitted to bitstream
 *     - for ac
 *       - run-length coding
 *       - the rrrrssss is huffman encoded and emitted to bitstream
 * 
 * while emitting to bitstream
 * - when a 0xff byte is emitted, make sure to append a 0x00 for stuffing
 */
#ifndef JPEG_ENC
#define JPEG_ENC

#include <fstream>
#include <sstream>
#include "jpeg_tables.h"
#include "bitstream.h"
#include "huffman_enc.h"

class JpegEncoder {
    std::ifstream file;
    size_t w, h;
    // source buffers
    unsigned char* r;
    unsigned char* g;
    unsigned char* b;
    char* Y_MCU;
    char* Cb_MCU;
    char* Cr_MCU;

    BitStream bitstream;

    uint8_t qt_luma[64];
    uint8_t qt_chroma[64];

    HuffmanEnc ht_luma_dc;
    HuffmanEnc ht_luma_ac;
    HuffmanEnc ht_chroma_dc;
    HuffmanEnc ht_chroma_ac;

    void read_as_ppm();
    void init_ht_tables();
    void init_qt_tables();
    void get_YCbCr_from_source(size_t x, size_t y, size_t w, size_t h, size_t stride, char* Y, char* Cb, char* Cr);
    int encode_8x8_per_component(char* src_buffer, size_t x, size_t y, size_t sample_h, size_t sample_v, bool is_chroma, int prev_dc);
    void output_app0();
    void output_qt(bool is_chroma, const uint8_t* table);
    void output_hts();
    void output_ht(bool is_chroma, bool is_ac, const HuffmanEnc& table, uint16_t* total_length, std::stringstream& total);
    void output_sof();
    void output_sos();
    void output_encoded_image_data();
public:
    JpegEncoder(const char* filename);
    ~JpegEncoder();
    void encode();
};

#endif
