#include <fstream>
#define read_assert_str_equal(file, buf, str, err) \
    file.read(buf, sizeof(str) - 1); \
    assert(!memcmp(buf, str, sizeof(str) - 1) && err)

#define read_assert_num_equal(file, buf, num, err) \
    file.read(buf, 1); \
    assert(buf[0] == num && err)

uint16_t read_u16_be(std::ifstream& file);
