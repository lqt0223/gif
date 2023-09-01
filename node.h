#ifndef NODE
#define NODE

#include <string>
class Node {
public:
    char letter;
    Node* left;
    Node* right;
    Node();
    static Node* new_node();
    static void traverse_tree(Node* node, std::string& path);
    static void free(Node* node);
};

#endif
