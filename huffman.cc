#include <cmath>
#include <iostream>
#include <map>
#include <string>
#include <utility>
#include <vector>
#include "huffman.h"

HuffmanTree::~HuffmanTree() {
    Node::free(this->root);
}
// the nodes created here should all use dynamic allocation and should only be deallocated with the tree
void HuffmanTree::init_with_nb_and_symbols(char nb_sym[16], char* symbols) {
    Node* left = Node::new_node();
    Node* right = Node::new_node();
    this->root = Node::new_node();
    this->root->freq = 0;
    this->root->letter = -1;
    this->root->setLeft(left);
    this->root->setRight(right);
    Node* cur = left;
    Node* next_cur;
    size_t w = 0;
    size_t i = 0, j = 0; // the index pointer to nb_sym_per_depth and symbols
    size_t k = 1 << 16; // the counter to path, will be parsed as binary path from root to leaf node
    for (size_t depth = 0; depth < 16; depth++) {
        Node* left_most = nullptr;
        char nb_sym_in_depth = nb_sym[depth];
        size_t total_nodes_in_depth = 1 << (depth + 1);
        size_t empty_nodes_in_depth = total_nodes_in_depth - nb_sym_in_depth;
        
        // printf("%lu %d ", depth + 1, nb_sym);
        for (size_t l = 0; l < nb_sym_in_depth; l++) {
            // add leaf node with letter to tree
            cur->letter = symbols[j];
            j++;
            next_cur = cur->getSiblingRight();
            if (next_cur == nullptr) {
                break;
            }
            cur = next_cur;
        }
        while (true) {
            Node* new_left = Node::new_node();
            Node* new_right = Node::new_node();
            cur->setLeft(new_left);
            cur->setRight(new_right);
            if (left_most == nullptr) left_most = new_left;
            next_cur = cur->getSiblingRight();
            if (next_cur == nullptr) {
                break;
            }
            cur = next_cur;
            w++;
        }
        cur = left_most;
    }
}

void traverse_tree_and_add(Node* node, unsigned int code, uint8_t n, huffman_table_t& table) {
    if (node->left == nullptr && node->right == nullptr && node->letter != -1) {
        huffman_code_t code_info = { code, n };
        table[code_info] = node->letter;
    } else {
        code <<= 1;
        n += 1;
        if (node->left) {
            traverse_tree_and_add(node->left, code, n,table);
        }
        if (node->right) {
            code += 1;
            traverse_tree_and_add(node->right, code, n,table);
        }
    }
}

void HuffmanTree::fill_table(huffman_table_t& table) {
    traverse_tree_and_add(this->root, 0, 0, table);
}
char HuffmanTree::decode(unsigned int code, int n) {
    Node* cur = this->root;
    for (int i = n - 1; i >= 0; i--) {
        int mask = 1 << i;
        bool right = !!(code & mask);
        if (right) {
            cur = cur->right;
        } else {
            cur = cur->left;
        }
    }
    return cur->letter;
}

// for debugging
void HuffmanTree::all_nodes() {
    std::string path;
    Node::traverse_tree(this->root, path);
}

