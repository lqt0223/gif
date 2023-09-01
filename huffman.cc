#include "huffman.h"
#include <vector>

HuffmanTree::~HuffmanTree() {
    if (this->root != nullptr) {
        Node::free(this->root);
    }
}
// the nodes created here should all use dynamic allocation and should only be deallocated with the tree
HuffmanTree::HuffmanTree(char nb_sym[16], const char* symbols) {
    this->root = Node::new_node();
    Node* root_left = Node::new_node();
    Node* root_right = Node::new_node();
    this->root->left = root_left;
    this->root->right = root_right;
    std::vector<Node*> depth_queue { root_left, root_right };
    
    size_t j = 0;
    for (size_t depth = 0; depth < 16; depth++) {
        char nb_sym_in_depth = nb_sym[depth];

        size_t queue_prev_length = depth_queue.size();
        for (size_t i = 0; i < queue_prev_length; i++) {
            Node* node = depth_queue[i];
            if (nb_sym_in_depth == 0) {
                Node* left = Node::new_node();
                Node* right = Node::new_node();
                node->left = left;
                node->right = right;
                depth_queue.push_back(left);
                depth_queue.push_back(right);
            } else {
                node->letter = symbols[j];
                j++;
                nb_sym_in_depth--;
            }
        }
        for (size_t i = 0; i < queue_prev_length; i++) {
            depth_queue.erase(depth_queue.begin());
        }
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
void HuffmanTree::all_nodes() {
    std::string path;
    Node::traverse_tree(this->root, path);
    printf("\n");
}
//
