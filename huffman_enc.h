// the huffman table to used with encoder
#include "jpeg_tables.h"
#include <vector>

class HuffmanEnc {
    huffman_table_t store;
public:
    // the constructor with specification (one line byte streams in the jfif)
    void init_with_spec(const char nb_sym[16], const unsigned char* symbols);
    // the constructor with entries
    void init_with_entries(const huffman_table_t& table);
    std::string ordered_table_symbols() const;
    std::pair<std::string, std::string> to_spec() const;
    std::pair<uint8_t, uint16_t> at(size_t i);
    void dump();
};
