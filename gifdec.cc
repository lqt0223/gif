#include "gif.h"
#include <fstream>
#include <SDL2/SDL.h>
#include <sstream>
#include <vector>

using namespace std;

char buf[256];
map<uint16_t, u8string> code_table;
typedef struct {
    uint8_t cr;
    const string& gct;
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

u8string bytes_from_all_subblocks(istream& file) {
    u8string bytes;
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
    return bytes;
}

// write as argb for sdl rendering
void write_to_fb(char*& fb_ptr, const char8_t* indexes, size_t n, const char* gct) {
    for (size_t i = 0; i < n; i++) {
        *fb_ptr = 0;
        fb_ptr += 1;
        memcpy(fb_ptr, gct + (indexes[i] * 3), 3);
        fb_ptr += 3;
    }
}

void lzw_unpack_decode(ifstream& file, const dec_ctx_t& ctx, char* framebuffer) {
    char* fb_ptr = framebuffer;

    uint8_t min_code_size = ctx.cr + 1;
    uint16_t clear_code = 1 << min_code_size;
    uint16_t end_code = clear_code + 1;
    uint16_t table_size = 1 << min_code_size;
    uint32_t n_bit = 0;
    uint32_t code, prev_code;
    uint8_t code_size;
    u8string indexes, prev_indexes, new_indexes;

    u8string bytes = bytes_from_all_subblocks(file); // packed bytes from all sub blocks

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

    code_size = min_code_size + 1;
    // first code is clear_code - skip
    n_bit += code_size;
clear:
    init_code_table(table_size);
    code_table[clear_code] = u8string(1, clear_code);
    code_table[end_code] = u8string(1, end_code);
    code_size = min_code_size + 1;

    // extract second code from bytes
    code = get_next_code();

    indexes = code_table[code];
    const char* gct_data = ctx.gct.data();
    write_to_fb(fb_ptr, indexes.data(), indexes.size(), gct_data);
    prev_code = code;

    while (1) {
        code = get_next_code();

        if (code == clear_code) {
            goto clear;
        } else if (code == end_code || fb_ptr - framebuffer >= ctx.buf_size) {
            break;
        }

        // lzw decoding
        if (code_table.contains(code)) {
            indexes = code_table[code];
            write_to_fb(fb_ptr, indexes.data(), indexes.size(), gct_data);
            code_table[code_table.size()] = code_table[prev_code] + u8string(1, indexes[0]);
            prev_code = code;
        } else {
            prev_indexes = code_table[prev_code];
            new_indexes = prev_indexes + u8string(1, prev_indexes[0]);
            write_to_fb(fb_ptr, new_indexes.data(), new_indexes.size(), gct_data);
            code_table[code_table.size()] = new_indexes;
            prev_code = code;
        }
        if (code_table.size() == (1 << code_size)) {
            code_size++;
        }
    }
}

void decode_frame(ifstream& file, const dec_ctx_t& ctx, char* framebuffer) {
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

    lzw_unpack_decode(file, ctx, framebuffer);
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
    size_t buf_size = lsd.w*lsd.h*4;
    char* framebuffer = new char[buf_size];
    vector<char*> frames;
    // Netscape Looping Application Extension todo this block is optional
    read_assert_str_equal(file, "\x21\xff\x0b", "netscape looping application extension header error");
    read_assert_str_equal(file, "NETSCAPE2.0", "netscape looping application extension identifier error");
    read_assert_str_equal(file, "\x03\x01\x00\x00\x00", "netscape looping application extension content error");

    while (file.peek() != 0x3b) {
        char* frame = new char[buf_size];
        dec_ctx_t ctx = { lsd.packed.cr, gct, buf_size };
        decode_frame(file, ctx, frame);
        frames.push_back(frame);
    }

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
    size_t total_frames = frames.size();
    while (1) {
        current = SDL_GetTicks();
        ts = current - start;
        num_of_frame = (ts / 16) % total_frames;
        SDL_LockTexture(tex, NULL, (void**)&framebuffer, &pitch);
        memcpy(framebuffer, frames[num_of_frame], buf_size);
        SDL_UnlockTexture(tex);
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
                r = framebuffer[idx];
                g = framebuffer[idx+1];
                b = framebuffer[idx+2];
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
    file.close();
    delete[] framebuffer;
}
