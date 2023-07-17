#include "lzw.h"
#include <cstdint>
#include <iostream>
#include <map>
#include <unordered_map>

void lzw_encode_naive(uint8_t* decoded, uint8_t size, uint8_t* encoded) {
    // first encoded
    uint8_t old_phrase, new_phrase;
    std::unordered_map<uint8_t*, uint8_t> table;
    encoded[0] = decoded[0];
    old_phrase = decoded[0];
    new_phrase = decoded[0];
    for (int i = 1; i < size; i++) {
        encoded[i] = decoded[i];
        old_phrase = new_phrase;
    }
}

void lzw_decode(uint8_t* encoded, uint8_t size, uint8_t* decoded) {
    for (int i = 0; i < size; i++) {
         std::cout << +encoded[i] << std::endl;
    }
    // std::cout << std::endl;
}
