#include "node.h"
#include <iostream>

Node::Node(): freq(-1), letter(-1), left(nullptr), right(nullptr), parent(nullptr) {};
Node::Node(int f, char c, Node* lf, Node* rt) : freq(f), letter(c), parent(nullptr) {
    setLeft(lf);
    setRight(rt);
};
Node::Node(int f, char c) : freq(f), letter(c), left(nullptr), right(nullptr), parent(nullptr) {};
Node* Node::new_node() {
    Node* n_node = new Node;
    n_node->left = nullptr;
    n_node->right = nullptr;
    n_node->parent = nullptr;
    n_node->freq = -1;
    n_node->letter = -1;
    return n_node;
};
Node* Node::copy(const Node* root) {
    Node *new_root;
    if(root!=NULL){
        new_root= Node::new_node();
        new_root->freq=root->freq;
        new_root->letter=root->letter;
        new_root->parent=root->parent;
        Node* _left = copy(root->left);
        new_root->left=_left;
        if (_left != nullptr) _left->parent = new_root;

        Node* _right = copy(root->right);
        new_root->right=_right;
        if (_right != nullptr) _right->parent = new_root;
    } else return nullptr;
    return new_root;
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
        Node* nptr = this;
        while (nptr->parent != nullptr && nptr->parent->right == nptr) {
            nptr = nptr->parent;
            count++;
        }
        if (nptr->parent == nullptr) {
            return nullptr;
        }
        nptr = nptr->parent->right;
        while (count > 0) {
            nptr = nptr->left;
            count--;
        }
        return nptr;
    }
    return nullptr;
};

void Node::traverse_tree(Node* node, std::string& path) {
    // if it is a leaf node, log
    if (node->left == nullptr && node->right == nullptr && node->letter != -1) {
        std::cout << node->letter << ": " << path << std::endl;
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


