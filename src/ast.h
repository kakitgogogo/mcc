#pragma once

#include <string.h>
#include <fstream>
#include <unistd.h>
#include "token.h"
#include "type.h"
#include "scope.h"
#include "generator.h"

class Node;
class Scope;
class Generator;

using NodePtr = std::shared_ptr<Node>;
using TokenPtr = std::shared_ptr<Token>;

char* make_tmpname();
char* make_label();
char* make_static_label(char *name);

enum NodeKind {
    // keywords and punctators are alse useful here, so keep their position
    NK_START = TIDENT, // TIDENT is just larger then the last punctuator

    NK_ERROR,

    NK_LITERAL,
    NK_LOCAL_VAR,
    NK_GLOBAL_VAR,
    NK_FUNC_DESG, // function designator
    NK_TYPEDEF,
    NK_FUNC_CALL,
    NK_FUNCPTR_CALL,
    NK_STRUCT_MEMBER, // get struct or union member
    NK_TERNARY,
    NK_INIT,
    NK_DECL,
    NK_IF,
    NK_JUMP,
    NK_LABEL,
    NK_COMPOUND_STMT,
    NK_RETURN,
    NK_FUNC_DEF,

    NK_CAST, // explicit conversion
    NK_CONV, // implicit conversion
    NK_DEREF, // dereference
    NK_COMPUTED_GOTO,

    // some punctators have multi-meanings on the semantic level
    NK_PRE_INC,
    NK_PRE_DEC,
    NK_POST_INC,
    NK_POST_DEC,
    NK_ADDR,
    NK_LABEL_ADDR,

    NK_SAL,
    NK_SAR,
    NK_SHR,
};

class Node: public std::enable_shared_from_this<Node> {
public:
    Node(int kind, Type* ty, TokenPtr first_token): 
        kind(kind), type(ty), first_token(first_token) {}

    virtual ~Node() {}

    // When global_label is not null, it indicates that the address may be calculated 
    // (this operation is used for initialization of global variables).
    virtual long long eval_int(char** global_label = nullptr);
    virtual double eval_float();

    virtual void codegen(Generator& gen);

    virtual char* to_dot_graph(FILE* fout);
public:
    int kind;
    Type* type;
    TokenPtr first_token;
};

class IntNode: public Node {
public:
    IntNode(TokenPtr first_token, Type* ty, long long value): 
        Node(NK_LITERAL, ty, first_token), value(value) {}

    virtual long long eval_int(char** global_label = nullptr);
    virtual double eval_float();

    virtual void codegen(Generator& gen);

    virtual char* to_dot_graph(FILE* fout);
public:
    long long value;
};

class FloatNode: public Node {
public:
    FloatNode(TokenPtr first_token, Type* ty, double value): 
        Node(NK_LITERAL, ty, first_token), value(value) {}

    virtual long long eval_int(char** global_label = nullptr);
    virtual double eval_float();

    virtual void codegen(Generator& gen);

    virtual char* to_dot_graph(FILE* fout);
public:
    double value;
    char* label = nullptr;
};

class StringNode: public Node {
public:
    StringNode(TokenPtr first_token, Type* ty, char* value): 
        Node(NK_LITERAL, ty, first_token), value(value) {}

    virtual void codegen(Generator& gen);

    virtual char* to_dot_graph(FILE* fout);
public:
    char* value;
    char* label = nullptr;
};

// local varirant include temporary variant
class LocalVarNode: public Node {
public:
    LocalVarNode(TokenPtr first_token, Type* ty, char* name): 
        Node(NK_LOCAL_VAR, ty, first_token), var_name(name) {}

    virtual void codegen(Generator& gen);

    virtual char* to_dot_graph(FILE* fout);
public:
    char* var_name;
    int offset;
    std::vector<NodePtr> init_list;
};

class GlobalVarNode: public Node {
public:
    GlobalVarNode(TokenPtr first_token, Type* ty, char* name): 
        Node(NK_GLOBAL_VAR, ty, first_token), var_name(name), 
        global_label(name) {}

    virtual long long eval_int(char** global_label = nullptr);

    virtual void codegen(Generator& gen);

    virtual char* to_dot_graph(FILE* fout);
public:
    char* var_name;
    char* global_label;
};

class FuncDesignatorNode: public Node {
public:
    FuncDesignatorNode(TokenPtr first_token, Type* ty, char* name): 
        Node(NK_FUNC_DESG, ty, first_token), func_name(name) {}

    virtual void codegen(Generator& gen);

    virtual char* to_dot_graph(FILE* fout);
public:
    char* func_name;
};

class TypedefNode: public Node {
public:
    TypedefNode(TokenPtr first_token, Type* ty, char* name): 
        Node(NK_TYPEDEF, ty, first_token), name(name) {}

    virtual void codegen(Generator& gen);

    virtual char* to_dot_graph(FILE* fout);
public:
    char* name;
};

class UnaryOperNode: public Node {
public:
    UnaryOperNode(TokenPtr first_token, int kind, Type* ty, NodePtr operand): 
        Node(kind, ty, first_token), operand(operand) {}  

    virtual long long eval_int(char** global_label = nullptr);
    virtual double eval_float();

    virtual void codegen(Generator& gen);

    virtual char* to_dot_graph(FILE* fout);
public:
    NodePtr operand;
};

class BinaryOperNode: public Node {
public:
    BinaryOperNode(TokenPtr first_token, int kind, Type* ty, NodePtr left, NodePtr right): 
        Node(kind, ty, first_token), left(left), right(right) {}  

    virtual long long eval_int(char** global_label = nullptr);
    virtual double eval_float();

    virtual void codegen(Generator& gen);

    virtual char* to_dot_graph(FILE* fout);
public:
    NodePtr left, right;
};

class TernaryOperNode: public Node {
public:
    TernaryOperNode(TokenPtr first_token, Type* ty, NodePtr cond, NodePtr then, NodePtr els): 
        Node(NK_TERNARY, ty, first_token), 
        cond(cond), then(then), els(els) {}

    virtual long long eval_int(char** global_label = nullptr);
    virtual double eval_float();

    virtual void codegen(Generator& gen);

    virtual char* to_dot_graph(FILE* fout);
public:
    NodePtr cond, then, els;
};

class FuncCallNode: public Node {
public:
    // If the expression that denotes the called function has type pointer to function returning an
    // object type, the function call expression has the same type as that object type
    FuncCallNode(TokenPtr first_token, int kind, char* func_name, FuncType* func_type, 
        NodePtr func_ptr, std::vector<NodePtr> args): 
        Node(kind, func_type->return_type, first_token), func_name(func_name),
        func_type(func_type), func_ptr(func_ptr), args(args) {}

    virtual void codegen(Generator& gen);

    virtual char* to_dot_graph(FILE* fout);
public:
    char* func_name;
    FuncType* func_type;
    NodePtr func_ptr;
    std::vector<NodePtr> args;
};

class StructMemberNode: public Node {
public:
    StructMemberNode(TokenPtr first_token, Type* field_type, NodePtr struc, char* field_name): 
        Node(NK_STRUCT_MEMBER, field_type, first_token), struc(struc), field_name(field_name) {}  

    virtual long long eval_int(char** global_label = nullptr);

    virtual void codegen(Generator& gen);

    virtual char* to_dot_graph(FILE* fout);
public:
    NodePtr struc;
    char* field_name;
};

class LabelAddrNode: public Node {
public:
    LabelAddrNode(TokenPtr first_token, char* label): 
        Node(NK_LABEL_ADDR, make_ptr_type(type_void), first_token), 
        label(label) {}

    virtual void codegen(Generator& gen);

    virtual char* to_dot_graph(FILE* fout);
public:
    char* label;
};

class InitNode: public Node {
public: 
    InitNode(TokenPtr first_token, Type* type, NodePtr value, int offset): 
        Node(NK_INIT, type, first_token), value(value), 
        offset(offset) {}

    virtual void codegen(Generator& gen);

    virtual char* to_dot_graph(FILE* fout);
public:
    NodePtr value;
    int offset;
};

class DeclNode: public Node {
public: 
    DeclNode(TokenPtr first_token, Type* type, NodePtr var): 
        Node(NK_DECL, type, first_token), var(var) {}

    virtual void codegen(Generator& gen);

    virtual char* to_dot_graph(FILE* fout);
public:
    NodePtr var;
    std::vector<NodePtr> init_list;
};

// following are statement node, no need type
class CompoundStmtNode: public Node {
public: 
    CompoundStmtNode(TokenPtr first_token, std::vector<NodePtr> list): 
        Node(NK_COMPOUND_STMT, nullptr, first_token), list(list) {}

    virtual void codegen(Generator& gen);

    virtual char* to_dot_graph(FILE* fout);
public:
    std::vector<NodePtr> list;
};

class IfNode: public Node {
public:
    IfNode(TokenPtr first_token, NodePtr cond, NodePtr then, NodePtr els): 
        Node(NK_IF, nullptr, first_token), 
        cond(cond), then(then), els(els) {}

    virtual void codegen(Generator& gen);

    virtual char* to_dot_graph(FILE* fout);
public:
    NodePtr cond, then, els;
};

class LabelNode: public Node {
public:
    LabelNode(TokenPtr first_token, char* origin_label, char* normal_label): 
        Node(NK_LABEL, nullptr, first_token), 
        origin_label(origin_label), normal_label(normal_label) {}

    virtual void codegen(Generator& gen);

    virtual char* to_dot_graph(FILE* fout);
public:
    char* origin_label;
    char* normal_label;
};

class JumpNode: public Node {
public:
    JumpNode(TokenPtr first_token, char* origin_label, char* normal_label): 
        Node(NK_JUMP, nullptr, first_token), 
        origin_label(origin_label), normal_label(normal_label) {}

    virtual void codegen(Generator& gen);

    virtual char* to_dot_graph(FILE* fout);
public:
    char* origin_label;
    char* normal_label;
};

class ReturnNode: public Node {
public:
    ReturnNode(TokenPtr first_token, NodePtr return_val): 
        Node(NK_RETURN, nullptr, first_token), 
        return_val(return_val) {}

    virtual void codegen(Generator& gen);

    virtual char* to_dot_graph(FILE* fout);
public:
    NodePtr return_val;
};

class FuncDefNode: public Node {
public:
    FuncDefNode(TokenPtr first_token, Type* func_type, char* func_name, std::vector<NodePtr> params, 
        NodePtr body, std::vector<NodePtr> local_vars): 
        Node(NK_FUNC_DEF, func_type, first_token), func_name(func_name),
        params(params), body(body), local_vars(local_vars) {}

    virtual void codegen(Generator& gen);

    virtual char* to_dot_graph(FILE* fout);
public:
    char* func_name;
    std::vector<NodePtr> params;
    NodePtr body;
    std::vector<NodePtr> local_vars;
};

extern NodePtr error_node;

std::shared_ptr<IntNode> make_int_node(TokenPtr first_token, Type* int_type, long long value);
std::shared_ptr<FloatNode> make_float_node(TokenPtr first_token, Type* float_type, double value);
std::shared_ptr<StringNode> make_string_node(TokenPtr first_token, char* str, int len, int encode_method);
std::shared_ptr<LocalVarNode> make_localvar_node(TokenPtr first_token, Type* var_type, char* name, Scope* scope);
std::shared_ptr<GlobalVarNode> make_globalvar_node(TokenPtr first_token, Type* var_type, char* name, Scope* scope);
std::shared_ptr<GlobalVarNode> make_static_localvar_node(TokenPtr first_token, Type* var_type, char* name, Scope* scope);
std::shared_ptr<FuncDesignatorNode> make_func_designator_node(TokenPtr first_token, Type* func_type, char* name);
std::shared_ptr<TypedefNode> make_typedef_node(TokenPtr first_token, Type* type, char* name, Scope* scope);
std::shared_ptr<UnaryOperNode> make_unary_oper_node(TokenPtr first_token, int kind, Type* type, NodePtr operand);
std::shared_ptr<BinaryOperNode> make_binary_oper_node(TokenPtr first_token, int kind, Type* type, NodePtr left, NodePtr right);
std::shared_ptr<TernaryOperNode> make_ternary_oper_node(TokenPtr first_token, Type* type, NodePtr cond, NodePtr then, NodePtr els);
std::shared_ptr<FuncCallNode> make_func_call_node(TokenPtr first_token, NodePtr func_desg, std::vector<NodePtr> args);
std::shared_ptr<FuncCallNode> make_funcptr_call_node(TokenPtr first_token, NodePtr func_ptr, Type* func_type, std::vector<NodePtr> args);
std::shared_ptr<StructMemberNode> make_struct_member_node(TokenPtr first_token, Type* field_type, NodePtr struc, char* field_name);
std::shared_ptr<LabelAddrNode> make_label_addr_node(TokenPtr first_token, char* label);
std::shared_ptr<InitNode> make_init_node(TokenPtr first_token, Type* type, NodePtr value, int offset);
std::shared_ptr<DeclNode> make_decl_node(TokenPtr first_token, Type* type, NodePtr var);
std::shared_ptr<CompoundStmtNode> make_compound_stmt_node(TokenPtr first_token, std::vector<NodePtr> list);
std::shared_ptr<IfNode> make_if_node(TokenPtr first_token, NodePtr cond, NodePtr then, NodePtr els);
std::shared_ptr<LabelNode> make_label_node(TokenPtr first_token, char* origin_label, char* normal_label = nullptr);
std::shared_ptr<JumpNode> make_jump_node(TokenPtr first_token, char* origin_label, char* normal_label = nullptr);
std::shared_ptr<ReturnNode> make_return_node(TokenPtr first_token, NodePtr return_val);
std::shared_ptr<FuncDefNode> make_func_def_node(TokenPtr first_token, Type* func_type, char* func_name, std::vector<NodePtr> params, NodePtr body, Scope* scope);

char* op2s(int op);

void dump_ast(char* filename, std::vector<NodePtr>& ast);
