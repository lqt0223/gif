#ifndef NODE
#define NODE

#include <memory>
#include <string>
class Node {
public:
    char letter;
    std::shared_ptr<Node> left;
    std::shared_ptr<Node> right;
    Node();
    static void traverse_tree(Node* node, std::string& path);
};

#endif
