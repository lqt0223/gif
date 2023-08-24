#ifndef HUFFMAN
#define HUFFMAN

#include <map>
#include <string>
#include <utility>
#include <vector>
#include "node.h"

typedef struct HuffmanCode {
    unsigned int code; // the binary huffman code
    uint8_t length; // the length of this code
} huffman_code_t;

inline bool operator < ( const HuffmanCode& c1, const HuffmanCode& c2) {
    if (c1.code != c2.code) {
        return c1.code < c2.code;
    } else {
        return c1.length < c2.length;
    }
}

typedef std::map<huffman_code_t, char> huffman_table_t;

class HuffmanTree {
public:
    Node* root;
    HuffmanTree();
    HuffmanTree(char nb_sym[16], char* symbols);
    char decode(unsigned int code, int n);
    void fill_table(huffman_table_t& table);
};

#endif
