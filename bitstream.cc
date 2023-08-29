#include "bitstream.h"

void BitStream::append_bit(uint8_t size, uint16_t bits) {
    for (uint8_t i = 0; i < size; i++) {
        uint8_t bit = bits >> (size - i - 1) & 1;
        size_t shift = this->bit_offset % 8;
        size_t index = this->bit_offset / 8;
        if (this->store.size() == 0 || this->store.size() == index) {
            this->store.push_back(0);
        }
        this->store[index] |= bit << (7 - shift);
        this->bit_offset++;

        // byte stuffing - replace ff as ff00
        char last = this->store[this->store.size() - 1];
        if (last == '\xff') {
            this->store.push_back(0);
            this->bit_offset += 8;
        }
    }
}
