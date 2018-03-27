#include <memory>
#include <assert.h>
#include "token.h"
#include "encode.h"
#include "ast.h"
#include "scope.h"

char* make_tmpname() {
    static int c = 0;
    return format(".T%d", c++);
}

char* make_label() {
    static int c = 0;
    return format(".L%d", c++);
}

char* make_static_label(char *name) {
    static int c = 0;
    return format(".S%d.%s", c++, name);
}

std::shared_ptr<Node> error_node = 
    std::shared_ptr<Node>(new Node(NK_ERROR, nullptr, nullptr));
 
std::shared_ptr<IntNode> make_int_node(TokenPtr first_token, Type* type, long long value) {
    return std::shared_ptr<IntNode>(new IntNode(first_token, type, value));
}

std::shared_ptr<FloatNode> make_float_node(TokenPtr first_token, Type* type, double value) {
    return std::shared_ptr<FloatNode>(new FloatNode(first_token, type, value));
}

std::shared_ptr<StringNode> make_string_node(TokenPtr first_token, char* str, int encode_method) {
    int len = strlen(str);
    Type* type;
    char* value;

    switch(encode_method) {
    // Default encode method is utf8. After lexer, string encoding is utf8
    case ENC_NONE:
    case ENC_UTF8: {
        type = make_array_type(type_char, len);
        value = str;
        break;
    }
    case ENC_CHAR16: {
        Buffer buf = encode_utf16(str, len);
        type = make_array_type(type_ushort, buf.size() / type_ushort->size);
        value = buf.data();
        break;
    }
    case ENC_CHAR32:
    case ENC_WCHAR: {
        Buffer buf = encode_utf32(str, len);
        type = make_array_type(type_uint, buf.size() / type_uint->size);
        value = buf.data();
        break;
    }
    }
    return std::shared_ptr<StringNode>(new StringNode(first_token, type, value));
}

std::shared_ptr<LocalVarNode> make_localvar_node(TokenPtr first_token, Type* type, char* name, Scope* scope) {
    std::shared_ptr<LocalVarNode> node = 
        std::shared_ptr<LocalVarNode>(new LocalVarNode(first_token, type, name));
    if(scope->islocal()) {
        scope->add(name, node); 
    }
    return node;
}

std::shared_ptr<GlobalVarNode> make_globalvar_node(TokenPtr first_token, Type* type, char* name, Scope* scope) {
    std::shared_ptr<GlobalVarNode> node = 
        std::shared_ptr<GlobalVarNode>(new GlobalVarNode(first_token, type, name));
    scope->add_global(name, node);
    return node;
}

std::shared_ptr<GlobalVarNode> make_static_localvar_node(TokenPtr first_token, Type* var_type, char* name, 
    Scope* scope) {
    std::shared_ptr<GlobalVarNode> node = 
        std::shared_ptr<GlobalVarNode>(new GlobalVarNode(first_token, var_type, name));
    node->global_label = make_static_label(name);
    assert(scope->islocal());
    scope->add(name, node);
    return node;
}

std::shared_ptr<FuncDesignatorNode> make_func_designator_node(TokenPtr first_token, Type* type, char* name) {
    return std::shared_ptr<FuncDesignatorNode>(new FuncDesignatorNode(first_token, type, name)) ;
}

std::shared_ptr<TypedefNode> make_typedef_node(TokenPtr first_token, Type* type, char* name, Scope* scope) {
    std::shared_ptr<TypedefNode> node = 
        std::shared_ptr<TypedefNode>(new TypedefNode(first_token, type, name));
    if(scope)
        scope->add(name, node);
    return node;
}

std::shared_ptr<UnaryOperNode> make_unary_oper_node(TokenPtr first_token, int kind, 
    Type* type, std::shared_ptr<Node> operand) {
    return std::shared_ptr<UnaryOperNode>(new UnaryOperNode(first_token, kind, type, operand));
}

std::shared_ptr<BinaryOperNode> make_binary_oper_node(TokenPtr first_token, int kind, Type* type, 
    std::shared_ptr<Node> left, std::shared_ptr<Node> right) {
    return std::shared_ptr<BinaryOperNode>(
        new BinaryOperNode(first_token, kind, type, left, right));
}

std::shared_ptr<FuncCallNode> make_func_call_node(TokenPtr first_token, std::shared_ptr<Node> func_desg, 
    std::vector<std::shared_ptr<Node>> args) {
    assert(func_desg->kind == NK_FUNC_DESG);
    std::shared_ptr<FuncDesignatorNode> node = 
        std::dynamic_pointer_cast<FuncDesignatorNode>(func_desg);
    return std::shared_ptr<FuncCallNode>(new FuncCallNode(first_token, 
        NK_FUNC_CALL, node->func_name, dynamic_cast<FuncType*>(node->type), nullptr, args));
}

std::shared_ptr<FuncCallNode> make_funcptr_call_node(TokenPtr first_token, std::shared_ptr<Node> func_ptr, 
    Type* func_type, std::vector<std::shared_ptr<Node>> args) {
    assert(func_type->kind == TK_FUNC);
    FuncType* ftype = dynamic_cast<FuncType*>(func_type);
    return std::shared_ptr<FuncCallNode>(new FuncCallNode(first_token, 
        NK_FUNCPTR_CALL, nullptr, ftype, func_ptr, args));
}

std::shared_ptr<StructMemberNode> make_struct_member_node(TokenPtr first_token, Type* field_type, StructType* struc, 
    char* field_name) {
    return std::shared_ptr<StructMemberNode>(new StructMemberNode(first_token, field_type, struc, field_name));
}

std::shared_ptr<LabelAddrNode> make_label_addr_node(TokenPtr first_token, char* label) {
    return std::shared_ptr<LabelAddrNode>(new LabelAddrNode(first_token, label));
}

std::shared_ptr<TernaryOperNode> make_ternary_oper_node(TokenPtr first_token, Type* type, std::shared_ptr<Node> cond, 
    std::shared_ptr<Node> then, std::shared_ptr<Node> els) {
    return std::shared_ptr<TernaryOperNode>(new TernaryOperNode(first_token, type, cond, then, els));
}

std::shared_ptr<InitNode> make_init_node(TokenPtr first_token, Type* type, std::shared_ptr<Node> value, int offset) {
    return std::shared_ptr<InitNode>(new InitNode(first_token, type, value, offset));
}

std::shared_ptr<DeclNode> make_decl_node(TokenPtr first_token, Type* type, std::shared_ptr<Node> var) {
    return std::shared_ptr<DeclNode>(new DeclNode(first_token, type, var));
}

std::shared_ptr<CompoundStmtNode> make_compound_stmt_node(TokenPtr first_token, std::vector<NodePtr> list) {
    return std::shared_ptr<CompoundStmtNode>(new CompoundStmtNode(first_token, list));
}

std::shared_ptr<IfNode> make_if_node(TokenPtr first_token, NodePtr cond, NodePtr then, NodePtr els) {
    return std::shared_ptr<IfNode>(new IfNode(first_token, cond, then, els)); 
}

std::shared_ptr<LabelNode> make_label_node(TokenPtr first_token, char* origin_label, char* normal_label) {
    return std::shared_ptr<LabelNode>(new LabelNode(first_token, origin_label, normal_label));     
} 

std::shared_ptr<JumpNode> make_jump_node(TokenPtr first_token, char* origin_label, char* normal_label) {
    return std::shared_ptr<JumpNode>(new JumpNode(first_token, origin_label, normal_label));     
} 

std::shared_ptr<ReturnNode> make_return_node(TokenPtr first_token, NodePtr return_val) {
    return std::shared_ptr<ReturnNode>(new ReturnNode(first_token, return_val));     
}

std::shared_ptr<FuncDefNode> make_func_def_node(TokenPtr first_token, Type* func_type, char* func_name, std::vector<NodePtr> params, NodePtr body, Scope* scope) {
    return std::shared_ptr<FuncDefNode>(new FuncDefNode(first_token, func_type, func_name, params, body, scope->get_local_env()));        
}
 

int Node::eval_int() {
    error("eval_int error: expression must be intergral constant expression");
    return 0;
}

int IntNode::eval_int() {
    return value;
}

int UnaryOperNode::eval_int() {
    switch(kind) {
    case '!': return !operand->eval_int();
    case '~': return ~operand->eval_int();
    case NK_CAST: return operand->eval_int();
    case NK_CONV: return operand->eval_int();
    }
    error("expression must be intergral constant expression");
    return 0;
}

int BinaryOperNode::eval_int() {
    switch(kind) {
    #define L (left->eval_int())
    #define R (right->eval_int())
    case '+': return L + R;
    case '-': return L - R;
    case '*': return L * R;
    case '/': return L / R;
    case '%': return L % R;
    case '&': return L & R;
    case '^': return L ^ R;
    case '|': return L | R;
    case '<': return L < R;
    case P_LE: return L <= R;
    case P_EQ: return L == R;
    case P_NE: return L != R;
    case P_LOGAND: return L && R;
    case P_LOGOR:  return L || R;
    case NK_SAL: return L << R;
    case NK_SAR: return L >> R;
    case NK_SHR: return ((unsigned long)L) >> R;
    }
    error("expression must be intergral constant expression");
    return 0;
}

int TernaryOperNode::eval_int() {
    int cond_val = cond->eval_int();
    if(cond_val) 
        return then ? then->eval_int() : cond_val;
    return els->eval_int();
}

// dot graphviz

static inline char* make_point_id() {
    static int id;
    return format("P%d", id++);
}

static char* ty2s(Type* type) {
    if(type) return type->to_string();
    return "null";
}

static char* op2s(int op) {
    switch(op) {
    case '<': return "lt";
    case '>': return "gt";
    case P_LE: return "le";
    case P_GE: return "ge";
    }
    
    switch(op) {
#define def(id, str) case id: return str;
#include "keywords.h"
#undef def

    case NK_CAST: return "cast"; 
    case NK_CONV: return "convert";
    case NK_DEREF: return "deref";
    case NK_COMPUTED_GOTO: return "computed_goto";

    case NK_PRE_INC: return "pre_inc";
    case NK_PRE_DEC: return "pre_dec";
    case NK_POST_INC: return "post_inc";
    case NK_POST_DEC: return "post_dec";
    case NK_ADDR: return "addr";
    case NK_LABEL_ADDR: return "label_addr";

    case NK_SAL: return "sal";
    case NK_SAR: return "sar";
    case NK_SHR: return "shr";
    default: return format("%c", op);
    }
}

char* Node::to_dot_graph(std::ofstream& fout) {
    assert(kind == NK_ERROR);
    char* id = make_point_id();
    fout << id << "[label=\"{<head>error|<type>null}\"];\n";
    return id;
}

char* IntNode::to_dot_graph(std::ofstream& fout) {
    char* id = make_point_id();
    fout << id << 
        format("[label=\"{<head>int_const|<type>%s|value:%lld}\"];\n", ty2s(type), value);    
    return id;
}

char* FloatNode::to_dot_graph(std::ofstream& fout) {
    char* id = make_point_id();
    fout << id << 
        format("[label=\"{<head>float_const|<type>%s|value:%lf}\"];\n", ty2s(type), value);    
    return id;
}

char* StringNode::to_dot_graph(std::ofstream& fout) {
    char* id = make_point_id();
    fout << id << 
        format("[label=\"{<head>string_const|<type>%s|value:%s}\"];\n", ty2s(type), value);    
    return id;
}

char* LocalVarNode::to_dot_graph(std::ofstream& fout) {
    char* id = make_point_id();
    fout << id << 
        format("[label=\"{<head>local_var|<type>%s|<name>%s|<list>init_list}\"];\n", ty2s(type), var_name);
    for(auto init:init_list) {
        char* child_id = init->to_dot_graph(fout);
        fout << format("%s:list -> %s:head;\n", id, child_id);
    }
    return id;
}

char* GlobalVarNode::to_dot_graph(std::ofstream& fout) {
    char* id = make_point_id();
    fout << id << 
        format("[label=\"{<head>global_var|<type>%s|name:%s}\"];\n", ty2s(type), var_name);    
    return id;
}

char* FuncDesignatorNode::to_dot_graph(std::ofstream& fout) {
    char* id = make_point_id();
    fout << id << 
        format("[label=\"{<head>func_desg|<type>%s|name:%s}\"];\n", ty2s(type), func_name);    
    return id;
}

char* TypedefNode::to_dot_graph(std::ofstream& fout) {
    char* id = make_point_id();
    fout << id << 
        format("[label=\"{<head>typedef|<type>%s|name:%s}\"];\n", ty2s(type), name);    
    return id;
}

char* UnaryOperNode::to_dot_graph(std::ofstream& fout) {
    char* id = make_point_id();
    fout << id << 
        format("[label=\"{<head>%s|<type>%s|<operand>operand}\"];\n", op2s(kind), ty2s(type));    
    if(operand) {
        char* child_id = operand->to_dot_graph(fout);
        fout << format("%s:operand -> %s:head;\n", id, child_id);
    }
    return id;
}

char* BinaryOperNode::to_dot_graph(std::ofstream& fout) {
    char* id = make_point_id();
    fout << id << 
        format("[label=\"{<head>%s|<type>%s|<left_operand>left|<right_operand>right}\"];\n", op2s(kind), ty2s(type));    
    if(left) {
        char* child_id = left->to_dot_graph(fout);
        fout << format("%s:left_operand -> %s:head;\n", id, child_id);
    }
    if(right) {
        char* child_id = right->to_dot_graph(fout);
        fout << format("%s:right_operand -> %s:head;\n", id, child_id);
    }
    return id;
}

char* TernaryOperNode::to_dot_graph(std::ofstream& fout) {
    char* id = make_point_id();
    fout << id << 
        format("[label=\"{<head>ternery|<type>%s|<cond>cond|<then>then|<els>els}\"];\n", ty2s(type));    
    if(cond) {
        char* child_id = cond->to_dot_graph(fout);
        fout << format("%s:cond -> %s:head;\n", id, child_id);
    }
    if(then) {
        char* child_id = then->to_dot_graph(fout);
        fout << format("%s:then -> %s:head;\n", id, child_id);
    }
    if(els) {
        char* child_id = els->to_dot_graph(fout);
        fout << format("%s:els -> %s:head;\n", id, child_id);
    }
    return id;
}

char* FuncCallNode::to_dot_graph(std::ofstream& fout) {
    char* id = make_point_id();
    fout << id << 
        format("[label=\"{<head>func_call|<type>%s|name:%s|<args>args}\"];\n", ty2s(type), func_name);    
    for(auto arg:args) {
        char* child_id = arg->to_dot_graph(fout);
        fout << format("%s:args -> %s:head;\n", id, child_id);
    }
    return id;
}

char* StructMemberNode::to_dot_graph(std::ofstream& fout) {
    char* id = make_point_id();
    fout << id << 
        format("[label=\"{<head>struct_member|<type>%s|field:%s|struct_type:%s}\"];\n", ty2s(type), field_name, ty2s(struc));    
    return id;
}

char* LabelAddrNode::to_dot_graph(std::ofstream& fout) {
    char* id = make_point_id();
    fout << id << 
        format("[label=\"{<head>label_addr|<type>%s|label:%s}\"];\n", ty2s(type), label);    
    return id;
}

char* InitNode::to_dot_graph(std::ofstream& fout) {
    char* id = make_point_id();
    fout << id << 
        format("[label=\"{<head>init|<type>%s|offset:%d|<value>value}\"];\n", ty2s(type), offset);    
    if(value) {
        char* child_id = value->to_dot_graph(fout);
        fout << format("%s:value -> %s:head;\n", id, child_id);
    }
    return id;
}

char* DeclNode::to_dot_graph(std::ofstream& fout) {
    char* id = make_point_id();
    fout << id << 
        format("[label=\"{<head>decl|<type>%s|<var>var|<init_list>init_list}\"];\n", ty2s(type));    
    if(var) {
        char* child_id = var->to_dot_graph(fout);
        fout << format("%s:var -> %s:head;\n", id, child_id);
    }
    for(auto init:init_list) {
        char* child_id = init->to_dot_graph(fout);
        fout << format("%s:init_list -> %s:head;\n", id, child_id);
    }
    return id;
}

char* CompoundStmtNode::to_dot_graph(std::ofstream& fout) {
    char* id = make_point_id();
    fout << id << 
        format("[label=\"{<head>compound_stmt|null|<list>list}\"];\n");    
    for(auto item:list) {
        char* child_id = item->to_dot_graph(fout);
        fout << format("%s:list -> %s:head;\n", id, child_id);
    }
    return id;
}

char* IfNode::to_dot_graph(std::ofstream& fout) {
    char* id = make_point_id();
    fout << id << 
        format("[label=\"{<head>if|null|<cond>cond|<then>then|<els>els}\"];\n");    
    if(cond) {
        char* child_id = cond->to_dot_graph(fout);
        fout << format("%s:cond -> %s:head;\n", id, child_id);
    }
    if(then) {
        char* child_id = then->to_dot_graph(fout);
        fout << format("%s:then -> %s:head;\n", id, child_id);
    }
    if(els) {
        char* child_id = els->to_dot_graph(fout);
        fout << format("%s:els -> %s:head;\n", id, child_id);
    }
    return id;
}

char* LabelNode::to_dot_graph(std::ofstream& fout) {
    char* id = make_point_id();
    fout << id << 
        format("[label=\"{<head>label|null|label:%s(%s)}\"];\n", origin_label, normal_label);
    return id;    
}

char* JumpNode::to_dot_graph(std::ofstream& fout) {
    char* id = make_point_id();
    fout << id << 
        format("[label=\"{<head>jump|null|label:%s(%s)}\"];\n", origin_label, normal_label);
    return id;  
}

char* ReturnNode::to_dot_graph(std::ofstream& fout) {
    char* id = make_point_id();
    fout << id << 
        format("[label=\"{<head>return|null|<value>value}\"];\n");    
    if(return_val) {
        char* child_id = return_val->to_dot_graph(fout);
        fout << format("%s:value -> %s:head;\n", id, child_id);
    }
    return id;
}

char* FuncDefNode::to_dot_graph(std::ofstream& fout) {
    char* id = make_point_id();
    fout << id << 
        format("[label=\"{<head>func_def:%s|<type>%s|<params>params|<body>body}\"];\n", func_name, ty2s(type));
    for(auto param:params) {
        char* child_id = param->to_dot_graph(fout);
        fout << format("%s:params -> %s:head;\n", id, child_id);
    }
    if(body) {
        char* child_id = body->to_dot_graph(fout);
        fout << format("%s:body -> %s:head;\n", id, child_id);
    }
}

