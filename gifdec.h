#include "gif.h"
#include <cassert>
#include <fstream>
#include <unordered_map>

using namespace std;

// write as argb for sdl rendering
typedef struct {
    uint16_t l;
    uint16_t t;
    uint16_t w;
    uint16_t h;
} rect_t;

enum pix_fmt_t {
    ARGB,
    RGBA,
    RGB
};

class GifDecoder {
    // file states
    lsd_t lsd{};
    gce_t gce{};
    image_desc_t image_desc{};

    string gct;
    string lct;

    uint32_t file_loop_offset{};

    // storages
    ifstream file;
    unordered_map<uint16_t, u8string> code_table;

    // configs
    pix_fmt_t pix_fmt;

    void parse_header();
    void parse_metadata();
    void parse_lsd_and_set_gct();
    void parse_gce();
    void parse_application_extension();
    void parse_comment_extension();
    void lzw_unpack_decode();
    void init_code_table(uint16_t size);
    void decode_frame_internal();

    string bytes_from_all_sub_blocks();
public:
    unsigned char* buffer;
    GifDecoder(const char* filename, pix_fmt_t pix_fmt);
    ~GifDecoder();
    bool decode_frame();
    uint16_t get_width() const;
    uint16_t get_height() const;
    void loop();
};
