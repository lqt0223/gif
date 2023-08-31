#include "huffman_enc.h"
#include <string>
#include <utility>

void HuffmanEnc::init_with_spec(const char nb_sym[16], const unsigned char* symbols) {
    auto huffmanCode = 0;
    for (int numBits = 1; numBits <= 16; numBits++) {
        // ... and each code of these bitsizes
        for (int i = 0; i < nb_sym[numBits - 1]; i++) // note: numCodes array starts at zero, but smallest bitsize is 1
            this->store.insert({ *symbols++, { numBits, huffmanCode++ } });

        // next Huffman code needs to be one bit wider
        huffmanCode <<= 1;
    }
}
// the constructor with entries
void HuffmanEnc::init_with_entries(const huffman_table_t& table){
    for (auto entry: table) {
        this->store.insert(entry);
    }
}

// convert from table to spec
std::pair<std::string, std::string> HuffmanEnc::to_spec() const {
    std::string code_lengths(16, 0);
    std::string symbols = this->ordered_table_symbols();
    for (auto [category, code_info]: this->store) {
        auto [code_length, code] = code_info;
        code_lengths[code_length - 1]++;
    }
    return { code_lengths, symbols };
}

void HuffmanEnc::dump() {
    for (auto entry: this->store) {
        printf("%d %d %d\n", entry.first, entry.second.first, entry.second.second);
    }
}


std::pair<uint8_t, uint16_t> HuffmanEnc::at(size_t i) {
    return this->store.at(i);
}

// return ordered table symbols from table
std::string  HuffmanEnc::ordered_table_symbols() const{
    std::vector<huffman_table_entry_t> vec;
    for (huffman_table_entry_t entry: this->store) {
        vec.push_back({ entry.first, { entry.second.first, entry.second.second } });
    }
    sort(vec.begin(), vec.end(), [](huffman_table_entry_t a, huffman_table_entry_t b) {
        return a.second.second < b.second.second;
    });
    std::string output;
    for (huffman_table_entry_t entry: vec) {
        output += entry.first;
    }
    return output;
}
