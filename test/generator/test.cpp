#include "generator.h"
#include <iostream>
using namespace std;

using NodePtr = shared_ptr<Node>;

int main() {
    Parser parser(nullptr);
    Generator gen("a.asm", nullptr);
    // Node* node1 = new IntNode(nullptr, type_int, -111);
    // node1->codegen(gen);

    // Node* node2 = new Node(NK_ERROR, nullptr, nullptr);
    // node2->codegen(gen);

    // Node* node3 = new FloatNode(nullptr, type_double, 12.34);
    // node3->codegen(gen);

    // Node* node4 = new StringNode(nullptr, type_char, "哈哈shabi\n\t\r");
    // node4->codegen(gen);

    // LocalVarNode* node5 = new LocalVarNode(nullptr, type_bool, "a");
    // node5->init_list = vector<NodePtr>({make_init_node(nullptr, type_bool, make_int_node(nullptr, type_int, 111), 0)});
    // node5->codegen(gen);

    NodePtr node6 = parser.make_binop(nullptr, '<', make_float_node(nullptr, type_double, 1.1), make_int_node(nullptr, type_uint, 1));
    cout << node6->type->to_string() << endl;
    node6->codegen(gen);
}