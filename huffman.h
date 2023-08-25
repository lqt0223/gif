#ifndef HUFFMAN
#define HUFFMAN

#include <map>
#include <string>
#include <utility>
#include <vector>
#include "node.h"

class HuffmanTree {
public:
    Node* root;
    void init_with_nb_and_symbols(const char nb_sym[16], const char* symbols);
    // char decode(unsigned int code, int n);
    // void all_nodes(); 
    ~HuffmanTree();
};

#endif
