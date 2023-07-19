#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <iostream>

using namespace std;

template<typename T>
inline T i_ceil_div(T x, T y) {
    return x/y+(x%y != 0);
}

// integer ceil division operation
//#define i_ceil_div(x, y) (x/y + (x % y != 0))
// output any expression as string auto formatted
#define c_out(x) cout << x
// output any expression as a char (may emit invisible character)
#define c_out_raw(x) cout << (unsigned char)x
#define c_out_raw2(x, y) cout << (unsigned char)x << (unsigned char)y
#define c_out_raw3(x, y, z) cout << (unsigned char)x << (unsigned char)y << (unsigned char)z
// output an u16 number in 2 bytes, little-endian
#define c_out_u16_le(x) \
    uint8_t a = x & 0xff; \
    uint8_t b = (x >> 8) & 0xff; \
    c_out_raw2(a, b)

// declare bit fields such that msb is at last
typedef struct {
    uint8_t gct_sz: 3;
    uint8_t sort: 1;
    uint8_t cr: 3; // color resolution
    uint8_t has_gct: 1;
} lsd_packed_t;

// logical screen descriptor
typedef union __attribute__((__packed__)) {
    uint8_t raw[7];
    struct __attribute__((__packed__)) {
        uint16_t w;
        uint16_t h;
        lsd_packed_t packed;
        uint8_t bci; // background color index
        uint8_t par; // pixel aspect ratio
    };
} lsd_t;

typedef struct {
    uint8_t transparent: 1;
    uint8_t user_input: 1;
    uint8_t disposal: 3;
    uint8_t  : 3; // unused
} gce_packed_t;

// graphic control extension
typedef union {
    uint8_t raw[4];
    struct __attribute__((__packed__)) {
        gce_packed_t packed;
        uint16_t delay;
        uint8_t tran_index;
    };
} gce_t;

typedef struct {
    uint8_t lct_sz: 3;
    uint8_t  : 2; // unused
    uint8_t sort: 1;
    uint8_t interlace: 1;
    uint8_t has_lct: 1;
} image_desc_packed_t;

// image descriptor
typedef union {
    uint8_t raw[9];
    struct __attribute__((__packed__)) {
        uint16_t l;
        uint16_t t;
        uint16_t w;
        uint16_t h;
        image_desc_packed_t packed;
    };
} image_desc_t;

typedef vector<pair<uint16_t, uint8_t>> lzw_compressed_t;
typedef struct ppm_t {
    string header;
    string buffer;
    size_t width;
    size_t height;
} ppm_t;

void debug_lzw(lzw_compressed_t vec) {
    cout << endl;
    cout << "debugging lzw" << endl;
    for (size_t i = 0; i < 5; i++) {
        cout << i << " " << vec[i].first << " " << static_cast<char8_t >(vec[i].second) << endl;
    }
    for (size_t i = vec.size() - 5; i < vec.size(); i++) {
        cout << i << " " << vec[i].first << " " << static_cast<char8_t >(vec[i].second) << endl;
    }
}

template<typename T>
void debug_print_vec(vector<T> vec) {
    cout << endl;
    cout << "debugging vec" << endl;
    for (size_t i = 0; i < 5; i++) {
        cout << i << " " << static_cast<char8_t >(vec[i]) << endl;
    }
    for (size_t i = vec.size() - 5; i < vec.size(); i++) {
        cout << i << " " << static_cast<char8_t >(vec[i]) << endl;
    }
}

template<typename T>
[[maybe_unused]] void debug_print_str(basic_string<T> str) {
    for (size_t i = 0; i < 5; i++) {
        cout << i << " " << str[i] << endl;
    }
    for (size_t i = str.size() - 5; i < str.size(); i++) {
        cout << i << " " << str[i] << endl;
    }
}

void read_ppm2(char *filename, ppm_t *ppm) {
    ifstream file(filename, ios::binary);
    // get header, width, height substrings
    string header;
    size_t width, height, color_max;
    // >> operator can extract a value from stream, ignoring any preceding whitespaces
    file >> header >> width >> height >> color_max;
    size_t buf_size = width * height * 3;
    string buffer(buf_size, ' ');
    // before reading binary contents, ignore the trailing whitespace of color_max
    file.seekg(1, ios::cur);
    file.read(&buffer[0], (streamsize)buf_size);

    ppm->width = width;
    ppm->height = height;
    ppm->buffer = buffer;
    ppm->header = header;
}

template<typename K, typename V>
class CodeTable {
public:
    map<K, V> _map;

    void clear() {
        _map.clear();
    }

    void insert(K codes) {
        size_t next_code = _map.size();
        _map[codes] = next_code;
    }

    bool has(K codes) {
        return _map.contains(codes);
    }

    size_t size() {
        return _map.size();
    }

    V get(K codes) {
        return _map.at(codes);
    }

    // for debug
//    void get_r(V coded) {
//        for (pair<K, V> kv: _map) {
//            auto [k, v] = kv;
//            if (v == coded) {
//                size_t sz = k.size();
//                printf("debug %d => ", coded);
//                for (size_t i = 0; i < sz; i++) {
//                    printf("%d,", k[i]);
//                }
//                printf("\n");
//            }
//        }
//    }
};

static CodeTable<u16string, uint16_t> code_table;

void init_code_table() {
    code_table.clear();
    for (size_t i = 0; i < 256; i++) {
        u16string c;
        c.push_back(i);
        code_table.insert(c);
    }
}

static map<uint32_t, uint8_t> color_mapping;

void init_color_mapping() {
    /* generate a 252 colored (close-to 256) color_mapping */
    uint8_t i = 0;

    for (size_t b = 0; b < 256; b += 42) {
        for (size_t g = 0; g < 256; g += 51) {
            for (size_t r = 0; r < 256; r += 51) {
                uint32_t rgb = (b << 16) | (g << 8) | r;
                color_mapping[rgb] = i;
                i++;
            }
        }
    }
}

// todo boxes_1.ppm
lzw_compressed_t lzw_encode(const string& color_indexes) {
    vector<pair<uint16_t, uint8_t>> output;
    uint8_t min_code_size = 8;
    uint16_t clear_code = 1 << min_code_size;
    uint16_t end_code = clear_code + 1;
    u16string clear_codestr;
    clear_codestr.push_back(clear_code);
    u16string end_codestr;
    end_codestr.push_back(end_code);
    init_code_table();
    code_table.insert(clear_codestr);
    code_table.insert(end_codestr);

    u16string prev;
    uint8_t code_size = min_code_size + 1;

    output.emplace_back(clear_code, code_size);
    uint8_t first = color_indexes[0];
    prev.push_back(first);

    for (size_t i = 1; i < color_indexes.size(); i++) {
        uint8_t next = color_indexes[i];
        // add clear code and reset color table here
        if (code_table.size() == 4096) { // MAX_SIZE
            init_code_table();
            code_table.insert(clear_codestr);
            code_table.insert(end_codestr);
            output.emplace_back(clear_code, code_size);
            code_size = min_code_size + 1;
        }

        u16string next_str;
        next_str.push_back(next);
        u16string comb = prev + next_str;
        if (code_table.has(comb)) {
            prev.push_back(next);
        } else {
            uint16_t code = code_table.get(prev);
            output.emplace_back(code, code_size);
            // raise the code_size here
            if (code_table.size() == 1 << code_size) {
                code_size++;
            }
            // add new_code intro color map
            code_table.insert(comb);
            prev = next_str;
        }
    }
    if (!prev.empty()) {
        uint16_t code = code_table.get(prev);
        output.emplace_back(code, code_size);
    }
    output.emplace_back(end_code, code_size);
    return output;
}

vector<uint8_t> lzw_pack(const lzw_compressed_t& codes) {
    vector<uint8_t> output;
    uint32_t n_bit = 0;
    for (auto item: codes) {
        auto [code, code_size] = item;
        // the bit position - amount to shift left
        uint16_t shl = n_bit % 8;
        // the index position of bytestream where the byte output is emitted
        uint16_t index = n_bit / 8;
        if (index >= output.size()) output.push_back(0);
        // fill in the bits of code onto the right position
        output[index] |= (code << shl);
        // ensure that the result is not greater than a byte
        output[index] &= 0xff;
        // if there is any bits that go past the next byte
        bool pass_next_byte = shl + code_size > 8;
        if (pass_next_byte) {
            // output remaining bits onto next byte
            uint8_t remain_bits = code >> (8 - shl);
            if (index + 1 >= output.size()) {
                output.push_back(0);
            }
            output[index + 1] |= remain_bits;
        }
        // if there is any bits that go past the next-next byte
        bool pass_next_next_byte = shl + code_size > 16;
        if (pass_next_next_byte) {
            // output remaining bits onto next byte
            uint8_t remain_bits = code >> (16 - shl);
            if (index + 2 >= output.size()) output.push_back(0);
            output[index + 2] |= remain_bits;
        }
        n_bit += code_size;
    }
    return output;
}

template<typename T>
vector<vector<T>> make_chunk(const vector<T>& arr, size_t chunk_size) {
    vector<vector<T>> chunks;
    size_t num_of_chunks = i_ceil_div(arr.size(), chunk_size);
    for (size_t i = 0; i < num_of_chunks; i++) {
        vector<T> chunk;
        for (size_t j = 0; j < chunk_size; j++) {
            size_t idx = i * chunk_size + j;
            if (idx < arr.size()) {
                T item = arr[i * chunk_size + j];
                chunk.push_back(item);
            }
        }
        chunks.push_back(chunk);
    }
    return chunks;
}

void encode_frame(const string& frame, size_t w, size_t h) {
    string color_indexes;
    for (size_t i = 0; i < frame.size(); i += 3) {
        uint8_t r = frame[i + 0];
        uint8_t g = frame[i + 1];
        uint8_t b = frame[i + 2];

        r -= r % 51;
        g -= g % 51;
        b -= b % 42;
        uint32_t rgb = (b << 16) | (g << 8) | r;
        uint8_t idx = color_mapping[rgb];
        color_indexes.push_back((char)idx);
    }
    // gce
    gce_t gce;
    gce.packed.disposal = 0;
    gce.packed.user_input = 0;
    gce.packed.transparent = 0;
    gce.delay = 0;
    gce.tran_index = 0;
    c_out_raw3(0x21, 0xf9, 0x04); // block label
    for (uint8_t i : gce.raw) {
        c_out_raw(i);
    }
    c_out_raw(0x00); // block terminator
    // image_desc
    image_desc_t image_desc;
    image_desc.w = w;
    image_desc.h = h;
    image_desc.l = 0;
    image_desc.t = 0;
    image_desc.packed.has_lct = 0;
    image_desc.packed.interlace = 0;
    image_desc.packed.lct_sz = 0;
    image_desc.packed.sort = 0;
    c_out_raw(0x2c); // block label;
    for (uint8_t i : image_desc.raw) {
        c_out_raw(i);
    }
    // lzw_image_data_block
    c_out_raw(0x08); // lzw min code size
    lzw_compressed_t lzw_compressed = lzw_encode(color_indexes);
#ifdef DEBUG
    debug_lzw(lzw_compressed);
#endif
    vector<uint8_t> packed = lzw_pack(lzw_compressed);
#ifdef DEBUG
    debug_print_vec(packed);
#endif
    vector<vector<uint8_t>> chunks = make_chunk(packed, 255);
    for (const vector<uint8_t>& chunk: chunks) {
        c_out_raw(chunk.size());
        for (uint8_t elem: chunk) {
            c_out_raw(elem);
        }
    }
    c_out_raw(0x00); // end of block
}

void gif_encode(const vector<string>& frames, size_t w, size_t h) {
    init_color_mapping();
    // header
    c_out("GIF89a");
    // lsd
    lsd_t lsd;
    lsd.w = w;
    lsd.h = h;
    lsd.packed.has_gct = 1;
    lsd.packed.cr = 7;
    lsd.packed.sort = 0;
    lsd.packed.gct_sz = 7;
    lsd.bci = 0;
    lsd.par = 0;
    for (uint8_t i : lsd.raw) {
        c_out_raw(i);
    }
    // gct
    for (auto kv: color_mapping) {
        uint32_t color = kv.first;
        uint8_t b = (color & 0xff0000) >> 16;
        uint8_t g = (color & 0x00ff00) >> 8;
        uint8_t r = (color & 0x0000ff);
        c_out_raw3(r, g, b);
    }
    // 4 blacks for padding
    for (size_t i = 0; i < 4; i++) {
        c_out_raw3(0, 0, 0);
    }
    // Netscape Looping Application Extension
    c_out_raw3(0x21, 0xff, 0x0b);
    c_out("NETSCAPE2.0");
    c_out_raw2(0x03, 0x01);
    c_out_u16_le(0); // infinite loop
    c_out_raw(0x00);

    // encode frame
    for (const string& frame: frames) {
        encode_frame(frame, w, h);
    }

    c_out_raw(0x3b); // end of gif
}

typedef struct {
    char r;
    char g;
    char b;
} rgb_t;

string get_frame(size_t width, size_t height, size_t t, rgb_t (*fn)(size_t left, size_t top, size_t width, size_t height, size_t t)) {
    string output_frame;
    for (size_t top = 0; top < height; top++) {
        for (size_t left = 0; left < width; left++) {
            rgb_t rgb = fn(left, top, width, height, t);
            output_frame += rgb.r;
            output_frame += rgb.g;
            output_frame += rgb.b;
        }
    }
    return output_frame;
}

rgb_t shader(size_t left, size_t top, size_t width, size_t height, size_t t) {
    char r = (char)((float)left / (float)width * 255.0);
    char g = (char)((float)top / (float)height * 255.0);
    char b = (char)t;
    return rgb_t { r, g, b };
}

int main() {
    size_t W = 64;
    size_t H = 64;
    string f1 = get_frame(W, H, 0, &shader);
    string f2 = get_frame(W, H, 127, &shader);
    string f3 = get_frame(W, H, 255, &shader);

    vector<string> frames = {f1, f2, f3};

    gif_encode(frames, W, H);
    return 0;
}
