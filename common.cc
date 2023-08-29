#include "common.h"
#include <cmath>
#include <fstream>

uint16_t read_u16_be(std::ifstream& file) {
    uint8_t u1, u2;
    file.read((char*)&u1, 1);
    file.read((char*)&u2, 1);
    return (u1 << 8) | u2;
}

void write_u16_be(uint16_t num) {
    uint8_t u1 = num >> 8;
    uint8_t u2 = num & 0xff;
    printf("%c%c", u1,u2);
}

uint16_t pad_8x(uint16_t input) {
    float mul = ceilf((float)input / 8.0);
    return (uint16_t)mul * 8;
}
uint16_t pad_16x(uint16_t input) {
    float mul = ceilf((float)input / 16.0);
    return (uint16_t)mul * 16;
}
