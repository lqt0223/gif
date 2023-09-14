#include "node.h"
#include <iostream>

void Node::traverse_tree(Node* node, std::string& path) { // NOLINT(misc-no-recursion)
    // if it is a leaf node, log
    if (node->left == nullptr && node->right == nullptr && node->letter != -1) {
        std::cout << +(unsigned char)node->letter << ": " << path << std::endl;
    } else {
        if (node->left) {
            std::string _path = std::string(path);
            _path.push_back('0');
            traverse_tree(node->left.get(), _path);
        }
        if (node->right) {
            std::string _path = std::string(path);
            _path.push_back('1');
            traverse_tree(node->right.get(), _path);
        }
    }
}


