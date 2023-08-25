#ifndef NODE
#define NODE

#include <string>
class Node {
public:
    char letter;
    Node* left;
    Node* right;
    Node* parent;
    Node();
    static Node* new_node();
    static void traverse_tree(Node* node, std::string& path);
    void setLeft(Node* _left);
    void setRight(Node* _right);
    Node* getSiblingRight(); 
    static void free(Node* node);
};

#endif
