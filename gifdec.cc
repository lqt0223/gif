#include "gif.h"
#include <SDL2/SDL.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

using namespace std;

// global variables
char buf[256];
map<uint16_t, u8string> code_table;

typedef struct {
    size_t num_of_frames;
    bool eof;
    u8string lzw_codes;
} dec_stat_t;
dec_stat_t dec_stat = { 0, false };

// decoding global context
typedef struct {
    const lsd_t& lsd;
    const string& gct;
    uint16_t w;
    uint16_t h;
} dec_ctx_t;

// decoding local context
typedef struct {
    const image_desc_t& image_desc;
    const gce_t& gce;
    const string& lct;
} dec_local_ctx_t;

void init_code_table(uint16_t init_table_size) {
    code_table.clear();
    for (uint16_t i = 0; i < init_table_size; i++) {
        code_table[code_table.size()] = u8string(1, i);
    }
}

#define read_assert_str_equal(file, str, err) \
    file.read(buf, sizeof(str) - 1); \
    assert(!memcmp(buf, str, sizeof(str) - 1) && err)

#define read_assert_num_equal(file, num, err) \
    file.read(buf, 1); \
    assert(buf[0] == num && err)

string bytes_from_all_sub_blocks(istream& file) {
    string bytes;
    // while we are not at the end of all sub blocks
    while (file.peek() != 0x00) {
        uint8_t sub_block_size;
        file.get((char&)sub_block_size);
        string sub_block_data(sub_block_size, ' ');
        // get sub block data
        file.read(&sub_block_data[0], sub_block_size);
        bytes.append(sub_block_data);
    }
    file.seekg(1, ios::cur); // skip end of block
    return bytes;
}

// write as argb for sdl rendering
typedef struct {
    uint16_t l;
    uint16_t t;
    uint16_t w;
    uint16_t h;
} rect_t;

class FrameBufferARGB {
    uint16_t w;
    uint16_t h;

    rect_t rect; // the sub rect for writing
    size_t i; // index within the sub rect

public:
    char* buffer_head;
    FrameBufferARGB(uint16_t width, uint16_t height):
        w(width),
        h(height),
        i(0),
        rect({  0, 0, width, height })
    {
        // init rect should be the full buffer
        this->buffer_head = new char[this->w * this->h * 4];
    };
    ~FrameBufferARGB() {
        delete[] this->buffer_head;
    }
    void clear_buffer() const {
        memset(this->buffer_head, 255, this->w*this->h*4);
    }
    void init_rect(rect_t _rect) {
        this->i = 0;
        this->rect = _rect;
    }
    [[nodiscard]] char* ptr() const {
        uint16_t x = this->i % this->rect.w;
        uint16_t y = this->i / this->rect.w;
        return this->buffer_head + (this->w * (this->rect.t + y) + this->rect.l + x) * 4;
    }
    // write at buffer current pos
    void write(const char* src, size_t size, size_t offset) const {
        char* ptr = this->ptr() + offset;
        memcpy(ptr, src, size);
    }
    void skip() {
        this->i++;
    }
    // write at buffer current pos
    void write_char(unsigned char chr, size_t size, size_t offset) const {
        char* ptr = this->ptr() + offset;
        memset(ptr, chr, size);
    }
    void write_rgb(const char* src) {
        this->write_char(0, 1, 0);
        this->write(src, 3, 1);
        this->i++;
    };
};

void write_colors_to_fb(FrameBufferARGB& framebuffer, u8string& indexes, const gce_t& gce, const char* gct_data) {
    for (auto index: indexes) {
        if (index == gce.tran_index) {
            framebuffer.skip();
        } else {
            framebuffer.write_rgb(gct_data + index * 3);
        }
    }
}

void lzw_unpack_decode(ifstream& file, const dec_ctx_t& ctx, const dec_local_ctx_t local_ctx, FrameBufferARGB& framebuffer) {
    uint8_t min_code_size;
    file.get((char&)min_code_size);

    auto gce = local_ctx.gce;
    uint16_t clear_code = 1 << min_code_size;
    uint16_t end_code = clear_code + 1;
    uint16_t table_size = 1 << min_code_size;
    uint32_t n_bit = 0;
    uint32_t code, prev_code;
    uint8_t code_size;
    u8string indexes, prev_indexes, new_indexes;

    string bytes = bytes_from_all_sub_blocks(file); // packed bytes from all sub blocks
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
    const char* color_table = !local_ctx.lct.empty() ? local_ctx.lct.data() : ctx.gct.data();
    write_colors_to_fb(framebuffer, indexes, gce, color_table);
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
            write_colors_to_fb(framebuffer, indexes, gce, color_table);
            code_table[code_table.size()] = code_table[prev_code] + u8string(1, indexes[0]);
            prev_code = code;
        } else {
            prev_indexes = code_table[prev_code];
            new_indexes = prev_indexes + u8string(1, prev_indexes[0]);
            write_colors_to_fb(framebuffer, new_indexes, gce, color_table);
            code_table[code_table.size()] = new_indexes;
            prev_code = code;
        }
        if (code_table.size() == (1 << code_size)) {
            code_size < 12 && code_size++; // some encoder will not emit clear code (and will not grow code size further)
        }
    }
}

void decode_frame(ifstream& file, const dec_ctx_t& ctx, FrameBufferARGB& framebuffer, const gce_t& gce) {
    // if disposal needed, clear the framebuffer
    if (gce.packed.disposal == 2) {
        framebuffer.clear_buffer();
    }
    // image descriptor
    read_assert_str_equal(file, "\x2c", "image descriptor header error");
    image_desc_t image_desc;
    file.read((char*)image_desc.raw, sizeof(image_desc.raw));

    framebuffer.init_rect({ 
        image_desc.l,
        image_desc.t,
        image_desc.w,
        image_desc.h
    });

    string lct;
    if (image_desc.packed.has_lct) {
        uint16_t lct_real_size = (1 << (image_desc.packed.lct_sz + 1)) * 3;
        lct = string(lct_real_size, ' ');
        file.read(&lct[0], lct_real_size);
    }

    // lzw-encoded block
    dec_local_ctx_t local_ctx = { image_desc, gce, lct };
    lzw_unpack_decode(file, ctx, local_ctx, framebuffer);
}

string parse_lsd(ifstream& file, const lsd_t& lsd) {
    file.read((char*)lsd.raw, sizeof(lsd.raw));
    // global color table
    uint16_t gct_real_size = (1 << (lsd.packed.gct_sz + 1)) * 3;
    string gct(gct_real_size, ' ');
    file.read(&gct[0], gct_real_size);
    return gct;
}

void parse_header(ifstream& file) {
    read_assert_str_equal(file, "GIF89a", "not gif89 file");
}

void parse_application_extension(ifstream& file) {
    // NETSCAPE or other
    read_assert_str_equal(file, "\x21\xff\x0b", "netscape looping application extension header error");
    file.seekg(11, ios::cur); // skip identifier and authentication code
    
    uint8_t block_size;
    file.get((char&)block_size);
    file.seekg(block_size, ios::cur);
    read_assert_num_equal(file, 0, "application extension block not terminated with 0");
}

void parse_comment_extension(ifstream& file) {
    read_assert_str_equal(file, "\x21\xfe", "comment extension header error");
    string bytes = bytes_from_all_sub_blocks(file); // packed bytes from all sub blocks
    if (!dec_stat.eof) {
        cerr << "Comment block at offset " << file.tellg() << " with content: " << bytes <<endl;
    }
}

void parse_gce(ifstream& file, gce_t& gce) {
    read_assert_str_equal(file, "\x21\xf9\x04", "graphic control extension header error");
    file.read((char*)gce.raw, sizeof(gce.raw));
    read_assert_str_equal(file, "\x00", "graphic control extension block terminal error");
}

uint32_t parse_metadata(ifstream& file, lsd_t& lsd, string& gct) {
    // header
    parse_header(file);
    // logical screen descriptor
    gct = parse_lsd(file, lsd);
    return file.tellg();
}

bool try_decode_frame(FrameBufferARGB& framebuffer, ifstream& file, const lsd_t& lsd, gce_t& gce, string& gct) {
    // while not reaching end of gif file
    while (file.peek() != 0x3b) {
        // read block type
        file.read(buf, 2);
        file.seekg(-2, ios::cur);
        // case0: graphic control extension
        if (!memcmp(buf, "\x21\xf9", 2)) {
            parse_gce(file, gce);
        // case1: image descriptor - local color table - image data
        } else if (buf[0] == '\x2c') {
            dec_ctx_t ctx = { lsd, gct, lsd.w, lsd.h };
            decode_frame(file, ctx, framebuffer, gce);
            if (!dec_stat.eof) dec_stat.num_of_frames++;
            return true;
        // case2: application extension (todo more)
        } else if (!memcmp(buf, "\x21\xff", 2)) {
            parse_application_extension(file);
        // case3: comment extension
        } else if (!memcmp(buf, "\x21\xfe", 2)) {
            parse_comment_extension(file);
        }
    }
    return false;
}

int main(int argc, char** argv) {
    assert(argc >= 2 && "no input file");
    char* filename = argv[1];
    ifstream file(filename, ios::binary);
    assert(file.is_open() && "open file error");
    size_t buf_size;

    lsd_t lsd;
    gce_t gce;
    string gct;
    uint32_t file_loop_offset = parse_metadata(file, lsd, gct);

    buf_size = lsd.w*lsd.h*4;
    FrameBufferARGB framebuffer(lsd.w, lsd.h);

    // logging
    cerr  << "dimension: " << lsd.w << "x" << lsd.h << endl;

    // draw decoded framebuffer with sdl
    // todo sdl error handling
    SDL_Init(SDL_INIT_EVERYTHING);
    SDL_Window* window = SDL_CreateWindow("gif decoder", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, lsd.w, lsd.h, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    SDL_Texture* tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_BGRA8888, SDL_TEXTUREACCESS_STREAMING, lsd.w,lsd.h);
    int pitch;
    stringstream s;

    int window_w, window_h, mouse_x, mouse_y, idx;
    unsigned char r,g,b;
    uint32_t start, current, ts, num_of_frame;
    start = SDL_GetTicks();
    size_t total_frames = dec_stat.num_of_frames;
    int fps = 24;
    if (argc == 4) {
        fps = (int)strtol(argv[3], nullptr, 10);
    }
    // do not bind pointer framebuffer.buffer_head to lockTexture, which will be
    //   - freed by SDL_DestroyTexture internally
    //   - double freed by framebuffer destructor
    // instead, use another pointer variable
    char* fb = framebuffer.buffer_head;

    // render first frame
    bool new_frame = try_decode_frame(framebuffer, file, lsd, gce, gct);
    if (new_frame) {
        SDL_LockTexture(tex, nullptr, (void**)&fb, &pitch);
        memcpy(fb, framebuffer.buffer_head, buf_size);
        SDL_UnlockTexture(tex);
        // SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, tex, nullptr, nullptr);
        SDL_RenderPresent(renderer);
    }
    bool at_foreground = true;
    while (true) {
	SDL_Event e;
        if (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                break;
            }
            if (e.type == SDL_WINDOWEVENT) {
                if (e.window.event == SDL_WINDOWEVENT_FOCUS_GAINED) {
                    at_foreground = true;
                } else if (e.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
                    at_foreground = false;
                }
            } else if (e.type == SDL_MOUSEMOTION) {
                s.seekp(0);
                SDL_GetWindowSize(window, &window_w, &window_h);
                mouse_x = e.motion.x * lsd.w / window_w;
                mouse_y = e.motion.y * lsd.h / window_h;
                s << mouse_x << ' ' << mouse_y << ' ';
                idx = (lsd.w * mouse_y + mouse_x) * 4;
                r = fb[idx + 1];
                g = fb[idx + 2];
                b = fb[idx + 3];
                s << r + 0 << ' ' << g + 0 << ' ' << b + 0 << endl;
                SDL_SetWindowTitle(window, s.str().c_str());
            }
        }
        current = SDL_GetTicks();
        ts = current - start;
        bool should_play = ts % (1000 / fps) == 0;
        if (should_play && at_foreground) {
            num_of_frame = (ts * fps / 1000) % total_frames;
            bool new_frame = try_decode_frame(framebuffer, file, lsd, gce, gct);
            if (new_frame) {
                SDL_LockTexture(tex, nullptr, (void**)&fb, &pitch);
                memcpy(fb, framebuffer.buffer_head, buf_size);
                SDL_UnlockTexture(tex);
            } else {
                // loop
                file.seekg(file_loop_offset, ios::beg);
                if (!dec_stat.eof) {
                    cerr  << "num of frames: " << dec_stat.num_of_frames << endl;
                    dec_stat.eof = true;
                }
                continue;
            }
        } else {
            continue;
        }
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, tex, nullptr, nullptr);
        SDL_RenderPresent(renderer);
    }
    SDL_DestroyTexture(tex);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    file.close();
}
