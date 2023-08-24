#ifndef NODE
#define NODE

#include <string>
class Node {
public:
    int freq;
    char letter;
    Node* left;
    Node* right;
    Node* parent;
    Node();
    Node(int f, char c, Node* lf, Node* rt);
    Node(int f, char c);
    static Node* new_node();
    static Node* copy(const Node* root);
    static void traverse_tree(Node* node, std::string& path);
    void setLeft(Node* _left);
    void setRight(Node* _right);
    Node* getSiblingRight(); 
};

#endif
