#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

class BitStream {
    size_t bit_offset { 0 };
public:
    std::string store;
    void append_bit(uint8_t size, uint16_t bit);
};
