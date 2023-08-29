#include <fstream>
#define read_assert_str_equal(file, buf, str, err) \
    file.read(buf, sizeof(str) - 1); \
    assert(!memcmp(buf, str, sizeof(str) - 1) && err)

#define read_assert_num_equal(file, buf, num, err) \
    file.read(buf, 1); \
    assert(buf[0] == num && err)

uint16_t read_u16_be(std::ifstream& file);
void write_u16_be(uint16_t num);

// pad a number by rounding up to the nearest integer which is divisible by 8
uint16_t pad_8x(uint16_t input);
uint16_t pad_16x(uint16_t input);
