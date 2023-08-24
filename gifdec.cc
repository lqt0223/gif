#include "gifdec.h"
#include <fstream>
#include "common.h"

char buf[256]; // a temp buffer for file content

// util methods
unsigned char* ptr_in_sub_rect(unsigned char* base, rect_t sub_rect, uint16_t rect_width, uint8_t size, size_t i) {
    uint16_t x = i % sub_rect.w;
    uint16_t y = i / sub_rect.w;
    return base + (rect_width * (sub_rect.t + y) + sub_rect.l + x) * size;
}

// public methods
GifDecoder::GifDecoder(const char* filename, pix_fmt_t _pix_fmt):
    file(filename, ios::binary), pix_fmt(_pix_fmt)
{
    assert(this->file.is_open() && "open file error");
    this->parse_metadata();
    uint16_t w = this->get_width();
    uint16_t h = this->get_height();
    this->buffer = new unsigned char[w * h * 4];
}

GifDecoder::~GifDecoder() {
    delete[] this->buffer;
}

uint16_t GifDecoder::get_width() const {
    return this->lsd.w;
}

uint16_t GifDecoder::get_height() const {
    return this->lsd.h;
}

void GifDecoder::loop() {
    this->file.seekg(this->file_loop_offset, ios::beg);
    memset(this->buffer, 255, this->lsd.w * this->lsd.h * 4); // set to white
    this->lct.clear(); // when looping remember to reset local color table
}

bool GifDecoder::decode_frame() {
    // while not reaching end of gif file
    while (this->file.peek() != 0x3b) {
        // read block type
        this->file.read(buf, 2);
        this->file.seekg(-2, ios::cur);
        // case0: graphic control extension
        if (!memcmp(buf, "\x21\xf9", 2)) {
            this->parse_gce();
        // case1: application extension (todo more)
        } else if (!memcmp(buf, "\x21\xff", 2)) {
            this->parse_application_extension();
        // case2: comment extension
        } else if (!memcmp(buf, "\x21\xfe", 2)) {
            this->parse_comment_extension();
        // case3: image descriptor - local color table - image data
        } else if (buf[0] == '\x2c') {
            this->decode_frame_internal();
            return false;

            // if (!dec_stat.eof) dec_stat.num_of_frames++; // todo
            // return true;
        }
    }
    return true;
}

// private methods
void GifDecoder::parse_metadata() {
    // header
    this->parse_header();
    // logical screen descriptor and global color table
    this->parse_lsd_and_set_gct();
    this->file_loop_offset = this->file.tellg();
}

void GifDecoder::parse_header() {
    read_assert_str_equal(this->file, "GIF89a", "not gif89 file");
}

void GifDecoder::parse_lsd_and_set_gct() {
    this->file.read((char*)this->lsd.raw, sizeof(this->lsd.raw));
    // global color table
    uint16_t gct_real_size = (1 << (this->lsd.packed.gct_sz + 1)) * 3;
    this->gct = string(gct_real_size, ' ');
    file.read(&this->gct[0], gct_real_size);
}

void GifDecoder::parse_gce() {
    read_assert_str_equal(this->file, "\x21\xf9\x04", "graphic control extension header error");
    this->file.read((char*)this->gce.raw, sizeof(this->gce.raw));
    read_assert_str_equal(this->file, "\x00", "graphic control extension block terminal error");
}

void GifDecoder::parse_application_extension() {
    // NETSCAPE or other
    read_assert_str_equal(this->file, "\x21\xff\x0b", "netscape looping application extension header error");
    this->file.seekg(11, ios::cur); // skip identifier and authentication code
    
    uint8_t block_size;
    this->file.get((char&)block_size);
    this->file.seekg(block_size, ios::cur);
    read_assert_num_equal(this->file, 0, "application extension block not terminated with 0");
}

void GifDecoder::parse_comment_extension() {
    read_assert_str_equal(file, "\x21\xfe", "comment extension header error");
    string bytes = this->bytes_from_all_sub_blocks(); // packed bytes from all sub blocks
    // if (!dec_stat.eof) { // todo
    //     cerr << "Comment block at offset " << file.tellg() << " with content: " << bytes <<endl;
    // }
}

string GifDecoder::bytes_from_all_sub_blocks() {
    string bytes;
    // while we are not at the end of all sub blocks
    while (this->file.peek() != 0x00) {
        uint8_t sub_block_size;
        this->file.get((char&)sub_block_size);
        string sub_block_data(sub_block_size, ' ');
        // get sub block data
        this->file.read(&sub_block_data[0], sub_block_size);
        bytes.append(sub_block_data);
    }
    this->file.seekg(1, ios::cur); // skip end of block
    return bytes;
}

void GifDecoder::decode_frame_internal() {
    // if disposal needed, clear the framebuffer
    if (this->gce.packed.disposal == 2) {
        memset(this->buffer, 255, this->lsd.w * this->lsd.h * 4); // set to white
    }
    // image descriptor
    read_assert_str_equal(this->file, "\x2c", "image descriptor header error");
    this->file.read((char*)this->image_desc.raw, sizeof(this->image_desc.raw));

    if (this->image_desc.packed.has_lct) {
        uint16_t lct_real_size = (1 << (this->image_desc.packed.lct_sz + 1)) * 3;
        this->lct = string(lct_real_size, ' ');
        this->file.read(&this->lct[0], lct_real_size);
    }

    // lzw-encoded block
    this->lzw_unpack_decode();
}

void GifDecoder::init_code_table(uint16_t init_table_size) {
    this->code_table.clear();
    for (uint16_t i = 0; i < init_table_size; i++) {
        this->code_table[this->code_table.size()] = u8string(1, i);
    }
}

void GifDecoder::lzw_unpack_decode() {
    uint8_t min_code_size;
    this->file.get((char&)min_code_size);

    uint16_t clear_code = 1 << min_code_size;
    uint16_t end_code = clear_code + 1;
    uint16_t table_size = 1 << min_code_size;
    uint32_t n_bit = 0;
    uint32_t code, prev_code;
    uint8_t code_size;
    u8string indexes, prev_indexes, new_indexes;

    string bytes = this->bytes_from_all_sub_blocks(); // packed bytes from all sub blocks
#ifdef DEBUG
    size_t n_code = 0;
#endif

    auto get_next_code = [&]() {
        // the bit position - amount to shift left
        uint32_t start_shift = n_bit % 8;
        // the index position of bytestream
        uint32_t start_index = n_bit / 8;
        uint32_t end_index = (n_bit + code_size) / 8;

        uint32_t code = 0;
        for (uint32_t j = 0; j <= end_index - start_index; j++) {
            code |= ((unsigned char)bytes[start_index + j] << (8 * j));
        }
        // right shift to remove low bits
        code >>= start_shift;
        // mask to remove high bits
        uint32_t mask = (1 << (code_size)) - 1;
        code &= mask;
        n_bit += code_size;
#ifdef DEBUG
        if (n_code < 20) {
            cerr<<"lzw_code debug: " << n_code << ", " << code << endl;
        }
        n_code++;
#endif
        return code;
    };

    size_t i = 0; // the number of written bytes to decoded_frame
    const char* color_table = this->lct.empty() ? this->gct.data() : this->lct.data(); // to use local or global color table
    auto write_to_decoded_frame = [&](const u8string& _indexes) {
        for (auto index: _indexes) {
            rect_t rect = { this->image_desc.l, this->image_desc.t, this->image_desc.w, this->image_desc.h };

            // skip on transparent index
            if (index == this->gce.tran_index) {
            // ARGB, write one byte for alpha and 3 bytes for rgb
            } else if (this->pix_fmt == ARGB) {
                unsigned char* ptr = ptr_in_sub_rect(this->buffer, rect, this->lsd.w, 4, i);
                *ptr = 255;
                memcpy(ptr + 1, color_table + index * 3, 3);
            // RGBA, write 3 bytes for rgb and one byte for alpha 
            } else if (this->pix_fmt == RGBA) {
                unsigned char* ptr = ptr_in_sub_rect(this->buffer, rect, this->lsd.w, 4, i);
                memcpy(ptr, color_table + index * 3, 3);
                *(ptr + 3) = 255;
            // RGB, write 3 bytes for rgb
            } else if (this->pix_fmt == RGB) {
                unsigned char* ptr = ptr_in_sub_rect(this->buffer, rect, this->lsd.w, 3, i);
                memcpy(ptr, color_table + index * 3, 3);
            }
            i++;
        }
    };

    code_size = min_code_size + 1;
    // first code is clear_code - skip
    code = get_next_code();
    assert(code == clear_code && "lzw decode error: first code not clear_code");
clear:
    init_code_table(table_size);
    code_table[clear_code] = u8string(1, clear_code);
    code_table[end_code] = u8string(1, end_code);
    code_size = min_code_size + 1;

    // extract second code from bytes
    code = get_next_code();

    indexes = code_table[code];
    write_to_decoded_frame(indexes);
    prev_code = code;

    while (true) {
        code = get_next_code();
        if (code == clear_code) {
            goto clear;
        } else if (code == end_code) {
            break;
        }

        // lzw decoding
        if (code_table.contains(code)) {
            indexes = code_table[code];
            write_to_decoded_frame(indexes);
            code_table[code_table.size()] = code_table[prev_code] + u8string(1, indexes[0]);
            prev_code = code;
        } else {
            prev_indexes = code_table[prev_code];
            new_indexes = prev_indexes + u8string(1, prev_indexes[0]);
            write_to_decoded_frame(new_indexes);
            code_table[code_table.size()] = new_indexes;
            prev_code = code;
        }
        if (code_table.size() == (1 << code_size)) {
            code_size < 12 && code_size++; // some encoder will not emit clear code (and will not grow code size further)
        }
    }
}
