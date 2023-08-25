#include "huffman.h"

HuffmanTree::~HuffmanTree() {
    Node::free(this->root);
}
// the nodes created here should all use dynamic allocation and should only be deallocated with the tree
void HuffmanTree::init_with_nb_and_symbols(const char nb_sym[16], const char* symbols) {
    Node* left = Node::new_node();
    Node* right = Node::new_node();
    this->root = Node::new_node();
    this->root->letter = -1;
    this->root->setLeft(left);
    this->root->setRight(right);
    Node* cur = left;
    Node* next_cur;
    size_t w = 0;
    size_t j = 0;
    for (size_t depth = 0; depth < 16; depth++) {
        Node* left_most = nullptr;
        char nb_sym_in_depth = nb_sym[depth];

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

// char HuffmanTree::decode(unsigned int code, int n) {
//     Node* cur = this->root;
//     for (int i = n - 1; i >= 0; i--) {
//         int mask = 1 << i;
//         bool right = !!(code & mask);
//         if (right) {
//             cur = cur->right;
//         } else {
//             cur = cur->left;
//         }
//     }
//     return cur->letter;
// }
//
// // for debugging
// void HuffmanTree::all_nodes() {
//     std::string path;
//     Node::traverse_tree(this->root, path);
// }
//
