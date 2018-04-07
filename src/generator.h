#pragma once

#include <unistd.h>
#include <fstream>
#include "ast.h"
#include "parser.h"

using NodePtr = std::shared_ptr<Node>;

class Node;
class DeclNode;
class Parser;

class Generator {
public:
    Generator(char* filename, Parser* parser);

    void emit_noindent(char* fmt);

    template<typename T, typename... Args>   
    void emit_noindent(char* fmt, T t, Args ... args);

    template<typename... Args> 
    void emit(char* fmt, Args ... args);

    void push(char* reg);
    void pop(char* reg);
    void push_xmm(int xmm_id);
    void pop_xmm(int xmm_id);

    int push_struct(int size);

    void emit_bitfield_load(Type* type);
    void emit_bitfield_save(Type* type, char* addr);

    void emit_int_to_int64(Type* type);
    void emit_float_to_int(Type* type);
    void emit_to_bool(Type *type);
    void emit_bool_conv(Type* type);
    void emit_conv(Type* from, Type* to);

    void emit_label(char* label);

    void emit_local_load(Type* type, char* base, int offset);
    void emit_local_save(Type* type, int offset);
    void emit_global_load(Type* type, char* label, int offset);
    void emit_global_save(Type* type, char* label, int offset);

    void emit_literal_save(NodePtr node, Type* totype, int offset);    

    void emit_decl_init(std::vector<NodePtr>& init_list, int offset, int total_size);
    void emit_lvar_init(NodePtr node);

    void emit_addr(NodePtr node);

    void emit_deref_save_aux(Type* type, int offset);
    void emit_deref_save(NodePtr node);

    void emit_struct_member_save(NodePtr struc, Type* field_type, int offset);
    void emit_struct_member_load(NodePtr struc, Type* field_type, int offset);    

    void emit_save(NodePtr node);

    void emit_binop_cmp(NodePtr node);
    void emit_binop_int_arith(NodePtr node);
    void emit_binop_float_arith(NodePtr node);

    void emit_copy_struct(NodePtr from, NodePtr to);

    void emit_data_primtype(Type* type, NodePtr val, int subsection);
    void emit_data_aux(std::vector<NodePtr> init_list, int total_size, int offset, int subsection);
    void emit_data(std::shared_ptr<DeclNode> decl);
    void emit_bss(std::shared_ptr<DeclNode> decl);

    void emit_builtin_va_start(NodePtr node);
    void emit_builtin_reg_class(NodePtr node);

    void emit_reg_area_save();

    void run();

public:
    int stack_size = 0;

private:
    std::ofstream fout;
    Parser* parser;
};