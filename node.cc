#include "node.h"
#include <iostream>

Node::Node(): letter(-1), left(nullptr), right(nullptr), parent(nullptr) {}
Node* Node::new_node() {
    Node* n_node = new Node;
    n_node->left = nullptr;
    n_node->right = nullptr;
    n_node->parent = nullptr;
    n_node->letter = -1;
    return n_node;
}
// recursively free itself and descendants
void Node::free(Node* node) { // NOLINT(misc-no-recursion)
    if (node != nullptr) {
        free(node->left);
        free(node->right);
        delete node;
    }
}
void Node::setLeft(Node* _left) {
    left = _left;
    _left->parent = this;
}
void Node::setRight(Node* _right) {
    right = _right;
    _right->parent = this;
}
Node* Node::getSiblingRight() {
    if (!this->parent) {
        return nullptr;
    }
    // if self is a left node
    if (this->parent->left == this) {
        return this->parent->right;
    // else if self is a right node
    } else if (this->parent->right == this) {
        int count = 0;
        Node* ptr = this;
        while (ptr->parent != nullptr && ptr->parent->right == ptr) { ptr = ptr->parent;
            count++;
        }
        if (ptr->parent == nullptr) {
            return nullptr;
        }
        ptr = ptr->parent->right;
        while (count > 0) {
            ptr = ptr->left;
            count--;
        }
        return ptr;
    }
    return nullptr;
}

void Node::traverse_tree(Node* node, std::string& path) { // NOLINT(misc-no-recursion)
    // if it is a leaf node, log
    if (node->left == nullptr && node->right == nullptr && node->letter != -1) {
        std::cout << +(unsigned char)node->letter << ": " << path << std::endl;
    } else {
        if (node->left) {
            std::string _path = std::string(path);
            _path.push_back('0');
            traverse_tree(node->left, _path);
        }
        if (node->right) {
            std::string _path = std::string(path);
            _path.push_back('1');
            traverse_tree(node->right, _path);
        }
    }
}


