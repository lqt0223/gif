#include "gif.h"
#include "lzw.h"
#include <cstdio>
#include <cstring>
#include <iostream>

// todo optim
void read_u8(std::ifstream* stream, uint8_t* u8) {
    stream->read((char*)u8, 1);
}

void read_u8_n(std::ifstream* stream, uint8_t* u8, size_t n) {
    stream->read((char*)u8, n);
}

void read_u16_le(std::ifstream* stream, uint16_t* u16) {
    char u8[2];
    stream->read(u8, 2);
    *u16 = (uint8_t)u8[1] * 256 + (uint8_t)u8[0];
}

bool equals_u82_u16(uint8_t a, uint8_t b, uint16_t c) {
    return a * 256 + b == c;
}

#define skip_g(stream, x) stream.seekg(x)
#define skip(stream, x) stream.seekg(x, std::ios::cur)

Gif::Gif(std::string filename) {
    this->stream = std::ifstream(filename,std::ios::binary);
    if (!this->stream.is_open()) {
        throw "cannot open";
    }
    // --- header ---
    // this->stream.seekg(6);
    skip_g(this->stream, 6); // skip the header 'GIF89a'

    // --- lsd (logical screen discriptor) ---
    // w, h is encoded as 2 byte in file (little endian)
    read_u16_le(&this->stream, &this->lsd.w);
    read_u16_le(&this->stream, &this->lsd.h);
    
    uint8_t lsd_packed;
    read_u8(&this->stream, &lsd_packed);
    this->lsd.has_gct = lsd_packed >> 7;
    this->lsd.cr = ((lsd_packed >> 4) & 0b111) + 1;
    this->lsd.sort = ((lsd_packed >> 3) & 1);
    this->lsd.gct_sz = 1 << ((lsd_packed & 0b111) + 1);

    read_u8(&this->stream, &this->lsd.bci);
    read_u8(&this->stream, &this->lsd.par);

    // --- gct (optional global color table) ---
    if (this->lsd.has_gct) {
        this->gct = new uint8_t[this->lsd.gct_sz*3];
        this->stream.read((char*)this->gct, this->lsd.gct_sz*3);
    }
    
    // --- handling any data blocks --
    uint8_t label1, label2;
    while (1) {
        read_u8(&this->stream, &label1);
        // trailer
        if (label1 == 0x3b) {
            break;
        // image descriptor && image data
        } else if (label1 == 0x2c) {
            // image descriptor
            uint16_t l, t, w, h;
            bool has_lct, interlace, sort;
            uint8_t lct_sz;
            read_u16_le(&this->stream, &l);
            read_u16_le(&this->stream, &t);
            read_u16_le(&this->stream, &w);
            read_u16_le(&this->stream, &h);
            uint8_t id_packed;
            read_u8(&this->stream, &id_packed);

            has_lct = id_packed >> 7;
            interlace = (id_packed >> 6) & 1;
            sort = (id_packed >> 5) & 1;
            lct_sz = has_lct ? (1 << ((id_packed & 0b111) + 1)) : 0;

            // lzw compressed image data
            uint8_t lzw_min, lzw_size;
            read_u8(&this->stream, &lzw_min);
            read_u8(&this->stream, &lzw_size);

            uint8_t* lzw_encoded_data = new uint8_t[lzw_size];
            read_u8_n(&this->stream, lzw_encoded_data, lzw_size);

            uint8_t* lzw_decoded_data = new uint8_t[4096];  // todo malloc size
            lzw_decode(lzw_encoded_data, lzw_size, lzw_decoded_data);
        // 2-byte labels
        } else {
            read_u8(&this->stream, &label2);

            // special-purpose block application extension
            // any general cases todo
            if (equals_u82_u16(label1, label2, 0x21ff)) {
                skip(this->stream, 1); // skip block size
                uint8_t ap_id[11];
                read_u8_n(&this->stream, ap_id, 11);
                // netscape loop extension
                if (!strcmp((char*)ap_id, "NETSCAPE2.0")) {
                    skip(this->stream, 2); // skip fixed value in block
                    read_u16_le(&this->stream, &this->loop);
                    skip(this->stream, 1); // skip block_end
                }
            // graphic control extension (multiple) todo
            } else if (equals_u82_u16(label1, label2, 0x21f9)) {
                skip(this->stream, 6); // skip the whole block todo
            // image descriptor
            }  else {
                // todo
                break;
            }
        }
    }
    // --- data (0 or n) ---
}
