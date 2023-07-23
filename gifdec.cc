#include <cassert>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include "gif.h"
#include <SDL2/SDL.h>
#include <sstream>

using namespace std;

char buf[256];
map<uint16_t, u8string> code_table;
typedef struct {
    uint8_t cr;
    const string& gct;
    char* framebuffer; // rgb packed
    size_t buf_size;
} dec_ctx_t;

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

void lzw_unpack_decode(ifstream& file, const dec_ctx_t& ctx) {
    uint8_t min_code_size = ctx.cr + 1;
    uint16_t clear_code = 1 << min_code_size;
    uint16_t end_code = clear_code + 1;
    char* fb_ptr = ctx.framebuffer;

    uint16_t table_size = 1 << min_code_size;
    init_code_table(table_size);
    code_table[clear_code] = u8string(1, clear_code);
    code_table[end_code] = u8string(1, end_code);

    uint8_t code_size = min_code_size + 1;
    uint32_t n_bit = 0;

    u8string bytes; // packed bytes from all sub blocks

    // while we are not at the end of all sub blocks
    while (file.peek() != 0x00) {
        uint8_t sub_block_size;
        file.get((char&)sub_block_size);
        string sub_block_data(sub_block_size, ' ');
        // get sub block data
        file.read(&sub_block_data[0], sub_block_size);
        for (uint8_t c: sub_block_data) {
            bytes.push_back(c);
        }
        // bytes.append(sub_block_data);
    }
    file.seekg(1, ios::cur); // skip end of block

    auto get_next_code = [&]() {
        // the bit position - amount to shift left
        uint32_t start_shift = n_bit % 8;
        uint32_t end_shift = (n_bit + code_size) % 8;
        // the index position of bytestream
        uint32_t start_index = n_bit / 8;
        uint32_t end_index = (n_bit + code_size) / 8;

        uint32_t end_loop = end_index - start_index;

        uint32_t code = 0;
        for (uint8_t j = 0; j <= end_loop; j++) {
            code |= (bytes[start_index + j] << (8 * j));
        }
        // right shift to remove low bits
        code >>= start_shift;
        // mask to remove high bits
        uint32_t mask = (1 << (code_size)) - 1;
        code &= mask;
        n_bit += code_size;
        return code;
    };

    // first code is clear_code - skip
    n_bit += code_size;
    // extract second code from bytes
    uint32_t code = get_next_code();

    u8string indexes = code_table[code];
    for (size_t i = 0; i < indexes.size(); i++) {
        memcpy(fb_ptr, &ctx.gct[indexes[i]*3], 3);
        fb_ptr += 3;
    }
    uint32_t prev_code = code;

    while (1) {
        // todo debug only
        size_t idx = (fb_ptr - ctx.framebuffer) / 3;
        size_t l = idx % 99;
        size_t t = idx / 99;
        uint32_t code = get_next_code();

        if (code == clear_code) {
            init_code_table(table_size);
            // todo why
            // code_table[clear_code] = u8string(1, clear_code);
            // code_table[end_code] = u8string(1, end_code);
            code_size = min_code_size + 1;
            continue;
        } else if (code == end_code || fb_ptr - ctx.framebuffer >= ctx.buf_size) {
            break;
        }

        // lzw decoding
        if (code_table.contains(code)) {
            u8string indexes = code_table[code];
            for (size_t i = 0; i < indexes.size(); i++) {
                memcpy(fb_ptr, &ctx.gct[indexes[i]*3], 3);
                fb_ptr += 3;
            }
            code_table[code_table.size()] = code_table[prev_code] + u8string(1, indexes[0]);
            prev_code = code;
        } else {
            u8string prev_indexes = code_table[prev_code];
            u8string new_indexes = prev_indexes + u8string(1, prev_indexes[0]);
            for (size_t i = 0; i < new_indexes.size(); i++) {
                memcpy(fb_ptr, &ctx.gct[new_indexes[i]*3], 3);
                fb_ptr += 3;
            }
            code_table[code_table.size()] = new_indexes;
            prev_code = code;
        }
        if (code_table.size() == (1 << code_size)) {
            code_size++;
        }
    }
    // for (size_t i = 0; i < ctx.framebuffer.size(); i++) {
    //     printf("%03d ",(unsigned char)ctx.framebuffer[i]);
    //     if (i % 30 == 29) printf("\n");
    // }
}

void decode_frame(ifstream& file, const dec_ctx_t& ctx) {
    // graphic control extension
    read_assert_str_equal(file, "\x21\xf9\x04", "graphic control extension header error");
    gce_t gce;
    file.read((char*)gce.raw, sizeof(gce.raw));
    read_assert_str_equal(file, "\x00", "graphic control extension block terminal error");

    // image descriptor
    read_assert_str_equal(file, "\x2c", "graphic control extension block terminal error");
    image_desc_t image_desc;
    file.read((char*)image_desc.raw, sizeof(image_desc.raw));

    // lzw-encoded block
    uint8_t min_code_size = ctx.cr + 1;
    read_assert_num_equal(file, min_code_size, "lzw min-code-size error");

    lzw_unpack_decode(file, ctx);
}

int main(int argc, char** argv) {
    assert(argc >= 2 && "no input file");
    char* filename = argv[1];
    ifstream file(filename, ios::binary);
    assert(file.is_open() && "open file error");

    read_assert_str_equal(file, "GIF89a", "not gif89 file");
    // logical screen descriptor
    lsd_t lsd;
    file.read((char*)lsd.raw, sizeof(lsd.raw));
    // global color table
    uint16_t gct_real_size = (1 << (lsd.packed.gct_sz + 1)) * 3;
    string gct(gct_real_size, ' ');
    file.read(&gct[0], gct_real_size);

    // framebuffer
    size_t buf_size = lsd.w*lsd.h*3;
    char* framebuffer = new char[buf_size];
    // Netscape Looping Application Extension todo this block is optional
    read_assert_str_equal(file, "\x21\xff\x0b", "netscape looping application extension header error");
    read_assert_str_equal(file, "NETSCAPE2.0", "netscape looping application extension identifier error");
    read_assert_str_equal(file, "\x03\x01\x00\x00\x00", "netscape looping application extension content error");

    // while (file.peek() != 0x3b) {
    dec_ctx_t ctx = { lsd.packed.cr, gct, framebuffer, buf_size };
    decode_frame(file, ctx);

    file.close();
    // draw decoded framebuffer with sdl
    // todo sdl error handling
    SDL_Init(SDL_INIT_EVERYTHING);
    SDL_Window* window = SDL_CreateWindow("gif decoder", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, lsd.w, lsd.h, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    SDL_Surface* surf = SDL_CreateRGBSurfaceFrom(ctx.framebuffer, lsd.w, lsd.h, 3 * 8, lsd.w * 3, 0x0000FF, 0x00FF00, 0xFF0000, 0);
    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
    SDL_FreeSurface(surf);
    stringstream s;

    int window_w, window_h, mouse_x, mouse_y, idx;
    unsigned char r,g,b;
    while (1) {
	SDL_Event e;
        if (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                break;
            }
            if (e.type == SDL_MOUSEMOTION) {
                s.seekp(0);
                SDL_GetWindowSize(window, &window_w, &window_h);
                mouse_x = e.motion.x * lsd.w / window_w;
                mouse_y = e.motion.y * lsd.h / window_h;
                s << mouse_x << ' ' << mouse_y << ' ';
                idx = (lsd.w * mouse_y + mouse_x) * 3;
                r = ctx.framebuffer[idx];
                g = ctx.framebuffer[idx+1];
                b = ctx.framebuffer[idx+2];
                s << r + 0 << ' ' << g + 0 << ' ' << b + 0 << endl;
                SDL_SetWindowTitle(window, s.str().c_str());
            }
        }
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, tex, NULL, NULL);
        SDL_RenderPresent(renderer);
    }
    SDL_DestroyTexture(tex);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    delete[] framebuffer;
}
