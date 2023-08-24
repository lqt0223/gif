#include "common.h"
#include <fstream>

uint16_t read_u16_be(std::ifstream& file) {
    uint8_t u1, u2;
    file.read((char*)&u1, 1);
    file.read((char*)&u2, 1);
    return (u1 << 8) | u2;
}
