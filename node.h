#ifndef NODE
#define NODE

#include <memory>
#include <string>
class Node {
public:
    char letter = -1;
    std::shared_ptr<Node> left = nullptr;
    std::shared_ptr<Node> right = nullptr;
    static void traverse_tree(Node* node, std::string& path);
};

#endif
