#pragma once

#include <vector>
#include <map>
#include <algorithm>
#include <memory>
#include "token.h"
#include "preprocessor.h"
#include "type.h"
#include "ast.h"
#include "scope.h"

using NodePtr = std::shared_ptr<Node>;
using TokenPtr = std::shared_ptr<Token>;

class Parser {
public:
    Parser(Preprocessor* p): pp(p), scope(new Scope()) {}

    std::vector<NodePtr>& get_ast();

public:
    // aux function
    Type* get_typedef(char* name);
    bool is_type_name(TokenPtr tok);
    bool is_assignable(Type* type1, Type* type2);

    Type* usual_arith_convert(Type* type1, Type* type2);
    NodePtr convert(NodePtr node, Type* type = nullptr);

    NodePtr make_binop(TokenPtr tok, int op, NodePtr left, NodePtr right);

    //---------------------------- Expressions ----------------------------

    // primary-expression
    NodePtr read_prim_expr();
    NodePtr read_ident(TokenPtr t);
    NodePtr read_constant(TokenPtr t);
    NodePtr read_string(TokenPtr t);
    NodePtr read_generic();
    std::vector<std::pair<Type*, NodePtr>> read_generic_assoc_list();

    // postfix-expression
    NodePtr read_post_expr(bool maybe_return_type = false);
    NodePtr read_post_expr_tail(NodePtr node);
    NodePtr read_func_call(NodePtr func);
    std::vector<NodePtr> read_func_call_args(Type* func_type);
    NodePtr read_struct_member(NodePtr node);

    // unary-expression
    NodePtr read_unary_expr();
    Type* read_sizeof_operand();

    // cast-expression
    NodePtr read_cast_expr();

    // multiplicative-expression
    NodePtr read_mul_expr();
    NodePtr read_mul_expr_tail(NodePtr node);

    // additive-expression
    NodePtr read_add_expr();
    NodePtr read_add_expr_tail(NodePtr node);

    // shift-expression
    NodePtr read_shift_expr();
    NodePtr read_shift_expr_tail(NodePtr node);

    // relational-expression
    NodePtr read_relational_expr();
    NodePtr read_relational_expr_tail(NodePtr node);

    // equality-expression
    NodePtr read_equal_expr();
    NodePtr read_equal_expr_tail(NodePtr node);

    // and-expression
    NodePtr read_and_expr();
    NodePtr read_and_expr_tail(NodePtr node);

    // exclusive-or-expression
    NodePtr read_xor_expr();
    NodePtr read_xor_expr_tail(NodePtr node);

    // inclusive-or-expression
    NodePtr read_or_expr();
    NodePtr read_or_expr_tail(NodePtr node);

    // logical-and-expression
    NodePtr read_land_expr();
    NodePtr read_land_expr_tail(NodePtr node);

    // logical-or-expression
    NodePtr read_lor_expr();
    NodePtr read_lor_expr_tail(NodePtr node);

    // conditional-expression
    NodePtr read_cond_expr();
    NodePtr read_cond_expr_tail(NodePtr cond);

    // assignment-expression
    NodePtr read_assign_expr();
    NodePtr read_assign_expr_tail(NodePtr node);

    // expression
    NodePtr read_expr();

    // constant-expression
    NodePtr read_const_expr();

    //---------------------------- Declarations ----------------------------

    // declaration
    void read_decl(std::vector<NodePtr>& list, bool isglobal); 

    // static_assert-declaration
    void read_static_assert(TokenPtr tok);

    // declaration-specifiers
    Type* read_decl_spec();
    Type* read_decl_spec_opt();

    // struct-or-union-specifier
    Type* read_struct_or_union_spec(int kind);
    void read_struct_decl_list(std::vector<std::pair<char*, Type*>>& fields);

    // enum-specifier
    Type* read_enum_spec();

    // alignment-specifier
    int read_alignas();

    // typeof
    Type* read_typeof();

    // declarator
    Type* read_declarator(char** name, Type* basetype, 
        std::vector<NodePtr>* params, int declarator_kind);
    Type* read_declarator_tail(Type* basetype, std::vector<NodePtr>* params);
    
    // parameter-list
    Type* read_param_list(Type* return_type, std::vector<NodePtr>* params);
    void read_declarator_params(std::vector<NodePtr>* params, 
        std::vector<Type*>& param_types, bool& has_var_params);
    void read_declarator_params_oldstyle(std::vector<NodePtr>* params,
        std::vector<Type*>& param_types);

    // array-size
    Type* read_array_size(Type* basetype);

    // type-name
    Type* read_type_name();

    // initializer
    void read_initializer(Type* type, std::vector<NodePtr>& init_list);

    // initializer-list
    void read_initializer_list(std::vector<NodePtr>& init_list, Type* type, int offset = 0);
    static void assign_string(std::vector<NodePtr>& init_list, Type* type, TokenPtr tok, int offset); 
    void read_struct_initializer_list(std::vector<NodePtr>& init_list, Type* type, int offset);
    void read_array_initializer_list(std::vector<NodePtr>& init_list, Type* type, int offset);
    void read_designator_list_tail(std::vector<NodePtr>& init_list, Type* type, int offset);

    //---------------------------- Statements ----------------------------

    // statement
    NodePtr read_stmt();

    // labeled-statement
    NodePtr read_label_stmt(TokenPtr tok);
    NodePtr read_case_stmt(TokenPtr tok);
    NodePtr read_default_stmt(TokenPtr tok);
    NodePtr read_label_stmt_tail(NodePtr node);

    // compound-statement
    NodePtr read_compound_stmt(TokenPtr tok);
    void read_block_item(std::vector<NodePtr>& list);

    // expression-statement
    NodePtr read_expr_stmt();

    // boolean-expr
    NodePtr read_boolean_expr();

    // if-statement
    NodePtr read_if_stmt(TokenPtr tok);

    // switch-statement
    NodePtr read_switch_stmt(TokenPtr tok);
    NodePtr make_switch_jump(NodePtr var, const CaseTuple& c);

    // while-statement
    NodePtr read_while_stmt(TokenPtr tok);

    // do-statement
    NodePtr read_do_stmt(TokenPtr tok);

    // for-statement
    NodePtr read_for_stmt(TokenPtr tok);
    NodePtr read_for_init();

    // goto-statement
    NodePtr read_goto_stmt(TokenPtr tok);

    // continue-statement
    NodePtr read_continue_stmt(TokenPtr tok);

    // break-statement
    NodePtr read_break_stmt(TokenPtr tok);
    
    // return-statement
    NodePtr read_return_stmt(TokenPtr tok);  

    //----------------------- External Definitions ------------------------- 

    // external-declaration
    void read_extern_decl();

    // function-definition
    NodePtr read_func_def();
    bool is_func_def();
    void read_oldstyle_param_type(FuncType* func_type, std::vector<NodePtr>& params);
    NodePtr read_func_body(TokenPtr tok, FuncType* func_type, char* fname, std::vector<NodePtr> params);

private:
    Preprocessor* pp;
    Scope* scope;

    std::vector<NodePtr> toplevers;

    std::map<char*, NodePtr, cstr_cmp> labels;
    std::vector<NodePtr> gotos;
    std::map<char*, Type*, cstr_cmp> tags;
};