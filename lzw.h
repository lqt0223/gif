#include <cstdint>

void lzw_encode_naive(uint8_t* decoded, uint8_t size, uint8_t* encoded);
void lzw_decode(uint8_t* encoded, uint8_t size, uint8_t* decoded);
