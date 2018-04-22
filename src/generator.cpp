#include <stdlib.h>
#include <fstream>
#include <assert.h>
#include <algorithm>
#include "ast.h"
#include "error.h"
#include "generator.h"
using namespace std;

// (6 ∗ 8 + 8 ∗ 16) = 176
#define REG_SAVE_AREA_SIZE 176

// according to linux x86_64-abi: 
// User-level applications use as integer registers for passing the sequence
// %rdi, %rsi, %rdx, %rcx, %r8 and %r9.
// Floating point arguments are placed in the registers %xmm0-%xmm7
static char* REGS[6] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"}; 
// static char* XMMS[8] = {"xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7"};

#ifdef DEBUG_MODE

#define errorp_old errorp

#define errorp(...) { errorp_old(__VA_ARGS__); error("stop mcc"); }

#endif

Generator::Generator(char* filename, Parser* parser): parser(parser), fout(filename) {
    fout.setf(ios::fixed);
}

void Generator::emit_noindent(char* fmt) {
    fout << fmt << endl;
}

template<typename T, typename... Args>   
void Generator::emit_noindent(char* fmt, T t, Args ... args) {
    char* p = fmt;
    while(p) {
        if(*p == '?') {
            fout << t;
            ++p;
            if(!p) return;
            emit_noindent(p, args...);
            return;
        }
        fout << *p;
        ++p;
    }
    return;
}

template<typename... Args> 
void Generator::emit(char* fmt, Args ... args) {
    fout << '\t';
    emit_noindent(fmt, args...);
}

void Generator::push(char* reg) {
    emit("push %?", reg);
    stack_size += 8;
}

void Generator::pop(char* reg) {
    emit("pop %?", reg);
    stack_size -= 8;
    assert(stack_size >= 0);
}

void Generator::push_xmm(int xmm_id) {
    emit("sub $8, %rsp");
    emit("movsd %xmm?, (%rsp)", xmm_id);
    stack_size += 8;
}

void Generator::pop_xmm(int xmm_id) {
    emit("movsd (%rsp), %xmm?", xmm_id);
    emit("add $8, %rsp");
    stack_size -= 8;
    assert(stack_size >= 0);
}

static inline int align8(int size) {
    int m = size % 8;
    return (m == 0) ? size : size - m + 8;
}

// Structure address is stored in rax
int Generator::push_struct(int size) {
    int aligned_size = align8(size);
    emit("sub ?, %rsp", aligned_size);
    emit("movq %rcx, -8(%rsp)");
    int i = 0;
    for(; i < aligned_size; i += 8) {
        emit("movq ?(%rax), %rcx", i);
        emit("movq %rcx, ?(%rsp)", i);
    }
    emit("movq -8(%rsp), %rcx");
    stack_size += aligned_size;
    return aligned_size;
}

static const char* get_reg(Type* type, char r) {
    assert(r == 'a' || r == 'c');
    switch (type->size) {
    case 1: return (r == 'a') ? "al" : "cl";
    case 2: return (r == 'a') ? "ax" : "cx";
    case 4: return (r == 'a') ? "eax" : "ecx";
    case 8: return (r == 'a') ? "rax" : "rcx";
    default:
        error("invalid data size: %s: %d", type->to_string(), type->size);
    }
}

void Generator::emit_bitfield_load(Type* type) {
    push("rcx");
    emit("shr $?, %rax", type->bitoff);
    emit("mov $?, %rcx", (uint64_t)(1 << type->bitsize) - 1);
    emit("and %rcx, %rax");
    pop("rcx");
}

// The result has not been saved to memory
void Generator::emit_bitfield_save(Type* type, char* addr) {
    push("rcx");
    push("rdi");
    emit("mov $?, %rdi", (uint64_t)(1 << type->bitsize) - 1);
    emit("and %rdi, %rax");
    emit("shl $?, %rax", type->bitoff);
    emit("mov ?, %?", addr, get_reg(type, 'c'));
    emit("mov $?, %rdi", ~(((uint64_t)(1 << type->bitsize) - 1) << type->bitoff));
    emit("and %rdi, %rcx");
    emit("or %rcx, %rax");
    pop("rdi");
    pop("rcx");
}

void Generator::emit_int_to_int64(Type* type) {
    switch(type->kind) {
    case TK_BOOL:
    case TK_CHAR:
        type->is_unsigned ? emit("movzbq %al, %rax") 
                          : emit("movsbq %al, %rax");
        return;
    case TK_SHORT:
        type->is_unsigned ? emit("movzwq %ax, %rax") 
                          : emit("movswq %ax, %rax");
        return;
    case TK_INT:
        type->is_unsigned ? emit("movl %eax, %eax") 
                          : emit("movslq %eax, %rax");
        return;
    case TK_LONG:
    case TK_LONG_LONG:
        return;
    }
}

void Generator::emit_float_to_int(Type* type) {
    switch(type->kind) {
    case TK_FLOAT:
        emit("cvttss2si %xmm0, %eax"); return;
    case TK_DOUBLE:
        emit("cvttsd2si %xmm0, %eax"); return;
    case TK_LONG_DOUBLE:
        emit("cvttsd2si %xmm0, %eax"); return;
    default:
        return;
    }
}

void Generator::emit_to_bool(Type *type) {
    if(type->is_float_type()) {
        push_xmm(1);
        emit("xorpd %xmm1, %xmm1");
        emit("%s %xmm1, %xmm0", (type->kind == TK_FLOAT) ? "ucomiss" : "ucomisd");
        emit("setne %al");
        pop_xmm(1);
    }
    else {
        emit("cmp $0, %rax");
        emit("setne %al");
    }
    emit("movzb %al, %eax");
}


void Generator::emit_bool_conv(Type* type) {
    if(type->kind == TK_BOOL) {
        emit("test %rax, %rax");
        emit("setne %al");
    }
}

void Generator::emit_conv(Type* from, Type* to) {
    if(from->is_int_type() && to->kind == TK_FLOAT)
        emit("cvtsi2ss %eax, %xmm0");
    else if(from->is_int_type() && (to->kind == TK_DOUBLE || to->kind == TK_LONG_DOUBLE))
        emit("cvtsi2sd %eax, %xmm0");
    else if(from->kind == TK_FLOAT && (to->kind == TK_DOUBLE || to->kind == TK_LONG_DOUBLE))
        emit("cvtps2pd %xmm0, %xmm0");
    else if((from->kind == TK_DOUBLE || from->kind == TK_LONG_DOUBLE) && to->kind == TK_FLOAT)
        emit("cvtpd2ps %xmm0, %xmm0");
    else if(to->kind == TK_BOOL) 
        emit_to_bool(from);
    else if(from->is_int_type() && to->is_int_type()) 
        emit_int_to_int64(from);
    else if(to->is_int_type())
        emit_float_to_int(from);
}


void Generator::emit_label(char* label) {
    emit("?:", label);
}

const char* Generator::get_mov_inst(Type *type) {
    switch (type->size) {
    case 1: 
        return type->is_unsigned ? "movzbq" : "movsbq";
    case 2: 
        return type->is_unsigned ? "movzwq" : "movswq";
    case 4: 
        return type->is_unsigned ? nullptr : "movslq";
    case 8: 
        return "movq";
    default:
        errorp(current_pos, "invalid mov data size: %s: %d", type->to_string(), type->size);
    }
}

void Generator::emit_local_load(Type* type, char* base, int offset) {
    switch(type->kind) {
    case TK_FLOAT:
        emit("movss ?(%?), %xmm0", offset, base); break;
    case TK_DOUBLE:
    case TK_LONG_DOUBLE:
        emit("movsd ?(%?), %xmm0", offset, base); break;
    case TK_ARRAY:
    case TK_STRUCT:
        emit("lea ?(%?), %rax", offset, base); break;
    default: {
        const char* inst = get_mov_inst(type);
        if(inst == nullptr) 
            emit("movl ?(%?), %eax", offset, base);
        else 
            emit("? ?(%?), %rax", inst, offset, base);
        if(type->bitsize > 0) 
            emit_bitfield_load(type);
    }
    }
}

void Generator::emit_local_save(Type* type, int offset) {
    switch(type->kind) {
    case TK_FLOAT:
        emit("movss %xmm0, ?(%rbp)", offset); break;
    case TK_DOUBLE:
    case TK_LONG_DOUBLE:
        emit("movsd %xmm0, ?(%rbp)", offset); break;
    default: {
        emit_bool_conv(type);
        const char* reg = get_reg(type, 'a');
        char* addr = format("%d(%%rbp)", offset);
        if(type->bitsize > 0) 
            emit_bitfield_save(type, addr);
        emit("mov %?, ?", reg, addr);
    }
    }
}

void Generator::emit_global_load(Type* type, char* label, int offset) {
    switch(type->kind) {
    case TK_ARRAY: 
    case TK_STRUCT:
        emit("lea ?+?(%rip), %rax", label, offset); break;
    case TK_FLOAT:
        emit("movss ?+?(%rip), %xmm0", label, offset); break;
    case TK_DOUBLE:
    case TK_LONG_DOUBLE:
        emit("movsd ?+?(%rip), %xmm0", label, offset); break;
    default: {
        const char* inst = get_mov_inst(type);
        if(inst == nullptr) 
            emit("movl ?+?(%rip), %rax", label, offset);
        else 
            emit("? ?+?(%rip), %rax", inst, label, offset);
        if(type->bitsize > 0) 
            emit_bitfield_load(type);
    }
    }
}

void Generator::emit_global_save(Type* type, char* label, int offset) {
    emit_bool_conv(type);
    const char* reg = get_reg(type, 'a');
    char* addr = format("%s+%d(%%rip)", label, offset);
    if(type->bitsize > 0) 
        emit_bitfield_save(type, addr);
    emit("mov %?, ?", reg, addr);
}

// Save literal values directly to memory
void Generator::emit_literal_save(NodePtr node, Type* totype, int offset) {
    switch(totype->kind) {
    case TK_BOOL:  
        emit("movb $?, ?(%rbp)", !!node->eval_int(), offset); break;
    case TK_CHAR:  
        emit("movb $?, ?(%rbp)", node->eval_int(), offset); break;
    case TK_SHORT: 
        emit("movw $?, ?(%rbp)", node->eval_int(), offset); break;
    case TK_INT:   
        emit("movl $?, ?(%rbp)", node->eval_int(), offset); break;
    case TK_LONG:
    case TK_LONG_LONG:
    case TK_PTR: {
        emit("movq $?, ?(%rbp)", (uint64_t)(node->eval_int()), offset);
        break;
    }
    case TK_FLOAT: {
        float d = node->eval_float();
        emit("movl $?, ?(%rbp)", *(uint32_t *)(&d), offset);
        break;
    }
    case TK_DOUBLE:
    case TK_LONG_DOUBLE: {
        double d = node->eval_float();
        emit("movq $?, ?(%rbp)", *(uint64_t *)(&d), offset);
        break;
    }
    default:
        error("invalid iteral type(%s) for emit_literal_save()", totype->to_string());
    }
}

void Generator::emit_decl_init(vector<NodePtr>& init_list, int offset, int total_size) {
    int last_end = 0;
    auto emit_zero = [&](int start, int end) {
        for(; start <= end - 8; start += 8)
            emit("movq $0, ?(%rbp)", start);
        for(; start <= end - 4; start += 4)
            emit("movl $0, ?(%rbp)", start);
        for(; start < end; ++start)
            emit("movb $0, ?(%rbp)", start);
    };
    for(auto item:init_list) {
        shared_ptr<InitNode> init = dynamic_pointer_cast<InitNode>(item);
        // file zero 
        if(init->offset > last_end) {
            emit_zero(offset + last_end, offset + init->offset);
        }
        last_end = init->offset + init->type->size;

        bool isbitfield = (init->type->bitsize > 0);
        if(init->value->kind == NK_LITERAL && !isbitfield) {
            emit_literal_save(init->value, init->type, offset + init->offset);
        }
        else {
            init->value->codegen(*this);
            emit_local_save(init->type, offset + init->offset);
        }
    }
    emit_zero(offset + last_end, offset + total_size);
}

void Generator::emit_lvar_init(NodePtr node) {
    shared_ptr<LocalVarNode> lvar = dynamic_pointer_cast<LocalVarNode>(node);
    if(!lvar->init_list.empty()) {
        emit_decl_init(lvar->init_list, lvar->offset, lvar->type->size);
        lvar->init_list.clear();
    }
}

void Generator::emit_addr(NodePtr node) {
    switch(node->kind) {
    case NK_LOCAL_VAR: {
        shared_ptr<LocalVarNode> lvar = dynamic_pointer_cast<LocalVarNode>(node);
        emit_lvar_init(lvar);
        emit("lea ?(%rbp), %rax", lvar->offset);
        return;
    }
    case NK_GLOBAL_VAR: {
        emit("lea ?(%rip), %rax", dynamic_pointer_cast<GlobalVarNode>(node)->global_label);
        return;
    }
    case NK_DEREF: {
        node->codegen(*this);
        return;
    }
    case NK_STRUCT_MEMBER: {
        shared_ptr<StructMemberNode> s = dynamic_pointer_cast<StructMemberNode>(node); 
        emit_addr(s->struc);
        emit("add $?, %rax", s->type->offset);
        return;
    }
    case NK_FUNC_DESG: {
        emit("lea ?(%rip), %rax", dynamic_pointer_cast<FuncDesignatorNode>(node)->func_name);
        return;
    }
    default:
        error("invalid & usage");
    }
}

// The address of the target is stored in rax, 
// and the value to be assigned is stored on the stack.
void Generator::emit_deref_save_aux(Type* type, int offset) {
    if(type->is_float_type()) {
        emit("movsd (%rsp), %xmm0");
        if(type->kind == TK_FLOAT) {
            emit("movss %xmm0, ?(%rax)", offset);
        }
        else {
            emit("movsd %xmm0, ?(%rax)", offset);
        }
        pop_xmm(0);
    }
    else {
        emit("movq (%rsp), %rcx");
        const char* reg = get_reg(type, 'c');
        emit("mov %?, ?(%rax)", reg, offset);
        pop("rax");
    }
}

void Generator::emit_deref_save(NodePtr node) {
    shared_ptr<UnaryOperNode> deref = dynamic_pointer_cast<UnaryOperNode>(node);
    Type* type = dynamic_cast<PtrType*>(deref->operand->type)->ptr_type;
    if(type->is_float_type()) {
        push_xmm(0);
    }
    else {
        push("rax");
    }
    deref->operand->codegen(*this);
    emit_deref_save_aux(type, 0);
}

void Generator::emit_struct_member_save(NodePtr struc, Type* field_type, int offset) {
    switch(struc->kind) {
    case NK_LOCAL_VAR: {
        shared_ptr<LocalVarNode> lvar = dynamic_pointer_cast<LocalVarNode>(struc);        
        emit_lvar_init(lvar);
        emit_local_save(field_type, offset + lvar->offset + field_type->offset);
        return;
    }
    case NK_GLOBAL_VAR: {
        shared_ptr<GlobalVarNode> gvar = dynamic_pointer_cast<GlobalVarNode>(struc);        
        emit_global_save(field_type, gvar->global_label, offset + field_type->offset);
        return;
    }
    case NK_DEREF: {
        if(field_type->is_float_type()) {
            push_xmm(0);
        }
        else {
            push("rax");
        }
        shared_ptr<UnaryOperNode> deref = dynamic_pointer_cast<UnaryOperNode>(struc);
        deref->operand->codegen(*this);
        emit_deref_save_aux(field_type, offset + field_type->offset);
        return;
    }
    case NK_STRUCT_MEMBER: {
        NodePtr struc_struc = dynamic_pointer_cast<StructMemberNode>(struc)->struc;
        emit_struct_member_save(struc_struc, field_type, offset + struc->type->offset);
        return;
    }
    default: 
        error("internal error: emit_struct_member_save()");
    }
}

void Generator::emit_struct_member_load(NodePtr struc, Type* field_type, int offset) {
    switch(struc->kind) {
    case NK_LOCAL_VAR: {
        shared_ptr<LocalVarNode> lvar = dynamic_pointer_cast<LocalVarNode>(struc);        
        emit_lvar_init(lvar);
        emit_local_load(field_type, "rbp", offset + lvar->offset + field_type->offset);
        return;
    }
    case NK_GLOBAL_VAR: {
        shared_ptr<GlobalVarNode> gvar = dynamic_pointer_cast<GlobalVarNode>(struc);        
        emit_global_load(field_type, gvar->global_label, offset + field_type->offset);
        return;
    }
    case NK_DEREF: {
        shared_ptr<UnaryOperNode> deref = dynamic_pointer_cast<UnaryOperNode>(struc);
        deref->operand->codegen(*this);
        emit_local_load(field_type, "rax", offset + field_type->offset);
        return;
    }
    case NK_STRUCT_MEMBER: {
        NodePtr struc_struc = dynamic_pointer_cast<StructMemberNode>(struc)->struc;
        emit_struct_member_load(struc_struc, field_type, offset + struc->type->offset);
        return;
    }
    default: 
        error("internal error: emit_struct_member_load()");
    }
}


void Generator::emit_save(NodePtr node) {
    switch(node->kind) {
    case NK_LOCAL_VAR: {
        shared_ptr<LocalVarNode> var = dynamic_pointer_cast<LocalVarNode>(node); 
        emit_lvar_init(var);
        emit_local_save(var->type, var->offset);
        return;
    }
    case NK_GLOBAL_VAR: {
        shared_ptr<GlobalVarNode> var = dynamic_pointer_cast<GlobalVarNode>(node); 
        emit_global_save(var->type, var->global_label, 0); 
        return;
    }
    case NK_DEREF: {
        emit_deref_save(node);
        return;
    }
    case NK_STRUCT_MEMBER: {
        NodePtr struc = dynamic_pointer_cast<StructMemberNode>(node)->struc;
        emit_struct_member_save(struc, node->type, 0);
        return;
    }
    default:
        error("invalid store operation");
    }
}

void Generator::emit_binop_cmp(NodePtr node) {
    shared_ptr<BinaryOperNode> expr = dynamic_pointer_cast<BinaryOperNode>(node); 
    if(expr->left->type->is_float_type()) {
        expr->left->codegen(*this);
        push_xmm(0);
        expr->right->codegen(*this);
        pop_xmm(1);
        if(expr->left->type->kind == TK_FLOAT) 
            emit("ucomiss %xmm0, %xmm1");
        else
            emit("ucomisd %xmm0, %xmm1");
    }
    else {
        expr->left->codegen(*this);
        push("rax");
        expr->right->codegen(*this);
        pop("rcx");
        int kind = expr->left->type->kind;
        if(kind == TK_LONG || kind == TK_LONG_LONG) 
            emit("cmp %rax, %rcx");
        else
            emit("cmp %eax, %ecx");
    }

    const char* inst;
    bool use_unsigned_set_inst = expr->left->type->is_float_type() || expr->left->type->is_unsigned;
    switch(node->kind) {
    case '<': use_unsigned_set_inst ? inst = "setb" : inst = "setl"; break;
    case P_LE: use_unsigned_set_inst ? inst = "setbe" : inst = "setle"; break;
    case P_EQ: inst = "sete"; break;
    case P_NE: inst = "setne"; break;
    }
    emit("? %al", inst);
    emit("movzb %al, %eax");
}

// result is stored in the rax
void Generator::emit_binop_int_arith(NodePtr node) {
    const char* inst;
    switch(node->kind) {
    case '+': inst = "add"; break;
    case '-': inst = "sub"; break;
    case '*': inst = "imul"; break;
    case '/': case '%': break;
    case '^': inst = "xor"; break;
    case NK_SAL: inst = "sal"; break;
    case NK_SAR: inst = "sar"; break;
    case NK_SHR: inst = "shr"; break;
    default: error("invalid binary integer arithmetic operator %s", op2s(node->kind));    
    }
    shared_ptr<BinaryOperNode> expr = dynamic_pointer_cast<BinaryOperNode>(node); 
    expr->left->codegen(*this);
    push("rax");
    expr->right->codegen(*this);
    emit("movq %rax, %rcx");
    pop("rax");
    if(expr->kind == '/' || expr->kind == '%') {
        if(expr->type->is_unsigned) {
            emit("movl $0, %edx");
            emit("divq %rcx");
        }
        else {
            emit("cqto");
            emit("idivq %rcx");
        }
        if(expr->kind == '%') {
            emit("movq %rdx, %rax");
        }
    }
    else if(expr->kind == NK_SAL || expr->kind == NK_SAR || expr->kind == NK_SHR) {
        emit("? %cl, %?", inst, get_reg(expr->left->type, 'a'));
    }
    else {
        emit("? %rcx, %rax", inst);
    }
}

// result is stored in the xmm0
void Generator::emit_binop_float_arith(NodePtr node) {
    const char* inst;
    bool is_double = (node->type->kind == TK_DOUBLE || node->type->kind == TK_LONG_DOUBLE);
    switch(node->kind) {
    case '+': inst = (is_double ? "addsd" : "addss"); break;
    case '-': inst = (is_double ? "subsd" : "subss"); break;
    case '*': inst = (is_double ? "mulsd" : "mulss"); break;
    case '/': inst = (is_double ? "divsd" : "divss"); break;
    default: error("invalid binary float arithmetic operator %s", op2s(node->kind));    
    }
    shared_ptr<BinaryOperNode> expr = dynamic_pointer_cast<BinaryOperNode>(node); 
    expr->left->codegen(*this);
    push_xmm(0);
    expr->right->codegen(*this);
    emit("? %xmm0, %xmm1", (is_double ? "movsd" : "movss"));    
    pop_xmm(0);
    emit("? %xmm1, %xmm0", inst);
}

void Generator::emit_copy_struct(NodePtr from, NodePtr to) {
    int aligned_size = align8(from->type->size);
    push("rcx");
    push("r11");
    emit_addr(from);
    emit("movq %rax, %rcx");
    emit_addr(to);
    int i = 0;
    for(; i < aligned_size; i += 8) {
        emit("movq ?(%rcx), %r11", i);
        emit("movq %r11, ?(%rax)", i);
    }
    pop("r11");
    pop("rcx");
}

void Generator::emit_data_primtype(Type* type, NodePtr val, int subsection) {
    switch(type->kind) {
    case TK_BOOL:
        emit(".byte ?", !!val->eval_int());
        return;
    case TK_CHAR:
        emit(".byte ?", short(uint8_t(val->eval_int())));
        return;
    case TK_SHORT: 
        emit(".short ?", short(val->eval_int()));
        return;
    case TK_INT:
        emit(".long ?", int(val->eval_int()));
        return;
    case TK_LONG:
    case TK_LONG_LONG: {
        char* global_label = nullptr;
        long long v = val->eval_int(&global_label);
        if(!global_label) {
            emit(".quad ?", v);
        }
        else {
            emit(".quad ?+?", global_label, v);
        }
        return;
    }
    case TK_FLOAT: {
        double v = val->eval_float();
        emit(".long %d", *(uint32_t *)&v);
        return;
    }
    case TK_DOUBLE:
    case TK_LONG_DOUBLE: {
        double v = val->eval_float();
        emit(".quad %d", *(uint64_t *)&v);
        return;
    }
    case TK_PTR: {
        while(val->kind == NK_CAST || val->kind == NK_CONV) {
            val = dynamic_pointer_cast<UnaryOperNode>(val)->operand;
        }
        Type* subtype = dynamic_cast<PtrType*>(type)->ptr_type;
        // string
        if(val->type->is_string_type() && val->kind == NK_LITERAL) {
            assert(subtype->kind == TK_CHAR);
            char *label = make_label();
            emit(".data ?", subsection + 1);
            emit_label(label);
            char* value = dynamic_pointer_cast<StringNode>(val)->value;
            assert(val->type->kind == TK_ARRAY);
            emit(".string \"?\"", quote_string(value, dynamic_cast<ArrayType*>(val->type)->length));
            emit(".data ?", subsection);
            emit(".quad ?", label);
            return;
        }
        else if(val->kind == NK_ADDR) {
            NodePtr operand = dynamic_pointer_cast<UnaryOperNode>(val)->operand;            
            switch(operand->kind) {
            case NK_LOCAL_VAR: {
                shared_ptr<LocalVarNode> lvar = dynamic_pointer_cast<LocalVarNode>(operand);
                char *label = make_label();
                emit(".data ?", subsection + 1);
                emit_label(label);
                emit_data_aux(lvar->init_list, lvar->type->size, 0, subsection + 1);
                emit(".data ?", subsection);
                emit(".quad ?", label);
                return;
            }
            case NK_GLOBAL_VAR: {
                emit(".quad ?", dynamic_pointer_cast<GlobalVarNode>(operand)->global_label);
                return;
            }
            case NK_FUNC_DESG: {
                emit(".quad ?", dynamic_pointer_cast<FuncDesignatorNode>(operand)->func_name);                
                return;
            }
            }
        }

        char* global_label = nullptr;
        long long v = val->eval_int(&global_label);
        if(!global_label) {
            emit(".quad ?", v);
        }
        else {
            emit(".quad ?+?", global_label, v);
        }
        return;
    }
    default:
        errort(val->first_token, "expected constant expression");
    }
}


void Generator::emit_data_aux(std::vector<NodePtr> init_list, int total_size, int offset, int subsection) {
    auto emit_zero = [&](int start, int end) {
        // for(; start <= end - 8; start += 8)
        //     emit(".quad 0");
        // for(; start <= end - 4; start += 4)
        //     emit(".long 0");
        // for(; start < end; ++start)
        //     emit(".byte 0");
        if(end - start > 0)
            emit(".zero ?", end - start);
    };
    int last_end = 0;
    for(int i = 0; i < init_list.size(); ++i) {
        shared_ptr<InitNode> init = dynamic_pointer_cast<InitNode>(init_list[i]);
        // file zero 
        if(init->offset > last_end) {
            emit_zero(offset + last_end, offset + init->offset);
        }

        if(init->type->bitsize > 0) {
            assert(init->type->bitoff == 0);
            long long val = init->value->eval_int();
            Type* totype = init->type;
            for(++i; i < init_list.size(); ++i) {
                init = dynamic_pointer_cast<InitNode>(init_list[i]);
                if(init->type->bitsize <= 0) {
                    --i;
                    break;
                }
                totype = init->type;
                val |= ((((long long)1 << totype->bitsize) - 1) & init->value->eval_int()) << totype->bitoff;
            }
            emit_data_primtype(totype, make_int_node(nullptr, totype, val), subsection);
            last_end = init->offset + init->type->size;
            if(i == init_list.size()) break;
            continue;
        }
        else {
            last_end = init->offset + init->type->size;
        }

        if(init->value->kind == NK_LOCAL_VAR) {
            shared_ptr<LocalVarNode> lvar = dynamic_pointer_cast<LocalVarNode>(init->value);
            assert(!lvar->var_name);
            emit_data_aux(lvar->init_list, lvar->type->size, 0, subsection);
        }
        else if(init->value->kind == NK_ADDR) {
            assert(init->type->kind == TK_PTR || init->type->kind == TK_LONG || init->type->kind == TK_LONG_LONG);
            NodePtr operand = dynamic_pointer_cast<UnaryOperNode>(init->value)->operand;            
            switch(operand->kind) {
            case NK_LOCAL_VAR: {
                shared_ptr<LocalVarNode> lvar = dynamic_pointer_cast<LocalVarNode>(operand);
                char *label = make_label();
                emit(".data ?", subsection + 1);
                emit_label(label);
                emit_data_aux(lvar->init_list, lvar->type->size, 0, subsection + 1);
                emit(".data ?", subsection);
                emit(".quad ?", label);
                break;
            }
            case NK_GLOBAL_VAR: {
                emit(".quad ?", dynamic_pointer_cast<GlobalVarNode>(operand)->global_label);
                break;
            }
            case NK_FUNC_DESG: {
                emit(".quad ?", dynamic_pointer_cast<FuncDesignatorNode>(operand)->func_name);                
                break;
            }
            default: {
                char* global_label = nullptr;
                long long v = operand->eval_int(&global_label);
                if(!global_label) {
                    emit(".quad ?", v);
                }
                else {
                    emit(".quad ?+?", global_label, v);
                }
            }
            }
        }
        else {
            emit_data_primtype(init->type, init->value, subsection);
        }
    }
    emit_zero(last_end, total_size);
}

void Generator::emit_data(std::shared_ptr<DeclNode> decl) {
    emit(".data 0");
    assert(decl->var->kind == NK_GLOBAL_VAR);
    shared_ptr<GlobalVarNode> gvar = dynamic_pointer_cast<GlobalVarNode>(decl->var);
    if(!gvar->type->is_static()) {
        emit(".globl ?", gvar->global_label);
    }
    emit_noindent("?:", gvar->global_label);
    emit_data_aux(decl->init_list, gvar->type->size, 0, 0);
}

void Generator::emit_bss(std::shared_ptr<DeclNode> decl) {
    emit(".data");
    assert(decl->var->kind == NK_GLOBAL_VAR);
    shared_ptr<GlobalVarNode> gvar = dynamic_pointer_cast<GlobalVarNode>(decl->var);
    if(!gvar->type->is_static()) {
        emit(".globl ?", gvar->global_label);
    }
    emit(".lcomm ?, ?", gvar->global_label, gvar->type->size);
}

void Generator::emit_builtin_va_start(NodePtr node) {
    assert(node->kind == NK_FUNC_CALL);
    shared_ptr<FuncCallNode> fcall = dynamic_pointer_cast<FuncCallNode>(node);
    NodePtr arg = fcall->args[0];
    FuncType* ftype = dynamic_cast<FuncType*>(fcall->func_type);

    // arg is pointer to va_list;
    /* va_list:
        typedef struct {
            unsigned int gp_offset;
            unsigned int fp_offset;
            void *overflow_arg_area;
            void *reg_save_area;
        } va_list[1];
    */
    arg->codegen(*this);
    push("rcx");
    emit("movl $?, (%rax)", ftype->numgp * 8);
    emit("movl $?, 4(%rax)", 48 + ftype->numfp * 16);
    emit("lea ?(%rbp), %rcx", -REG_SAVE_AREA_SIZE);
    emit("mov %rcx, 16(%rax)");
    pop("rcx");
}

// 0 is GPR, 1 is SSE, 2 is MEMORY
void Generator::emit_builtin_reg_class(NodePtr node) {
    assert(node->kind == NK_FUNC_CALL);
    NodePtr arg = dynamic_pointer_cast<FuncCallNode>(node)->args[0];
    if(arg->kind == NK_CONV) {
        arg = dynamic_pointer_cast<UnaryOperNode>(arg)->operand;
    }
    assert(arg->type->kind == TK_PTR);
    Type* type = dynamic_cast<PtrType*>(arg->type)->ptr_type;
    if(type->kind == TK_STRUCT) {
        emit("movl $2, %eax");
    }
    else if(type->is_float_type()) {
        emit("movl $1, %eax");
    }
    else {
        emit("movl $0, %eax");
    }
}

void Generator::emit_reg_area_save() {
    emit("sub $?, %rsp", REG_SAVE_AREA_SIZE);
    for(int i = 0; i < 6; ++i) {
        emit("movq %?, ?(%rsp)", REGS[i], 8 * i);
    }
    for(int i = 0; i < 8; ++i) {
        emit("movaps %xmm?, ?(%rsp)", i, 48 + 16 * i);
    }
}


// -------------------------------- codegen --------------------------------

#define SAVE_CURRENT_POS gen.current_pos = shared_from_this()->first_token->get_pos()

void Node::codegen(Generator& gen) {
    error("internal error: Node cannot generate code");
}

void IntNode::codegen(Generator& gen) {
    SAVE_CURRENT_POS;
    gen.emit("movq $?, %rax", (uint64_t)value);
}

void FloatNode::codegen(Generator& gen) {
    SAVE_CURRENT_POS;
    if(!label) {
        label = make_label();
        gen.emit_noindent(".data");
        gen.emit_label(label);
        if(type->kind == TK_FLOAT) {
            gen.emit(".long ?", *(uint32_t*)&value);
        }
        else {
            gen.emit(".quad ?", *(uint64_t*)&value);
        }
        gen.emit_noindent(".text");
    }
    if(type->kind == TK_FLOAT) {
        gen.emit("movss ?(%rip), %xmm0", label);
    }
    else {
        gen.emit("movsd ?(%rip), %xmm0", label);
    }
}

void StringNode::codegen(Generator& gen) {
    SAVE_CURRENT_POS;
    if(!label) {
        label = make_label();
        gen.emit_noindent(".data");
        gen.emit_label(label);
        if(strlen(value) == 0) {
            gen.emit(".string \"\"");
        }
        else {
            assert(type->kind == TK_ARRAY);
            gen.emit(".string \"?\"", quote_string(value, dynamic_cast<ArrayType*>(type)->length));
        }
        gen.emit_noindent(".text");
    }
    gen.emit("lea ?(%rip), %rax", label);
}

void LocalVarNode::codegen(Generator& gen) {
    SAVE_CURRENT_POS;
    gen.emit_lvar_init(shared_from_this());
    gen.emit_local_load(type, "rbp", offset);
}

void GlobalVarNode::codegen(Generator& gen) {
    SAVE_CURRENT_POS;
    gen.emit_global_load(type, global_label, 0);
}

void FuncDesignatorNode::codegen(Generator& gen) {
    error("internal error: FuncDesignatorNode cannot generate code");
}

void TypedefNode::codegen(Generator& gen) {
    error("internal error: TypedefNode cannot generate code");
}

void UnaryOperNode::codegen(Generator& gen) {
    SAVE_CURRENT_POS;
    switch(kind) {
    case NK_DEREF: {
        operand->codegen(gen);
        Type* ptr_type = dynamic_cast<PtrType*>(operand->type)->ptr_type;
        gen.emit_local_load(ptr_type, "rax", 0);
        gen.emit_conv(ptr_type, type);
        return;
    }
    case NK_CONV: {
        operand->codegen(gen);
        gen.emit_conv(operand->type, type);
        return;
    }
    case NK_CAST: {
        operand->codegen(gen);
        gen.emit_conv(operand->type, type);
        return;
    }
    case NK_ADDR: {
        gen.emit_addr(operand);
        return;
    }
    case NK_POST_INC:
    case NK_POST_DEC: {
        operand->codegen(gen);
        gen.push("rax");
        int size = operand->type->kind == TK_PTR ? dynamic_cast<PtrType*>(operand->type)->ptr_type->size
                : (operand->type->kind == TK_ARRAY ? dynamic_cast<ArrayType*>(operand->type)->elem_type->size : 1);
        gen.emit("? $?, %rax", kind == NK_POST_INC ? "add": "sub", size);
        gen.emit_save(operand);
        gen.pop("rax");
        return;
    }
    case NK_PRE_INC:
    case NK_PRE_DEC: {
        operand->codegen(gen);
        int size = operand->type->kind == TK_PTR ? dynamic_cast<PtrType*>(operand->type)->ptr_type->size
                : (operand->type->kind == TK_ARRAY ? dynamic_cast<ArrayType*>(operand->type)->elem_type->size : 1);
        gen.emit("? $?, %rax", kind == NK_PRE_INC ? "add": "sub", size);
        gen.emit_save(operand);
        return;
    }
    case '~': {
        operand->codegen(gen);
        gen.emit("not %rax");
        return;
    }
    case '!': {
        operand->codegen(gen);
        gen.emit("cmp $0, %rax");
        gen.emit("sete %al");
        gen.emit("movzb %al, %eax");
        return;
    }
    case NK_COMPUTED_GOTO: {
        operand->codegen(gen);
        gen.emit("jmp *%rax");
        return;
    }
    default:
        error("invalid unary operator");
    }
}

void BinaryOperNode::codegen(Generator& gen) {
    SAVE_CURRENT_POS;
    switch(kind) {
    case '<':
    case P_LE:
    case P_EQ:
    case P_NE: {
        gen.emit_binop_cmp(shared_from_this());
        return;
    }
    case '&': case '|': {
        left->codegen(gen);
        gen.push("rax");
        right->codegen(gen);
        gen.pop("rcx");
        gen.emit("? %rcx, %rax", kind == '&' ? "and" : "or");
        return;
    }
    case P_LOGAND: {
        char* end = make_label();
        left->codegen(gen);
        gen.emit("test %rax, %rax");
        gen.emit("movq $0, %rax");
        gen.emit("je ?", end);
        right->codegen(gen);
        gen.emit("test %rax, %rax");
        gen.emit("movq $0, %rax");
        gen.emit("je ?", end);
        gen.emit("movq $1, %rax");
        gen.emit_label(end);
        return;
    }
    case P_LOGOR: {
        char* end = make_label();
        left->codegen(gen);
        gen.emit("test %rax, %rax");
        gen.emit("movq $1, %rax");
        gen.emit("jne ?", end);
        right->codegen(gen);
        gen.emit("test %rax, %rax");
        gen.emit("movq $1, %rax");
        gen.emit("jne ?", end);
        gen.emit("movq $0, %rax");
        gen.emit_label(end);
        return;
    }
    case '=': {
        if(left->type->kind == TK_STRUCT && left->type->size > 8) {
            gen.emit_copy_struct(right, left);
        }
        else {
            right->codegen(gen);
            gen.emit_conv(right->type, type);
            gen.emit_save(left);
        }
        return;
    }
    case ',': {
        left->codegen(gen);
        right->codegen(gen);
        return;
    }
    default: {
        if(type->kind == TK_PTR) {
            assert(left->type->kind == TK_PTR);
            left->codegen(gen);
            gen.push("rcx");
            gen.push("rax");
            right->codegen(gen);
            int size = dynamic_cast<PtrType*>(left->type)->ptr_type->size;
            if(size > 1)
                gen.emit("imul $?, %rax", size);
            gen.emit("movq %rax, %rcx");
            gen.pop("rax");
            switch(kind) {
            case '+': gen.emit("add %rcx, %rax"); break;
            case '-': gen.emit("sub %rcx, %rax"); break;
            default: error("invalid pointer operator %s", op2s(kind));
            }
            gen.pop("rcx");
        }
        else if(type->is_int_type()) {
            gen.emit_binop_int_arith(shared_from_this());
        }
        else if(type->is_float_type()) {
            gen.emit_binop_float_arith(shared_from_this());
        }
        else {
            error("invalid binary operator %s", op2s(kind));
        }
    }
    }
}

void TernaryOperNode::codegen(Generator& gen) {
    SAVE_CURRENT_POS;
    cond->codegen(gen);
    char* not_equal = make_label();
    gen.emit("test %rax, %rax");
    gen.emit("je ?", not_equal);
    if(then) {
        then->codegen(gen);
    }
    if(els) {
        char* end = make_label();
        gen.emit("jmp ?", end);
        gen.emit_label(not_equal);
        els->codegen(gen);
        gen.emit_label(end);
    }
    else {
        gen.emit_label(not_equal);
    }
}

/*
The calling convention used by X86-64 on Linux is somewhat different and is known as the System V ABI. 

1.Integer arguments (including pointers) are placed in the registers %rdi, %rsi, %rdx, %rcx, %r8, and %r9, in that order. 
2.Floating point arguments are placed in the registers %xmm0-%xmm7, in that order.
3.Arguments in excess of the available registers are pushed onto the stack.
4.If the function takes a variable number of arguments (like printf) then the %eax register must be set to the number of floating point arguments.
5.The called function may use any registers, but it must restore the values of the registers %rbx, %rbp, %rsp, and %r12-%r15, if it changes them.
6.The return value of the function is placed in %eax.
*/
void FuncCallNode::codegen(Generator& gen) {
    SAVE_CURRENT_POS;
    // If it is a built-in function, execute it directly
    if(func_name != nullptr && !strcmp(func_name, "__builtin_va_start")) {
        gen.emit_builtin_va_start(shared_from_this());
        return;
    }
    if(func_name != nullptr && !strcmp(func_name, "__builtin_reg_class")) {
        gen.emit_builtin_reg_class(shared_from_this());
        return;
    }

    int stack_size_dup = gen.stack_size;
    bool is_ptr_func_call = (func_ptr != nullptr);

    // classify arguments
    vector<NodePtr> int_args, float_args, other_args;
    int gprs = 0, xmms = 0;
    int gpr_max = 6, xmm_max = 8;
    for(auto arg:args) {
        if(arg->type->kind == TK_STRUCT) {
            other_args.push_back(arg);
        }
        else if(arg->type->is_float_type()) {
            (xmms++ < xmm_max) ? float_args.push_back(arg) : other_args.push_back(arg);
        }
        else {
            (gprs++ < gpr_max) ? int_args.push_back(arg) : other_args.push_back(arg);            
        }
    }

    // save register
    for(int i = 0; i < int_args.size(); ++i) {
        gen.push(REGS[i]);
    }
    for(int i = 1; i < float_args.size(); ++i) {
        gen.push_xmm(i);
    }

    bool padding = gen.stack_size % 16;
    if(padding) {
        gen.emit("sub $8, %rsp");
        gen.stack_size += 8;
    }
    
    // emit args
    int others_size = 0;
    for(int i = other_args.size() - 1; i >= 0; --i) {
        Type* type = other_args[i]->type;
        if(type->kind == TK_STRUCT) {
            gen.emit_addr(other_args[i]);
            others_size += gen.push_struct(type->size);
        }
        else if(type->is_float_type()) {
            other_args[i]->codegen(gen);
            gen.push_xmm(0);
            others_size += 8;
        }
        else {
            other_args[i]->codegen(gen);
            gen.push("rax");
            others_size += 8;
        }
    }

    // The following commented code is wrong; rdx, rcx may be changed.
    // for(int i = 0; i < float_args.size(); ++i) {
    //     float_args[i]->codegen(gen);
    //     if(i == 0) {
    //         gen.push_xmm(0);
    //     }
    //     else {
    //         gen.emit("movaps %xmm0, %xmm?", i);
    //     }
    // }
    // if(float_args.size() > 0) gen.pop_xmm(0);

    // for(int i = 0; i < int_args.size(); ++i) {
    //     int_args[i]->codegen(gen);
    //     gen.emit("movq %rax, %?", REGS[i]);
    // }

    for(int i = 0; i < float_args.size(); ++i) {
        float_args[i]->codegen(gen);
        gen.push_xmm(0);
    }
    for(int i = 0; i < int_args.size(); ++i) {
        int_args[i]->codegen(gen);
        gen.push("rax");
    }

    for(int i = int_args.size() - 1; i >= 0; --i) {
        gen.pop(REGS[i]);
    }
    for(int i = float_args.size() - 1; i >= 0; --i) {
        gen.pop_xmm(i);
    }

    // func call
    if(is_ptr_func_call) {
        func_ptr->codegen(gen);
        gen.emit("movq %rax, %r11");
    }

    if(func_type->has_var_param) {
        gen.emit("mov $?, %eax", (unsigned int)(float_args.size()));
    }

    if(is_ptr_func_call) {
        gen.emit("call *%r11");
    }
    else {
        gen.emit("call ?", func_name);
    }

    if(type->kind == TK_BOOL) {
        gen.emit("movzx %al, %rax");
    }

    // restore registers
    if(others_size > 0) {
        gen.emit("add $?, %rsp", others_size);
        gen.stack_size -= others_size;
    }
    if(padding) {
        gen.emit("add $8, %rsp");
        gen.stack_size -= 8;
    }
    for(int i = float_args.size() - 1; i > 0; --i) {
        gen.pop_xmm(i);
    }
    for(int i = int_args.size() - 1; i >= 0; --i) {
        gen.pop(REGS[i]);
    }

    assert(stack_size_dup == gen.stack_size);
}

void StructMemberNode::codegen(Generator& gen) {
    SAVE_CURRENT_POS;
    gen.emit_struct_member_load(struc, type, 0);
}

void LabelAddrNode::codegen(Generator& gen) {
    SAVE_CURRENT_POS;
    assert(label != nullptr);
    gen.emit("movq $?, %rax", label);
}

void InitNode::codegen(Generator& gen) {
    SAVE_CURRENT_POS;
    error("internal error: InitNode cannot generate code");
}

void DeclNode::codegen(Generator& gen) {
    SAVE_CURRENT_POS;
    if(init_list.empty()) return;
    assert(var->kind == NK_LOCAL_VAR);
    shared_ptr<LocalVarNode> lvar = dynamic_pointer_cast<LocalVarNode>(var);
    gen.emit_decl_init(init_list, lvar->offset, lvar->type->size);
}

void CompoundStmtNode::codegen(Generator& gen) {
    SAVE_CURRENT_POS;
    for(auto stmt:list) {
        stmt->codegen(gen);
    }
}

void IfNode::codegen(Generator& gen) {
    SAVE_CURRENT_POS;
    cond->codegen(gen);
    char* not_equal = make_label();
    gen.emit("test %rax, %rax");
    gen.emit("je ?", not_equal);
    if(then) {
        then->codegen(gen);
    }
    if(els) {
        char* end = make_label();
        gen.emit("jmp ?", end);
        gen.emit_label(not_equal);
        els->codegen(gen);
        gen.emit_label(end);
    }
    else {
        gen.emit_label(not_equal);
    }
}

void LabelNode::codegen(Generator& gen) {
    SAVE_CURRENT_POS;
    assert(normal_label != nullptr);
    gen.emit_label(normal_label);   
}

void JumpNode::codegen(Generator& gen) {
    SAVE_CURRENT_POS;
    assert(normal_label != nullptr);
    gen.emit("jmp ?", normal_label);
}

void ReturnNode::codegen(Generator& gen) {
    SAVE_CURRENT_POS;
    if(return_val) {
        return_val->codegen(gen);
        if(return_val->type->kind == TK_BOOL) {
            gen.emit("movzx %al, %rax");
        }
    }
    gen.emit("leave");
    gen.emit("ret");
}

static char* REGS_LOW[6] = {"dil", "sil", "dl", "cl", "r8b", "r9b"}; 

void FuncDefNode::codegen(Generator& gen) {
    SAVE_CURRENT_POS;
    gen.emit(".text");
    if(!type->is_static()) 
        gen.emit_noindent(".globl ?", func_name);
    gen.emit_noindent("?:", func_name);
    gen.emit("nop");
    gen.push("rbp");
    gen.emit("movq %rsp, %rbp");
    int offset = 0;

    // If the function has variable parameters
    FuncType* ftype = dynamic_cast<FuncType*>(type);
    if(ftype->has_var_param) {
        gen.emit_reg_area_save();
        offset -= REG_SAVE_AREA_SIZE;
    }

    // push function parameters
    int gprs = 0, xmms = 0, pos = 2; // pos = 2, because old rip and old rbp is stored in stack
    for(auto param:params) {
        if(param->type->kind == TK_STRUCT) {
            gen.emit("lea ?(%rbp), %rax", pos * 8);
            int size = gen.push_struct(param->type->size);
            offset -= size;
            pos += size / 8;
        }
        else if(param->type->is_float_type()) {
            if(xmms >= 8) {
                gen.emit("movq ?(%rbp), %rax", pos++ * 8);
                gen.push("rax");
            }
            else {
                gen.push_xmm(xmms++);
            }
            offset -= 8;
        }
        else {
            if(gprs >= 6) {
                if(param->type->kind == TK_BOOL) {
                    gen.emit("movb ?(%rbp), %al", pos++ * 8);
                    gen.emit("movzx %al, %rax");
                }
                else {
                    gen.emit("movq ?(%rbp), %rax", pos++ * 8);
                }
                gen.push("rax");
            }
            else {
                if(param->type->kind == TK_BOOL) {
                    gen.emit("movzx %?, %?", REGS_LOW[gprs], REGS[gprs]);
                }
                gen.push(REGS[gprs++]);
            }
            offset -= 8;
        }
        assert(param->kind == NK_LOCAL_VAR);
        dynamic_pointer_cast<LocalVarNode>(param)->offset = offset;
    }
    ftype->numgp = gprs;
    ftype->numfp = xmms;

    // emit local varient
    int localarea = 0;
    for(auto var:local_vars) {
        int size = align8(var->type->size);
        offset -= size;
        assert(var->kind == NK_LOCAL_VAR);
        dynamic_pointer_cast<LocalVarNode>(var)->offset = offset;
        localarea += size;
    }
    if(localarea) {
        gen.emit("sub $?, %rsp", localarea);
        gen.stack_size += localarea;
    }

    if(body != nullptr)
        body->codegen(gen);

    gen.emit("leave");
    gen.emit("ret");
}

void Generator::run() {
    vector<NodePtr> ast = parser->get_ast();
    if(ast.size() == 0) return;
    current_pos = ast[0]->first_token->get_pos();
    for(auto node:ast) {
        stack_size = 8;
        if(node->kind == NK_FUNC_DEF) {
            node->codegen(*this);
        }
        else if(node->kind == NK_DECL) {
            shared_ptr<DeclNode> decl = dynamic_pointer_cast<DeclNode>(node);
            if(decl->init_list.empty()) {
                emit_bss(decl);
            }
            else {
                emit_data(decl);
            }
        }
        else {
            error("invalid toplevel statement");
        }
    }
}