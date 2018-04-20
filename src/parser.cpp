#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <stdarg.h>
#include "parser.h"
#include "error.h"
#include "ast.h"
using namespace std;

#define UINT_MAX 0xFFFFFFFF
#define INT_MAX 0x7FFFFFFF
#define ULONG_MAX 0xFFFFFFFFFFFFFFFF
#define LONG_MAX 0x7FFFFFFFFFFFFFFF

enum DeclaratorKind {
    DK_CONCRETE, 
    DK_ABSTRACT, // type-only
    DK_OPTIONAL,
}; 

#define parser_error(...) errort(pp->peek_token(), __VA_ARGS__)

#ifdef DEBUG_MODE

#define errort_old errort

#define errort(...) { errort_old(__VA_ARGS__); error("stop mcc"); }

#endif

Parser::Parser(Preprocessor* p): pp(p), scope(new Scope()) {

}

// --------------------------- some auxiliar function ------------------------------

static inline NodePtr make_deref_node(TokenPtr tok, NodePtr p) {
    return make_unary_oper_node(tok, NK_DEREF, 
        dynamic_cast<PtrType*>(p->type)->ptr_type, p);
}

static Type* get_struct_field_type(std::vector<std::pair<char*, Type*>>& fields, 
    char* name, int* index) {
    for(size_t i = 0; i < fields.size(); ++i) {
        if(!strcmp(fields[i].first, name)) {
            if(index) *index = i;
            return fields[i].second;
        }
    }
    return nullptr;
}

bool is_lvalue(NodePtr node) {
    switch (node->kind) {
    case NK_LOCAL_VAR: case NK_GLOBAL_VAR: case NK_DEREF: case NK_STRUCT_MEMBER:
        return true;
    }
    return false;
}

static inline bool is_power2(int n) {
    return (n <= 0) ? false : !(n & (n - 1));
}

// C11 6.3.5p22 An array type of unknown size is an incomplete type.
static Type* copy_incomplete_type(Type* type) {
    if(!type) return nullptr;   
    if(!type->from_copy && type->kind == TK_ARRAY) {
        return (dynamic_cast<ArrayType*>(type)->length == -1) ? 
            type->copy() : type;
    }
    return type;
}

// --------------------------- about struct alignment ------------------------------
// Externsion: Arrays of Length Zero
static void check_struct_flexible_array(TokenPtr tok, vector<pair<char*, Type*>>& fields) {
    for(size_t i = 0; i < fields.size(); ++i) {
        // char* name = fields[i].first;
        Type* type = fields[i].second;
        if(type->kind != TK_ARRAY)
            continue;
        ArrayType* arrtype = dynamic_cast<ArrayType*>(type);
        if(arrtype->length == -1) {
            if(i != fields.size() - 1) {
                errort(tok, "flexible array member not at end of struct");
                return;
            }
            if(fields.size() <= 1) {
                errort(tok, "flexible array member with no other fields");
                return;
            }
            arrtype->length = 0;
            arrtype->size = 0;
        }
    }
}

// C11 6.7.2.1p13: The members of an anonymous structure or union are considered
// to be members of the containing structure or union.
static void untie_unnamed_struct(vector<pair<char*, Type*>>& fields, 
    Type* unnamed_struct, int offset) {
    
    assert(unnamed_struct->kind == TK_STRUCT);
    auto fs = dynamic_cast<StructType*>(unnamed_struct)->fields;
    for(auto& f:fs) {
        Type* type = f.second;
        type->offset += offset;
        fields.push_back(f);
    }
}

static int compute_padding(int offset, int align) {
    return (offset % align == 0) ? 0 : align - offset % align;
}

static void finish_bitfield(int& offset, int& bitoff) {
    offset += (bitoff + 7) / 8;
    bitoff = 0;
}

static vector<pair<char*, Type*>> update_struct_offset(int& size, 
    int& align, vector<pair<char*, Type*>>& fields) {
    int offset = 0, bitoff = 0;
    vector<pair<char*, Type*>> new_fields;
    for(auto& field : fields) {
        char* name = field.first;
        Type* type = field.second;

        if(name) align = max(align, type->align);

        if(!name && (type->kind == TK_STRUCT || type->kind == TK_UNION)) {
            align = max(align, type->align);
            finish_bitfield(offset, bitoff);
            offset += compute_padding(offset, align);
            untie_unnamed_struct(new_fields, type, offset);
            offset += type->size;
            continue;
        }

        // C11 6.7.2.1p12: As a special case, a bit-field structure member with a
        // width of 0 indicates that no further bit-field is to be packed into the
        // unit in which the previous bit-field, if any, was placed.
        if(type->bitsize == 0) {
            finish_bitfield(offset, bitoff);
            offset += compute_padding(offset, align);
            continue;
        }

        if(!name) continue;

        if(type->bitsize > 0) {
            int bits = type->size * 8;
            int room = bits - bitoff;
            if(type->bitsize <= room) {
                type->offset = offset;
                type->bitoff = bitoff;
            }
            else {
                finish_bitfield(offset, bitoff);
                offset += compute_padding(offset, align);
                type->offset = offset;
                type->bitoff = 0;
            }
            bitoff += type->bitsize;
        }
        else {
            finish_bitfield(offset, bitoff);
            offset += compute_padding(offset, align);
            type->offset = offset;
            offset += type->size;
        }
        new_fields.push_back(field);
    }
    finish_bitfield(offset, bitoff);
    size = offset + compute_padding(offset, align);
    return new_fields;
}

static vector<pair<char*, Type*>> update_union_offset(int& size, 
    int& align, vector<pair<char*, Type*>>& fields) {
    int maxsize = 0;
    vector<pair<char*, Type*>> new_fields;
    for(auto& field : fields) {
        char* name = field.first;
        Type* type = field.second;

        if(!name && (type->kind == TK_STRUCT || type->kind == TK_UNION)) {
            untie_unnamed_struct(new_fields, type, 0);
            continue;
        }
        else if(!name) continue;

        align = max(align, type->align);
        maxsize = max(maxsize, type->size);

        type->offset = 0;
        if(type->bitsize >= 0) {
            type->bitoff = 0;
        }
        new_fields.push_back(field);
    }
    size = maxsize + compute_padding(maxsize, align);
    return new_fields;
}

// ----------------------------------------------------------------------------------

void fill_in_null_type(Type*& type, Type* fill_type) {
    Type* p = type;
    while(true) {
        if(p->kind == TK_ARRAY) {
            ArrayType* arrtype = dynamic_cast<ArrayType*>(p);
            if(arrtype->elem_type->kind == TK_NULL) {
                if(fill_type->kind == TK_FUNC) {
                    error("declaration of type name as array of functions");
                    return;
                }
                arrtype->elem_type = fill_type;
                return;
            }
            p = arrtype->elem_type;
        }
        else if(p->kind == TK_PTR) {
            PtrType* ptrtype = dynamic_cast<PtrType*>(p);
            if(ptrtype->ptr_type->kind == TK_NULL) {
                ptrtype->ptr_type = fill_type;
                return;
            }
            p = ptrtype->ptr_type;
        }
        else if(p->kind == TK_NULL) {
            type = fill_type;
            return;
        }
        else {
            error("internal error: fill_in_null_type();");
        }
    }
}

// get typedef type
Type* Parser::get_typedef(char* name) {
    NodePtr var = scope->get(name);
    if(var && var->kind == NK_TYPEDEF)
        return var->type;
    return nullptr;
}

/*
accoding to first table: type-name begin with:
'_Bool'  '_Complex'  '_Imaginary'  'atomic'  'char'  
'const'  'double' 'enum' 'float'  'int'  'long'  
'restrict'  'short'  'signed'  'struct'  'union'
'typedef_name'  'unsigned'  'void'  'volatile'
*/
bool Parser::is_type_name(TokenPtr tok) {
    if(tok->kind == TIDENT)  
        return (get_typedef(tok->to_string()) != nullptr);
    return (tok->kind >= KW_VOID && tok->kind <= KW_ALIGNAS);
}

bool Parser::is_assignable(Type* type1, Type* type2) {
    if((type1->is_arith_type() || type1->kind == TK_PTR) &&
        (type2->is_arith_type() || type2->kind == TK_PTR))
        return true;
    if(type1->is_compatible(type2)) return true;
    parser_error("incompatible kind: '%s' and '%s'", 
        type1->to_string(), type2->to_string());
    return false;
}


// C11 6.3 Conversions

// C11 6.3.1.8: Usual arithmetic conversions
Type* Parser::usual_arith_convert(Type* type1, Type* type2) {
    assert(type1->is_arith_type() && type2->is_arith_type());
    // make sure that type1 has greater conversion rank 
    if(type1->kind < type2->kind) 
        swap(type1, type2);
    if(type1->is_float_type()) return type1;
    assert(type1->is_int_type() && type2->is_int_type());
    if(type1->size > type2->size) return type1;
    assert(type1->size == type2->size);
    if(type1->is_unsigned) return type1;
    if(type2->is_unsigned) return type2;
    return type1;
}

NodePtr Parser::convert(NodePtr node, Type* ty) {
    if(!node) 
        return node;
    if(ty) {
        if(node->type->is_compatible(ty))
            return node;
        return make_unary_oper_node(node->first_token, NK_CONV, ty, node);
    }
    Type* type = node->type;
    switch(type->kind) {
    // C11 6.3.1.1p2: If an int can represent all values of the original type (as 
    // restricted by the width, for a bit-field), the value is converted to an int
    case TK_BOOL: case TK_CHAR: case TK_SHORT:
        return make_unary_oper_node(node->first_token, NK_CONV, type_int, node);
    case TK_INT: // A bit-field of type int or signed int is converted to an int
        if(type->bitsize > 0)
            return make_unary_oper_node(node->first_token, NK_CONV, type_int, node);
        break;
    // C11 6.3.2.1p3: type ‘‘array of type’’ is converted to an expression with type ‘‘pointer to type’’
    case TK_ARRAY:
        return make_unary_oper_node(node->first_token, NK_CONV, 
            make_ptr_type(dynamic_cast<ArrayType*>(type)->elem_type), node);
    // C11 6.3.2.1p4: a function designator with type ‘‘function returning type’’ is converted 
    // to an expression that has type ‘‘pointer to function returning type’’.
    case TK_FUNC:
        return make_unary_oper_node(node->first_token, NK_ADDR, make_ptr_type(type), node);
    }
    return node;
}

inline bool is_valid_pointer_binop(int op) {
    switch (op) {
    case '-': case '<': case '>': case P_EQ:
    case P_NE: case P_GE: case P_LE:
        return true;
    }
    return false;
}

NodePtr Parser::make_binop(TokenPtr tok, int op, NodePtr left, NodePtr right) {
    if(left->type->kind == TK_PTR && right->type->kind == TK_PTR) {
        if(!is_valid_pointer_binop(op)) {
            parser_error("nvalid pointer arith’)");
        }
        // C11 6.5.6.9: Pointer subtractions have type ptrdiff_t.
        if (op == '-')
            return make_binary_oper_node(tok, op, type_long, left, right);
        // C11 6.5.8.6, 6.5.9.3: Pointer comparisons have type int.
        return make_binary_oper_node(tok, op, type_long, left, right);
    }
    if(left->type->kind == TK_PTR)
        return make_binary_oper_node(tok, op, left->type, left, right);
    if(right->type->kind == TK_PTR)
        return make_binary_oper_node(tok, op, right->type, right, left);

    assert(left->type->is_arith_type() && right->type->is_arith_type());
    Type* type = usual_arith_convert(left->type, right->type);
    return make_binary_oper_node(tok, op, type, convert(left, type), convert(right, type));
}

//---------------------------- Expressions ----------------------------

/*
prim_expr : 'ident' | constant | 'string' | '(' expr ')' | generic
*/
NodePtr Parser::read_prim_expr() {
    TokenPtr tok = pp->get_token();

    switch(tok->kind) {
    case TIDENT:
        return read_ident(tok);
    case TNUMBER: case TCHAR:
        return read_constant(tok);
    case TSTRING:
        return read_string(tok);
    case '(': {
        TokenPtr t = pp->peek_token();
        NodePtr node;
        // '(' '{' compound_stmt '}' ')'
        // usually used in macro define
        // return compound_stmt's last expression
        if(pp->next('{')) {
            NodePtr stmt = read_compound_stmt(t);
            if(stmt == nullptr) {
                errort(t, "compound stmt in here should not be empty");
                return error_node;
            }
            shared_ptr<CompoundStmtNode> cstmt = 
                dynamic_pointer_cast<CompoundStmtNode>(stmt);
            node = cstmt;
            if(cstmt->list.size() > 0) {
                if(!cstmt->list.back()->type) {
                    node->type = type_void;
                }
                else {
                    node->type = cstmt->list.back()->type;
                }
            }
        }
        else {
            node = read_expr();
        }
        if(!pp->next(')'))
            parser_error("expected ‘)’");
        return node;
    }
    case KW_GENERIC:
        return read_generic();
    default:
        error("internal error: primary expression begin with '%s'(%s:%d:%d)", 
            tok->to_string(), tok->filename, tok->row, tok->col);
    }
    return error_node;
}

/* C11 6.5.1p2
An identifier is a primary expression, provided it has been declared as designating an
object (in which case it is an lvalue) or a function (in which case it is a function
designator)
*/
NodePtr Parser::read_ident(TokenPtr t) {
    shared_ptr<Ident> tok = dynamic_pointer_cast<Ident>(t);
    NodePtr var = scope->get(tok->name);
    if(var == nullptr) {
        TokenPtr tok2 = pp->peek_token();
        if(!tok2->is_keyword('(')) {
            errort(tok, "‘%s’ undeclared", tok->name);
            return error_node;
        }
        warnt(t, "implicit declaration of function '%s'", tok->name);
        Type* functype = make_func_type(type_void, vector<Type*>(), true, false);
        return make_func_designator_node(tok, functype, tok->name);
    }
    // var is function
    if(var->type->kind == TK_FUNC) {
        return make_func_designator_node(tok, var->type, tok->name);
    }
    // var is left-value or enum-constant
    return var;
}

/*
constant : 'int_const' | 'float_const' | 'enum_const'
*/
NodePtr Parser::read_constant(TokenPtr t) {
    if(t->kind == TCHAR) {
        shared_ptr<Char> tok = dynamic_pointer_cast<Char>(t);
        Type* type;
        switch(tok->encode_method) {
        case ENC_NONE:
        case ENC_WCHAR: // GNU Libc specifies that wchar_t be 32-bit
            type = type_int; break;
        case ENC_CHAR16:
            type = type_ushort; break;
        case ENC_CHAR32:
            type = type_uint; break;
        }
        return make_int_node(tok, type, tok->character);
    }

    shared_ptr<Number> tok = dynamic_pointer_cast<Number>(t);
    char* num = tok->value;
    // float constant
    if(strpbrk(num, ".pP") || (strncasecmp(num, "0x", 2) && strpbrk(num, "eE"))) {
        char* end;
        double value = strtod(num, &end);
        if(!strcasecmp(end, "l"))
            return make_float_node(tok, type_ldouble, value);
        if(!strcasecmp(end, "f"))
            return make_float_node(tok, type_float, value);
        if(*end != '\0')
            errort(t, "invalid suffix '%s' on floating constant", end);
        return make_float_node(tok, type_double, value);
    }
    // integer constant
    else {
        char* end;
        // support binary integer
        long long value = !strncasecmp(num, "ob", 2) ? strtoul(num, &end, 2) 
            : strtoul(num, &end, 0);
        Type* type = (!strcasecmp(num, "u") ? type_uint : 
                     (!strcasecmp(num, "l") ? type_long : 
                     (!strcasecmp(num, "ll") ? type_llong : 
                     ((!strcasecmp(num, "ul") || !strcasecmp(num, "lu")) ? type_ulong : 
                     ((!strcasecmp(num, "ull") || !strcasecmp(num, "llu")) ? type_ullong :
                     nullptr)))));
        if(type != nullptr)
            return make_int_node(tok, type, value);

        // C11 6.4.4.1p5: Decimal constant type is int, long, or long long
        // in here, long = long long
        if(num[0] != '0') {
            type = (value <= INT_MAX) ? type_int : type_long;
        }
        // Octal or hexadecimal constant type may be unsigned
        else {
            type = (value <= INT_MAX) ? type_int : 
                   (value <= UINT_MAX) ? type_uint :
                   (value <= LONG_MAX) ? type_long :
                   type_ulong;
        }
        return make_int_node(tok, type, value);
    }
}

NodePtr Parser::read_string(TokenPtr t) {
    shared_ptr<String> tok = dynamic_pointer_cast<String>(t);
    return make_string_node(tok, tok->value, tok->encode_method);
}

/*
*syntex
generic-selection:
    _Generic ( assignment-expression , generic-assoc-list )
generic-assoc-list:
    generic-association
    generic-assoc-list , generic-association
generic-association:
    type-name : assignment-expression
    default : assignment-expression

*example:
#define cbrt(X) _Generic((X), \
    long double: cbrtl, \
    default: cbrt, \
    float: cbrtf \
    )(X)
*/
NodePtr Parser::read_generic() {
    if(!pp->next('(')) {
        parser_error("expected ‘(’");
        return error_node;
    }
    TokenPtr tok = pp->peek_token();
    NodePtr expr = read_assign_expr();
    if(!pp->next(',')) {
        parser_error("expected ‘,’");
        return error_node;
    }
    vector<pair<Type*, NodePtr>> list = read_generic_assoc_list();
    size_t i;
    for(i = 0; i < list.size(); ++i) {
        Type* type = list[i].first;
        if(type == nullptr) { // default case
            return list[i].second;
        }
        if(type->is_compatible(expr->type))
            return list[i].second;
    }
    errort(tok, "‘_Generic’ selector of type '%s' is not compatible with any association", 
        expr->type->to_string());
    return error_node;
} 

vector<pair<Type*, NodePtr>> Parser::read_generic_assoc_list() {
    vector<pair<Type*, NodePtr>> list;
    NodePtr default_case = nullptr;
    while(true) {
        if(pp->next(')')) break;
        if(pp->next(KW_DEFAULT)) {
            if(default_case) {
                parser_error("duplicate ‘default’ case");
                return list;
            }
            if(!pp->next(':')) {
                parser_error("expected ‘:'");
                return list;
            }
            default_case = read_assign_expr();
        }
        else {
            Type* type = read_type_name();
            if(!pp->next(':')) {
                parser_error("expected ‘:'");
                return list;
            }
            NodePtr expr = read_assign_expr();
            list.push_back(make_pair(type, expr));
        }
        pp->next(',');
    }
    list.push_back(make_pair(nullptr, default_case));
    return list;
}

/*
post_expr	:	prim_expr post_expr_tail 
            | '(' type_name ')' '{' initializer_list 'opt:,' '}' post_expr_tail

If the next token is '(', there will be two branches: "prim_expr ..." and 
"'(' type_name ...". "prim_expr ..." must be  "'(' expr ')' ...".
Observe the ll(1)-first table(in tools/syntex/table), we can find that expr is not empty and 
the beginning is not type_name.
*/

NodePtr Parser::read_post_expr(bool maybe_return_type) {
    TokenPtr tok = pp->get_token();
    // Create a initializer of type 'type_name'.
    // Use a temporary variable to refer to him
    if(tok->is_keyword('(') && is_type_name(pp->peek_token())) {
        Type* type = read_type_name();
        if(!pp->next(')')) {
            parser_error("expected ‘)'");
            return error_node;
        }
        if(pp->peek_token()->kind != '{') {
            // if next token isn't '{', expr must be "'(' type_name ')' ..."
            if(maybe_return_type) {
                return make_typedef_node(tok, type, "-", nullptr);
            }
            parser_error("expected ‘{'");
            return error_node;
        }
        vector<NodePtr> init_list; 
        read_initializer_list(init_list, type);
        shared_ptr<LocalVarNode> tmp_var = make_localvar_node(tok, type, nullptr, scope);
        tmp_var->init_list = init_list;
        return read_post_expr_tail(tmp_var);
    }
    pp->unget_token(tok);
    NodePtr node = read_prim_expr();
    return read_post_expr_tail(node);
}

/*
post_expr_tail  :	'[' expr ']' post_expr_tail 
				|	'(' args_expr ')' post_expr_tail
				|	'.' ident post_expr_tail
				|	'->' ident post_expr_tail
				|	'++' post_expr_tail
				|	'--' post_expr_tail
				|	'empty'
*/
NodePtr Parser::read_post_expr_tail(NodePtr node) {
    // array subsripting
    if(pp->next('[')) {
        NodePtr sub = read_expr();
        if(!pp->next(']')) {
            parser_error("expected ‘]'");
            return error_node;
        }
        if(node->type->kind != TK_ARRAY && node->type->kind != TK_PTR) {
            errort(node->first_token, "subscripted value is neither array nor pointer nor vector");
            return error_node;
        }
        // C11 6.5.2.1p2: E1[E2] is identical to (*((E1)+(E2)))
        NodePtr p = make_binop(sub->first_token, '+', convert(node), convert(sub));
        return read_post_expr_tail(make_deref_node(sub->first_token, p));
    }
    // function calls
    if(pp->next('(')) {
        node = convert(node);
        node = read_func_call(node);
        return read_post_expr_tail(node);
    }
    // Structure and union members
    if(pp->next('.')) {
        node = read_struct_member(node);
        return read_post_expr_tail(node);
    }
    // Structure and union members
    if(pp->next(P_ARROW)) {
        if(node->type->kind != TK_PTR) {
            parser_error("invalid type argument of ‘->’ (have ‘%s’)", 
                node->type->to_string());
            return error_node;
        }
        PtrType* type = dynamic_cast<PtrType*>(node->type);
        node = make_unary_oper_node(node->first_token, NK_DEREF, type->ptr_type, node);
        node = read_struct_member(node);
        return read_post_expr_tail(node);
    }
    // Postfix increment and decrement operators
    TokenPtr tok = pp->peek_token();
    if(pp->next(P_INC) || pp->next(P_DEC)) {
        int op = (tok->kind == P_INC ? NK_POST_INC : NK_POST_DEC);
        if(!is_lvalue(node)) {
            errort(tok, "lvalue required as %s operand", 
                op == NK_POST_INC ? "increment" : "decrement");
            return error_node;
        }
        return make_unary_oper_node(node->first_token, op, node->type, node);
    }
    // finish post_expr
    return node;
}

NodePtr Parser::read_func_call(NodePtr func) {
    if(func->kind == NK_ADDR) {
        NodePtr desg = 
            (dynamic_pointer_cast<UnaryOperNode>(func)->operand);
        assert(desg->kind == NK_FUNC_DESG);
        vector<NodePtr> args = read_func_call_args(desg->type);
        return make_func_call_node(func->first_token, desg, args);;
    }
    assert(func->type->kind == TK_PTR);
    Type* func_type = dynamic_cast<PtrType*>(func->type)->ptr_type;
    vector<NodePtr> args = read_func_call_args(func_type);
    return make_funcptr_call_node(func->first_token, func, func_type, args);
}

vector<NodePtr> Parser::read_func_call_args(Type* func_type) {
    assert(func_type->kind == TK_FUNC);
    FuncType* ftype = dynamic_cast<FuncType*>(func_type);
    vector<NodePtr> args;
    vector<Type*> param_types = ftype->param_types;
    size_t i = 0;
    while(true) {
        if(pp->next(')')) {
            break;
        }
        NodePtr arg = convert(read_assign_expr());
        Type* param_type;
        if(i < param_types.size()) {
            param_type = param_types[i++];
        }
        else {
            param_type = arg->type->is_float_type() ? type_double :
                arg->type->is_int_type() ? type_int :
                arg->type;
        }
        if(!is_assignable(param_type, arg->type)) {
            break;;
        }
        arg = convert(arg, param_type);
        args.push_back(arg);
        if(pp->next(')')) break;
        if(!pp->next(',')) {
            parser_error("expected ‘,'");
            break;
        }
    }
    return args;
}

NodePtr Parser::read_struct_member(NodePtr node) {
    Type* type = node->type;
    TokenPtr name = pp->get_token();
    char* name_str = name->to_string();
    if(name->kind != TIDENT) {
        errort(name, "expected a field name, but got %s", name_str);;
        return error_node;
    }
    if(type->kind != TK_STRUCT && type->kind != TK_UNION) {
        errort(name,
            "request for member ‘%s’ in something not a structure or union", 
            name_str);
        return error_node;
    }
    StructType* struc = dynamic_cast<StructType*>(type);
    Type* field = get_struct_field_type(struc->fields, name_str, nullptr);
    if(!field) {
        errort(name, "‘%s’ has no member named ‘%s’", type->to_string(), name_str);
        return error_node;
    }
    return make_struct_member_node(name, field, node, name_str);
} 

/*
unary_expr :	post_expr
			|	'++' unary_expr
			|	'--' unary_expr
			|	unary_oper cast_expr
			|	'sizeof' sizeof_operand
			|	'_Alignof' '(' type_name ')'
*/
NodePtr Parser::read_unary_expr() {
    TokenPtr tok = pp->get_token();
    switch(tok->kind) {
        // Prefix increment and decrement operators
        case P_INC: case P_DEC: {
            int op = (tok->kind == P_INC ? NK_PRE_INC : NK_PRE_DEC);
            NodePtr operand = read_unary_expr();
            if(!is_lvalue(operand)) {
                errort(tok, "lvalue required as %s operand", 
                    op == NK_PRE_INC ? "increment" : "decrement");
                return error_node;
            }
            return make_unary_oper_node(tok, op, operand->type, operand);
        }
        case P_LOGAND: {
            TokenPtr tok2 = pp->get_token();
            if(tok2->kind != TIDENT) {
                errort(tok, "expected a label name after &&");
                return error_node;
            }
            NodePtr node = make_label_addr_node(tok, tok2->to_string());
            gotos.push_back(node);
            return node;
        }
        case '&': {
            NodePtr operand = read_cast_expr();
            if(operand->kind == NK_FUNC_DESG)
                return convert(operand);
            if(!is_lvalue(operand)) {
                errort(tok, "lvalue required as unary '&' operand");
                return error_node;
            }
            return make_unary_oper_node(tok, NK_ADDR, 
                make_ptr_type(operand->type), operand);
        }
        case '*': {
            NodePtr operand = convert(read_cast_expr());
            if(operand->type->kind != TK_PTR) {
                errort(tok, "invalid type argument of unary ‘*’ (have ‘%s’)",
                    operand->type->to_string());
                return error_node;
            }
            Type* ptr_type = dynamic_cast<PtrType*>(operand->type)->ptr_type;
            if(ptr_type->kind == TK_FUNC)
                return operand;
            return make_unary_oper_node(tok, NK_DEREF, ptr_type, operand);
        }
        case '+': return read_cast_expr();
        case '-': {
            NodePtr operand = read_cast_expr();
            if(!operand->type->is_arith_type()) {
                errort(tok, "wrong type('%s') argument to unary minus", 
                    operand->type->to_string());
                return error_node;
            }
            if(operand->type->is_int_type()) {
                return make_binop(tok, '-', convert(make_int_node(tok, operand->type, 0)), 
                    convert(operand));
            }
            return make_binop(tok, '-', make_float_node(tok, operand->type, 0), operand);
        }
        case '~': {
            NodePtr operand = convert(read_cast_expr());
            if(!operand->type->is_int_type()) {
                errort(tok, "wrong type('%s') argument to bit-complement", 
                    operand->type->to_string());
                return error_node;
            }
            return make_unary_oper_node(tok, '~', operand->type, operand);
        }
        case '!': {
            NodePtr operand = convert(read_cast_expr());
            return make_unary_oper_node(tok, '!', type_int, operand);
        }
        case KW_SIZEOF: {
            Type* type = read_sizeof_operand();
            // GNU extension: Sizeof on void or function type is 1
            int size = (type->kind == TK_VOID || 
                type->kind == TK_FUNC) ? 1: type->size;
            assert(size >= 0);
            return make_int_node(tok, type_ulong, size);
        }
        case KW_ALIGNOF: {
            if(!pp->next('(')) {
                parser_error("expected ‘(’");
                return error_node;
            }
            Type* type = read_type_name();
            if(!pp->next(')')) {
                parser_error("expected ‘)’");
                return error_node;
            }
            return make_int_node(tok, type_ulong, type->align);
        }
    }
    pp->unget_token(tok);
    return read_post_expr();
}

/*
sizeof_operand : unary_expr | '(' type_name ')'
*/
Type* Parser::read_sizeof_operand() {
    TokenPtr tok = pp->get_token();
    if(tok->is_keyword('(') && is_type_name(pp->peek_token())) {
        pp->unget_token(tok);
        NodePtr node = read_post_expr(true);
        // '(' type_name ')'
        if(node->kind == NK_TYPEDEF) {
            return node->type;
        }
        // unary_expr => post_expr
        return node->type;
    }
    // other unary_expr
    pp->unget_token(tok);
    NodePtr node = read_unary_expr();
    return node->type;
}

/*
cast_expr :	unary_expr | '(' type_name ')' cast_expr
*/
NodePtr Parser::read_cast_expr() {
    TokenPtr tok = pp->get_token();
    if(tok->is_keyword('(') && is_type_name(pp->peek_token())) {
        pp->unget_token(tok);
        NodePtr node = read_post_expr(true);
        // '(' type_name ')' cast_expr
        if(node->kind == NK_TYPEDEF) {
            Type* type = node->type;
            return make_unary_oper_node(tok, NK_CAST, type, read_cast_expr());
        }
        // unary_expr => post_expr
        return node;
    }
    // other unary_expr
    pp->unget_token(tok);
    return read_unary_expr();
}

/*
mul_expr :	cast_expr mul_expr_tail 
mul_expr_tail :	'*' cast_expr mul_expr_tail
				|	'/' cast_expr mul_expr_tail
				|	'%' cast_expr mul_expr_tail
				|	'empty'
*/
NodePtr Parser::read_mul_expr() {
    NodePtr node = read_cast_expr();
    return read_mul_expr_tail(node);
}
NodePtr Parser::read_mul_expr_tail(NodePtr node) {
    TokenPtr tok = pp->peek_token();
    if(pp->next('*')) {
        node = make_binop(tok, '*', convert(node), convert(read_cast_expr()));
        return read_mul_expr_tail(node);
    }
    if(pp->next('/')) {
        node = make_binop(tok, '/', convert(node), convert(read_cast_expr()));
        return read_mul_expr_tail(node);
    }
    if(pp->next('%')) {
        node = make_binop(tok, '%', convert(node), convert(read_cast_expr()));
        return read_mul_expr_tail(node);
    }
    return node;
}

/*
add_expr :	mul_expr add_expr_tail 
add_expr_tail :	'+' mul_expr add_expr_tail
				|	'-' mul_expr add_expr_tail
				|	'empty'
*/
NodePtr Parser::read_add_expr(){
    NodePtr node = read_mul_expr();
    return read_add_expr_tail(node);
}
NodePtr Parser::read_add_expr_tail(NodePtr node) {
    TokenPtr tok = pp->peek_token();
    if(pp->next('+')) {
        node = make_binop(tok, '+', convert(node), convert(read_mul_expr()));
        return read_add_expr_tail(node);
    }
    if(pp->next('-')) {
        node = make_binop(tok, '-', convert(node), convert(read_mul_expr()));
        return read_add_expr_tail(node);
    }
    return node;
}

/*
shift_expr :	add_expr shift_expr_tail 
shift_expr_tail :	'<<' add_expr shift_expr_tail
				|	'>>' add_expr shift_expr_tail
				|	'empty'
*/
NodePtr Parser::read_shift_expr() {
    NodePtr node = read_add_expr();
    return read_shift_expr_tail(node);
}
NodePtr Parser::read_shift_expr_tail(NodePtr node) {
    TokenPtr tok = pp->get_token();
    if(tok->is_keyword(P_SAL) || tok->is_keyword(P_SAR)) {
        int op = tok->kind;
        op = (op == P_SAL ? NK_SAL : (node->type->is_unsigned ? NK_SHR : NK_SAR));
        NodePtr right = read_add_expr();
        if(!node->type->is_int_type() || !right->type->is_int_type()) {
            errort(tok, "invalid operands to binary << (have ‘%s’ and ‘%s’)", 
                node->type->to_string(), right->type->to_string());
            return error_node;
        }
        node = make_binop(tok, op, convert(node), convert(right));
        return read_shift_expr_tail(node);
    }
    pp->unget_token(tok);
    return node;
}

/*
rela_expr :	shift_expr rela_expr_tail 
rela_expr_tail :	'<' shift_expr rela_expr_tail
				|	'>' shift_expr rela_expr_tail
				|	'<=' shift_expr rela_expr_tail
				|	'>=' shift_expr rela_expr_tail
				|	'empty'
*/
NodePtr Parser::read_relational_expr() {
    NodePtr node = read_shift_expr();
    return read_relational_expr_tail(node);
}
NodePtr Parser::read_relational_expr_tail(NodePtr node) {
    bool is_relational_expr = false;
    TokenPtr tok = pp->peek_token();
    if(pp->next('<')) {
        is_relational_expr = true;
        node = make_binop(tok, '<', convert(node), convert(read_shift_expr()));
    }
    if(pp->next('>')) {
        is_relational_expr = true;
        node = make_binop(tok, '<', convert(read_shift_expr()), convert(node));
    }
    if(pp->next(P_LE)) {
        is_relational_expr = true;
        node = make_binop(tok, P_LE, convert(node), convert(read_shift_expr()));
    }
    if(pp->next(P_GE)) {
        is_relational_expr = true;
        node = make_binop(tok, P_LE, convert(read_shift_expr()), convert(node));
    }
    if(is_relational_expr) {
        // C11 6.5.8p6: relational operators shall yield 1 if the specified 
        // relation is true and 0 if it is false. The result has type int.
        node->type = type_int;
        return read_relational_expr_tail(node);
    }
    return node;
}

/*
equal_expr :	rela_expr equal_expr_tail 
equal_expr_tail :	'==' rela_expr equal_expr_tail
					|	'!=' rela_expr equal_expr_tail
					|	'empty'
*/
NodePtr Parser::read_equal_expr() {
    NodePtr node = read_relational_expr();
    return read_equal_expr_tail(node);
}
NodePtr Parser::read_equal_expr_tail(NodePtr node) {
    TokenPtr tok = pp->get_token();
    if(tok->is_keyword(P_EQ) || tok->is_keyword(P_NE)) {
        int op = tok->kind;
        NodePtr right = read_relational_expr();
        node = make_binop(tok, op, convert(node), convert(right));
        node->type = type_int; // bool convert to int
        return read_equal_expr_tail(node);
    }
    pp->unget_token(tok);
    return node;
}

/*
and_expr :	equal_expr and_expr_tail 
and_expr_tail :	'&' equal_expr and_expr_tail
				|	'empty'
*/
NodePtr Parser::read_and_expr() {
    NodePtr node = read_equal_expr();
    return read_and_expr_tail(node);
}
NodePtr Parser::read_and_expr_tail(NodePtr node) {
    TokenPtr tok = pp->peek_token();
    if(pp->next('&')) {
        node = make_binop(tok, '&', convert(node), convert(read_equal_expr()));
        return read_and_expr_tail(node);
    }
    return node;
}   

/*
xor_expr :	and_expr xor_expr_tail 
xor_expr_tail :	'^' and_expr xor_expr_tail
				|	'empty'
*/
NodePtr Parser::read_xor_expr() {
    NodePtr node = read_and_expr();
    return read_xor_expr_tail(node);
}
NodePtr Parser::read_xor_expr_tail(NodePtr node) {
    TokenPtr tok = pp->peek_token();
    if(pp->next('^')) {
        node = make_binop(tok, '^', convert(node), convert(read_and_expr()));
        return read_xor_expr_tail(node);
    }
    return node;
}

/*
or_expr :	xor_expr or_expr_tail 
or_expr_tail :	'|' xor_expr or_expr_tail
				|	'empty'
*/
NodePtr Parser::read_or_expr() {
    NodePtr node = read_xor_expr();
    return read_or_expr_tail(node);
}
NodePtr Parser::read_or_expr_tail(NodePtr node) {
    TokenPtr tok = pp->peek_token();
    if(pp->next('|')) {
        node = make_binop(tok, '|', convert(node), convert(read_xor_expr()));
        return read_or_expr_tail(node);
    }
    return node;
}

/*
land_expr :	or_expr land_expr_tail 
land_expr_tail :	'&&' or_expr land_expr_tail
				|	'empty'
*/
NodePtr Parser::read_land_expr() {
    NodePtr node = read_or_expr();
    return read_land_expr_tail(node);
}
NodePtr Parser::read_land_expr_tail(NodePtr node) {
    TokenPtr tok = pp->peek_token();
    if(pp->next(P_LOGAND)) {
        node = make_binop(tok, P_LOGAND, convert(node), convert(read_or_expr()));
        return read_land_expr_tail(node);
    }
    return node;
}

/*
lor_expr :	land_expr lor_expr_tail 
lor_expr_tail :	'||' land_expr lor_expr_tail
				|	'empty'
*/
NodePtr Parser::read_lor_expr() {
    NodePtr node = read_land_expr();
    return read_lor_expr_tail(node);
}
NodePtr Parser::read_lor_expr_tail(NodePtr node) {
    TokenPtr tok = pp->peek_token();
    if(pp->next(P_LOGOR)) {
        node = make_binop(tok, P_LOGOR, convert(node), convert(read_land_expr()));
        return read_lor_expr_tail(node);
    }
    return node;
}

/*
cond_expr :	lor_expr cond_expr_tail
cond_expr_tail :	'?' expr ':' cond_expr | 'empty'
*/
NodePtr Parser::read_cond_expr() {
    NodePtr cond = read_lor_expr();
    return read_cond_expr_tail(cond);
}
NodePtr Parser::read_cond_expr_tail(NodePtr cond) {
    if(pp->next('?')) {
        cond = convert(cond); // if array or func-desg: to pointer
        // C11 6.5.15p2: condition operand shall have scalar type
        if(!cond->type->is_scalar_type()) {
            parser_error("used %s type value where scalar is required", 
                cond->type->to_string());
        }
        NodePtr then;
        TokenPtr tok = pp->get_token();
        // [GNU] Omitting the middle operand is allowed.
        if(tok->kind != ':') {
            pp->unget_token(tok);
            then = convert(read_expr());
            tok = pp->get_token();
        }
        if(tok->kind != ':') {
            parser_error("expected ‘:'");
            return error_node;
        }
        NodePtr els = convert(read_cond_expr());
        Type* tt = then ? then->type : cond->type;
        Type* et = els->type;
        if(tt->is_arith_type() && et->is_arith_type()) {
            Type* u = usual_arith_convert(tt, et);
            return make_ternary_oper_node(cond->first_token, u, cond, 
                (then ? convert(then, u) : nullptr), convert(els, u));
        }
        // if(!tt->is_compatible(et)) {
        //     errort(tok, "operand types are incompatible('%s' and '%s')",
        //         tt->to_string(), et->to_string());
        //     return nullptr;
        // }
        return make_ternary_oper_node(cond->first_token, et, cond, then, els);
    }
    return cond;
}

/* 
*assign_expr: original syntex: 
assign_expr :	cond_expr | unary_expr assign_oper assign_expr

Because cond_expr's first table and unary_expr's first table 
has many intersections, I treat unary_expr as cond_expr. If next token 
is assign_oper, make sure that result of read_cond_expr is lvalue.

*assign_expr: current syntex: 
assign_expr :	cond_expr assign_expr_tail
assign_expr_tail: assign_oper assign_expr
*/
int get_assign_op(TokenPtr tok) {
    switch(tok->kind) {
    case '=': return '=';
    case P_ASSIGN_ADD: return '+';
    case P_ASSIGN_SUB: return '-';
    case P_ASSIGN_MUL: return '*';
    case P_ASSIGN_DIV: return '/';
    case P_ASSIGN_MOD: return '%';
    case P_ASSIGN_AND: return '&';
    case P_ASSIGN_XOR: return '^';
    case P_ASSIGN_OR:  return '|';
    case P_ASSIGN_SAL: return NK_SAL;
    case P_ASSIGN_SAR: return NK_SAR;
    }
    return 0;
}

NodePtr Parser::read_assign_expr() {
    NodePtr node = read_cond_expr();
    return read_assign_expr_tail(node);
}
NodePtr Parser::read_assign_expr_tail(NodePtr node) {
    TokenPtr tok = pp->get_token();
    int op = get_assign_op(tok);
    if(op) {
        if(!is_lvalue(node)) {
            errort(tok, "lvalue required as left operand of assignment");
            return error_node;
        }
        NodePtr right = convert(read_assign_expr());
        if(op != '=') {
            right = make_binop(tok, op, convert(node), right);
        }
        if(op == NK_SAR && node->type->is_unsigned) {
            op = NK_SHR;
        }
        if(node->type->is_arith_type() && node->type->kind != right->type->kind) {
            right = convert(right, node->type);
        }
        return make_binary_oper_node(tok, '=', node->type, node, right);
    }
    pp->unget_token(tok);
    return node;
}


/*
expr :	assign_expr expr_tail
expr_tail :	',' assign_expr expr_tail
			|	'empty'
*/
NodePtr Parser::read_expr() {
    NodePtr node = read_assign_expr();
    while(pp->next(',')) {
        NodePtr right = read_assign_expr();
        // C11 6.5.17p2: the right operand is evaluated; the result 
        // has its type and value.
        node = make_binary_oper_node(right->first_token, ',', right->type, node, right);
    }
    return node;
}

/*
const_expr :	cond_expr
*/
NodePtr Parser::read_const_expr() {
    return read_cond_expr();
}

//---------------------------- Declarations ----------------------------

/*
decl :	decl_spec init_decl_list ';' | static_assert_decl

init_declarator_list :	init_declarator init_declarator_list_tail | 'empty'
init_declarator_list_tail :	',' init_declarator init_declarator_list_tail | 'empty'

init_declarator :	declarator init_declarator_tail
init_declarator_tail :	'=' initializer | 'empty'

*/
void Parser::read_decl(vector<NodePtr>& list, bool isglobal) {
    TokenPtr tok = pp->peek_token();
    if(tok->kind == KW_STATIC_ASSERT) {
        tok = pp->get_token();
        read_static_assert(tok);
        return;
    }
    Type* basetype = read_decl_spec_opt();
    if(pp->next(';')) return;
    while(true) {
        char* name = nullptr;
        Type* type = read_declarator(&name, copy_incomplete_type(basetype), nullptr, DK_CONCRETE);
        if(type->storage_class == KW_TYPEDEF) {
            make_typedef_node(tok, type, name, scope);
        }
        else if(type->is_static() && !isglobal) {
            NodePtr var = make_static_localvar_node(tok, type, name, scope);
            shared_ptr<DeclNode> decl_node = make_decl_node(tok, type, var);
            if(pp->next('=')) {
                scope->clear_local();
                read_initializer(type, decl_node->init_list);
                scope->recover_local();
            }
            toplevers.push_back(decl_node);
        }
        else {
            if(type->kind == TK_VOID) {
                errort(tok, "type void is not allowed");
                return;
            }
            NodePtr var;
            if(isglobal) 
                var = make_globalvar_node(tok, type, name, scope);
            else 
                var = make_localvar_node(tok, type, name, scope);
            shared_ptr<DeclNode> decl_node = make_decl_node(tok, type, var);
            if(pp->next('=')) {
                read_initializer(type, decl_node->init_list);
                list.push_back(decl_node);
            }
            else if(type->storage_class != KW_EXTERN && type->kind != TK_FUNC) {
                list.push_back(decl_node);
            }
        }
        if(pp->next(';')) return;
        if(!pp->next(',')) {
            parser_error("';' or ',' are expected");
            return;
        }
    }
}

/*
static_assert_decl : '_Static_assert' '(' const_expr ',' 'string' ')' ';'
*/
void Parser::read_static_assert(TokenPtr first_token) {
    if(!pp->next('(')) {
        parser_error("expected ‘(’");
        return;
    }
    long long val = read_const_expr()->eval_int();
    if(!pp->next(',')) {
        parser_error("expected ‘,’");
        return;
    }
    TokenPtr tok = pp->get_token();
    if(tok->kind != TSTRING) {
        errort(tok, "expected string literal");
        return;
    }
    if(!pp->next(')')) {
        parser_error("expected ‘)’");
        return;
    }
    if(!pp->next(';')) {
        parser_error("expected ‘;’");
        return;
    }
    if (!val)
        errort(first_token, "_Static_assert failed: %s", tok->to_string());
}

/*
decl_spec :	storage_spec decl_spec_tail
			|	type_spec decl_spec_tail
			|	type_qual decl_spec_tail
			|	func_spec decl_spec_tail
			|	alignment_spec decl_spec_tail
decl_spec_tail :	decl_spec | 'empty'
*/
Type* Parser::read_decl_spec() {
    TokenPtr first_tok = pp->peek_token();
    if(!is_type_name(first_tok)) {
        errort(first_tok, "type name expected");
        return nullptr;
    }

    Type* type = nullptr;
    bool is_def_type = false;

    int sclass = 0;
    vector<int> type_qualifier;
    bool is_inline = false;
    bool is_noreturn = false;
    int sig = 0;
    int align = -1;

    enum { SIZE_SHORT = 1, SIZE_LONG, SIZE_LLONG };
    int size = 0;

    while(true) {
        TokenPtr tok = pp->get_token();
        if(tok->kind == TEOF) {
            errort(tok, "unexpected end");
            return nullptr;
        }
        if(tok->kind == TIDENT) {
            Type* def = get_typedef(tok->to_string());
            if(def && type) {
                errort(tok, "two or more data types in declaration specifiers");
                return nullptr;
            }
            if(def && !type) {
                is_def_type = true;
                type = def;
                continue;
            }
        }
        if(!is_type_name(tok)) {
            pp->unget_token(tok);
            break;
        }
        switch(tok->kind) {
        // storage-class-specifier
        case KW_TYPEDEF:  case KW_EXTERN: case KW_STATIC: 
        case KW_THREAD_LOCAL: case KW_AUTO:  case KW_REGISTER: {
            if(sclass) {
                errort(tok, "multiple storage classes in declaration specifiers");
                return nullptr;
            }
            sclass = tok->kind;
            break;
        }
        // type-qualifier
        case KW_CONST: case KW_RESTRICT: case KW_VOLATILE:
        case KW_ATOMIC: {
            type_qualifier.push_back(tok->kind);
            break;
        }
        // function-specifier
        case KW_INLINE: is_inline = true; break;
        case KW_NORETURN: is_noreturn = true; break;
        // type-specifier
        case KW_VOID: type = type_void; break;
        case KW_BOOL: type = type_bool; break;
        case KW_CHAR: type = type_char; break;
        case KW_INT: type = type_int; break;
        case KW_FLOAT: type = type_float; break;
        case KW_DOUBLE: type = type_double; break;
        case KW_SIGNED: case KW_UNSIGNED: {
            if(sig) {
                errort(tok, "multiple ‘unsigned’ or 'signed'");
                return nullptr;
            }
            sig = tok->kind;
            break;
        }
        case KW_SHORT: {
            if(size) {
                errort(tok, "both 'short' and '%s' in declaration specifiers",
                    size == SIZE_SHORT ? "short" : "long");
                return nullptr;
            }
            size = SIZE_SHORT;
            break;
        }
        case KW_LONG: {
            if(size == 0) size = SIZE_LONG;
            else if(size == SIZE_LONG) size = SIZE_LLONG;
            else {
                errort(tok, "both 'long long' and '%s' in declaration specifiers",
                    size == SIZE_SHORT ? "short" : "long");
                return nullptr;
            }
            break;
        }
        case KW_STRUCT: case KW_UNION: {
            if(type) {
                errort(tok, "two or more data types in declaration specifiers");
                return nullptr;
            }
            type = read_struct_or_union_spec(tok->kind == KW_STRUCT ? TK_STRUCT : TK_UNION);
            break;
        }
        case KW_ENUM: {
            if(type) {
                errort(tok, "two or more data types in declaration specifiers");
                return nullptr;
            }
            type = read_enum_spec();
            break;
        }
        case KW_ALIGNAS: {
            int val = read_alignas();
            if (val < 0){
                errort(tok, "negative alignment: %d", val);
                return nullptr;
            }
            // C11 6.7.5p6: An alignment specification of zero has no effect.
            if(val == 0)
                break;
            // C11 6.7.5p6: When multiple alignment specifiers occur in a 
            // declaration, the effective alignment requirement is the 
            // strictest specified alignment.
            if(align == -1 || val < align)
                align = val;
            break;
        }
        // typeof: GNU extension
        case KW_TYPEOF: {
            if(type) {
                errort(tok, "two or more data types in declaration specifiers");
                return nullptr;
            }
            type = read_typeof();
            break;
        }
        default:
            pp->unget_token(tok);
            goto update_type;
        }
    }
update_type:
    // update or check type
    if(align != -1 && !is_power2(align)) {
        errort(first_tok, "alignment:%d is not a positive power of 2", align);
        return nullptr;
    }
    if(is_def_type) {
        if(sig || size) {
            errort(first_tok, "type '%s' cannot be (un)signed or short(long)",
                type->to_string());
            return nullptr;
        }
        goto complete_type;
    }
    if(!type) {
        // warnt(first_tok, "type defaults to ‘int’");
        type = type_int;
    }
    if(size == SIZE_SHORT) {
        if(type != type_int) {
            errort(first_tok, "both ‘short’ and ‘%s’ in declaration specifiers",
                type->to_string());
            return nullptr;
        }
        type = type_short;
    }
    else if(size == SIZE_LONG) {
        if(type != type_int && type != type_double) {
            errort(first_tok, "both long and ‘%s’ in declaration specifiers",
                type->to_string());
            return nullptr;
        }
        type = (type == type_int ? type_long : type_ldouble);
    }
    else if(size == SIZE_LLONG) {
        if(type != type_int) {
            errort(first_tok, "both long long and ‘%s’ in declaration specifiers",
                type->to_string());
            return nullptr;
        }
        type = type_llong;
    }
    if(sig != 0) {
        if(type->kind < TK_CHAR || type->kind > TK_LONG_LONG) {
            errort(first_tok, "both ‘%s’ and ‘%s’ in declaration specifiers",
                sig == KW_SIGNED ? "signed" : "unsigned", type->to_string());
            return nullptr;
        }
        if(sig == KW_UNSIGNED) {
            switch(type->kind) {
            case TK_CHAR: type = type_uchar; break;
            case TK_SHORT: type = type_ushort; break;
            case TK_INT: type = type_uint; break;
            case TK_LONG: type = type_ulong; break;
            case TK_LONG_LONG: type = type_ullong; break;
            }
        }
    }
    // update done
complete_type:
    if((sclass && sclass != KW_TYPEDEF) || !type_qualifier.empty() 
            || is_inline || is_noreturn || align != -1) {
        type = type->copy();
    }
    type->storage_class = sclass;
    if(!type_qualifier.empty()) type->type_qualifier = type_qualifier;
    type->is_inline = is_inline;
    type->is_noreturn = is_noreturn;
    if(align != -1) type->align = align;
    return type;
}

// In extern declaration, if type specifier missing, 
// type defaults to ‘int’ in declaration.
Type* Parser::read_decl_spec_opt() {
    if(is_type_name(pp->peek_token())) {
        return read_decl_spec();
    }
    warnt(pp->peek_token(), "type defaults to ‘int’ in declaration");
    return type_int;
}

/*
struct_or_union_spec :	'struct|union_{' struct_decl_list '}' 
					 | 'struct|union_ident_{' struct_decl_list '}'
					 | 'struct|union_ident'          
*/
Type* Parser::read_struct_or_union_spec(int kind) {
    TokenPtr tok = pp->get_token();
    char* tag;
    Type* type;
    if(tok->kind == TIDENT) {
        tag = tok->to_string();
        auto iter = tags.find(tag);
        if(iter != tags.end()) {
            type = iter->second;
            if(type->kind != kind) {
                errort(tok, "'%s' defined as wrong kind of tag", tag);
                return nullptr;
            }
        }
        else {
            type = make_struct_type(kind, tag);
            tags[tag] = type;
        }
    }
    else {
        // A struct-declaration that does not declare an anonymous 
        // structure or anonymous union.
        // shall contain a struct-declarator-list.
        if(tok->kind != '{') {
            errort(tok, "expected '{'");
            return nullptr;
        }
        pp->unget_token(tok);
        type = make_struct_type(kind);
    }
    if(!pp->next('{'))
        return type;

    int size = 0, align = 1;
    
    StructType* struct_type = dynamic_cast<StructType*>(type);
    vector<pair<char*, Type*>> fields;
    read_struct_decl_list(fields);
    check_struct_flexible_array(tok, fields);
    if(kind == TK_STRUCT)
        struct_type->fields = update_struct_offset(size, align, fields);
    else
        struct_type->fields = update_union_offset(size, align, fields);
    struct_type->size = size;
    struct_type->align = align;
    return struct_type;
}

/*
struct_decl_list :	struct_decl struct_decl_list | 'empty'
struct_decl :	spec_qual_list struct_declarator_list ';' | static_assert_decl

spec_qual_list :	type_spec spec_qual_list_tail
				|	type_qual spec_qual_list_tail
spec_qual_list_tail : 	spec_qual_list | 'empty'

struct_declarator_list :	struct_declarator struct_declarator_list_tail | 'empty'
struct_declarator_list_tail :	',' struct_declarator struct_declarator_list_tail | 'empty'

struct_declarator :	declarator struct_declarator_tail | ':' const_expr
struct_declarator_tail :	':' const_expr | 'empty'           
*/
void Parser::read_struct_decl_list(std::vector<std::pair<char*, Type*>>& fields) {
    while(true) {
        TokenPtr tok = pp->peek_token();
        if(tok->kind == KW_STATIC_ASSERT) {
            read_static_assert(tok);
            continue;
        }
        if(!is_type_name(tok))
            break;
        // read spec_qual_list
        Type* basetype = read_decl_spec();
        if(basetype->kind == TK_STRUCT && pp->next(';')) {
            fields.push_back(make_pair(nullptr, basetype));
            continue;
        }
        // read struct_declarator_list
        while(true) {
            // read struct_declarator
            char* name = nullptr;
            Type* fieldtype = read_declarator(&name, basetype, nullptr, DK_OPTIONAL);
            if(fieldtype->kind == TK_VOID) {
                errort(tok, "variable or field ‘%s’ declared void", name);
                return;
            }
            if(!fieldtype->from_copy) fieldtype = fieldtype->copy();
            // read bit-field
            if(pp->next(':')) {
                if(!fieldtype->is_int_type()) {
                    errort(tok, "non-integer type:%s cannot be a bitfield", 
                        fieldtype->to_string());
                    return;
                }
                int val = read_const_expr()->eval_int();
                int maxsize = fieldtype->kind == TK_BOOL ? 1 : fieldtype->size * 8;
                if(val < 0 || val > maxsize) {
                    errort(tok, "invalid bitfield size:%d for %s", val,
                        fieldtype->to_string());
                    return;
                }
                if(val == 0 && name != nullptr) {
                    errort(tok, "zero-width bitfield needs to be unnamed");
                    return;
                }
                fieldtype->bitsize = val;
            }
            else {
                fieldtype->bitsize = -1;
            }
            // finish struct_declarator
            fields.push_back(make_pair(name, fieldtype));

            if(pp->next(',')) continue;
            if(pp->next('}')) {
                warnt(tok, "no semicolon at end of struct or union");
                return;
            }
            if(!pp->next(';')) {
                errort(tok, "expected ';'");
                return;
            }
            break;
        }
    }
    if(!pp->next('}')) {
        parser_error("expected '}'");
    }
}

/*
enum_spec :	'enum_{' enum_spec_tail | 'enum_ident_{' enum_spec_tail | 'enum_ident'
enum_spec_tail :	enum_list 'opt:,' '}'

enum_list :	enumerator enum_list_tail
enum_list_tail :	',' enumerator enum_list_tail | 'empty'

enumerator :	'ident' enumerator_tail
enumerator_tail :	'=' const_expr | 'empty'
*/
Type* Parser::read_enum_spec() {
    TokenPtr tok = pp->get_token();
    char* tag;
    if(tok->kind == TIDENT) {
        tag = tok->to_string();
        auto iter = tags.find(tag);
        if(iter != tags.end()) {
            Type* type = iter->second;
            if(type->kind != TK_ENUM) {
                errort(tok, "'%s' defined as wrong kind of tag", tag);
                return nullptr;
            }
        }
        else {
            tags[tag] = type_enum;
        }
    }
    else {
        if(tok->kind != '{') {
            errort(tok, "expected '{'");
            return nullptr;
        }
        pp->unget_token(tok);
    }

    if(!pp->next('{'))
        return type_int;;
    
    int val = 0;
    while(true) {
        TokenPtr tok = pp->get_token();
        if(tok->kind == '}') break;
        if(tok->kind != TIDENT) {
            errort(tok, "expected identifier");
            return type_int;
        }
        char* name = tok->to_string();

        if(pp->next('=')) {
            val = read_const_expr()->eval_int();
        }
        NodePtr intval = make_int_node(tok, type_int, val++);
        scope->add(name, intval);
        if(pp->next(',')) continue;
        if(pp->next('}')) break;

        parser_error("expected ',' or '}'");
        return type_int;
    }

    return type_int;
}

/*
alignment_spec	:	'_Alignas' '(' alignment_spec_tail
alignment_spec_tail: type_name ')' | const_expr ')'
*/
int Parser::read_alignas() {
    if(!pp->next('(')) {
        parser_error("expected '('");
        return 0;
    }
    int align = 0;
    if(is_type_name(pp->peek_token())) {
        align = read_type_name()->align;
    }
    else {
        align = read_const_expr()->eval_int();
    }
    if(!pp->next(')')) {
        parser_error("expected ')'");
        return 0;
    }
    return align;
}

/*
typeof : 'typeof' '(' 'expr' ')' | 'typeof' '(' type_name ')'
*/
Type* Parser::read_typeof() {
    if(!pp->next('(')) {
        parser_error("expected '('");
        return 0;
    }
    Type* type = nullptr;
    if(is_type_name(pp->peek_token())) {
        type = read_type_name();
    }
    else {
        type = read_expr()->type;
    }
    if(!pp->next(')')) {
        parser_error("expected ')'");
        return 0;
    }
    return type;
}

/*
declarator :	pointer direct_declarator | direct_declarator

direct_declarator :	'ident' direct_declarator_tail
					|	'(' declarator ')' direct_declarator_tail

direct_declarator_tail :	'[' array_size ']' direct_declarator_tail
						|	'(' param_list ')' direct_declarator_tail
						|	'empty'

abstract_declarator : pointer abstract_declarator_tail | direct_abstract_declarator
abstract_declarator_tail : direct_abstract_declarator | 'empty'

direct_abstract_declarator : '(' inside_the_paren ')' direct_abstract_declarator_tail
						   | '[' array_size ']' direct_abstract_declarator_tail
direct_abstract_declarator_tail : '(' param_type_list  'opt:,...' ')' direct_abstract_declarator_tail
								| '[' array_size ']' direct_abstract_declarator_tail
								| 'empty'
inside_the_paren : abstract_declarator | param_type_list 'opt:,...' | 'empty'
*/
// declarator and abstract declarator are implemented together
// params used in function define, Used to record parameter variables
Type* Parser::read_declarator(char** name, Type* basetype, vector<NodePtr>* params, 
    int declarator_kind) {
    if(pp->next('(')) {
        // to param_type_list
        if(is_type_name(pp->peek_token())) {
            return read_param_list(basetype, params);
        }
        // to declarator or abstract_declarator
        // Now we can't confirm the type is pointer to function or array. 
        // Type is determined by the following tokens. For example,
        // int (*) [3] is pointer to an array of three ints
        // int (*) (int) is pointer to function with no an int parameter
        // returning an int
        // So, first pass an hole_type to read_declarator, get a type with hole_type,
        // then read the following, get the real type, and then fill in the hole_type
        Type* hole_type = make_null_type();
        Type* r = read_declarator(name, hole_type, params, declarator_kind);
        if(!pp->next(')')) {
            parser_error("expected ')");
            return nullptr;
        }
        Type* real_type = read_declarator_tail(basetype, params);
        fill_in_null_type(r, real_type);
        basetype->copy_aux(r); // copy type-qualifier
        return r;
    }
    auto skip_type_qualifier = [&](){ 
        while(pp->next(KW_CONST) || pp->next(KW_RESTRICT) || pp->next(KW_VOLATILE)
            || pp->next(KW_ATOMIC)); 
    };
    if(pp->next('*')) {
        skip_type_qualifier(); // Temporarily ignore type qualifier
        Type* r = read_declarator(name, make_ptr_type(basetype), params, declarator_kind);
        basetype->copy_aux(r); // copy type-qualifier
        return r;
    }
    TokenPtr tok = pp->get_token();
    if(tok->kind == TIDENT) {
        if(declarator_kind == DK_ABSTRACT) {
            errort(tok, "identifier is not expected");
            return nullptr;
        }
        *name = tok->to_string();
        Type* r = read_declarator_tail(basetype, params);
        basetype->copy_aux(r); // copy type-qualifier
        return r;
    }
    if(tok->kind == DK_CONCRETE) {
        errort(tok, "expected identifier");
        return nullptr;
    }
    pp->unget_token(tok);
    Type* r = read_declarator_tail(basetype, params);
    basetype->copy_aux(r); // copy type-qualifier
    return r;
}

Type* Parser::read_declarator_tail(Type* basetype, vector<NodePtr>* params) {
    if(pp->next('(')) {
        return read_param_list(basetype, params);
    }
    if(pp->next('[')) {
        return read_array_size(basetype);
    }
    return basetype;
}

// param_list | param_type_list
Type* Parser::read_param_list(Type* return_type, vector<NodePtr>* params) {
    if (return_type->kind == TK_FUNC) {
        parser_error("function returning a function");
        return nullptr;
    }
    if (return_type->kind == TK_ARRAY){
        parser_error("function returning an array");
        return nullptr;
    }

    TokenPtr tok = pp->get_token();
    // C11 6.7.6.3p10: The special case of an unnamed parameter of type void as the 
    // only item in the list specifies that the function has no parameters.
    if(tok->kind == KW_VOID && pp->next(')')) {
        return make_func_type(return_type, vector<Type*>(), false, false);
    }
    if(tok->kind == ')') {
        return make_func_type(return_type, vector<Type*>(), true, true);
    }
    if(tok->kind == P_ELLIPSIS) {
        errort(tok, "requires a named argument before ‘...’");
        return nullptr;
    }
    pp->unget_token(tok);

    // ANSI-style
    if(is_type_name(tok)) {
        bool has_var_param = false;
        vector<Type*> param_types;
        read_declarator_params(params, param_types, has_var_param);
        return make_func_type(return_type, param_types, has_var_param, false);
    }

    if(params == nullptr) {
        errort(tok, "invalid function declaration");
    }

    // K&R-style or invalid-style
    vector<Type*> param_types;
    read_declarator_params_oldstyle(params, param_types);
    return make_func_type(return_type, param_types, false, true);
} 

// Reads an ANSI-style prototyped function parameter list.
void Parser::read_declarator_params(vector<NodePtr>* params, vector<Type*>& param_types,
    bool& has_var_param) {
    bool typeonly = (params == nullptr);
    while(true) {
        TokenPtr tok = pp->peek_token();
        if(pp->next(P_ELLIPSIS)) {
            if(!pp->next(')')) {
                errort(tok, "expected ')'");
                return;
            }
            has_var_param = true;
            return;
        }
        if(!is_type_name(tok)) {
            errort(tok, "type expected");
            return;
        }

        char* name;
        Type* type = read_decl_spec();
        type = read_declarator(&name, type, nullptr, typeonly ? DK_OPTIONAL : DK_CONCRETE);
        // C11 6.7.6.3p7: A declaration of a parameter as ‘‘array of type’’ shall be adjusted to 
        // ‘‘qualified pointer to type’’
        if(type->kind == TK_ARRAY) {
            type = make_ptr_type(dynamic_cast<ArrayType*>(type)->elem_type);
        }
        // A declaration of a parameter as ‘‘function returning type’’ shall be adjusted to 
        // ‘‘pointer to function returning type’’
        else if(type->kind == TK_FUNC) {
            type = make_ptr_type(type);
        }
        if(type->kind == TK_VOID) {
            errort(tok, "parameter '%' has incomplete type", name);
            return;
        }
        param_types.push_back(type);
        if(!typeonly) {
            params->push_back(make_localvar_node(tok, type, name, scope));
        }

        if(pp->next(')')) return;
        if(!pp->next(',')) {
            parser_error("expected ','");
            return;
        }
    }
}

// Reads a K&R-style un-prototyped function parameter list.
void Parser::read_declarator_params_oldstyle(vector<NodePtr>* params, vector<Type*>& param_types) {
    while(true) {
        TokenPtr tok = pp->get_token();
        if(tok->kind != TIDENT) {
            errort(tok, "invalid function declaration");
            return;
        }
        params->push_back(make_localvar_node(tok, type_int, tok->to_string(), scope));
        param_types.push_back(type_int);
        if(pp->next(')')) return;
        if(!pp->next(',')) {
            parser_error("expected ','");
            return;
        }
    }
}

Type* Parser::read_array_size(Type* basetype) {
    int length;
    if(pp->next(']')) {
        length = -1;
    }
    else {
        if(pp->next('*'))
            length = -1;
        else
            length = read_const_expr()->eval_int();
        if(!pp->next(']')) {
            parser_error("expected ']'");
            return nullptr;
        }
    }
    TokenPtr tok = pp->peek_token();
    Type* type = read_declarator_tail(basetype, nullptr);
    if(type->kind == TK_FUNC) {
        errort(tok, "array of functions");
        return nullptr;
    }
    return make_array_type(type, length);
}

/*
type_name :	spec_qual_list type_name_tail
type_name_tail : abstract_declarator | 'empty'
*/
Type* Parser::read_type_name() {
    Type* basetype = read_decl_spec();
    return read_declarator(nullptr, basetype, nullptr, DK_ABSTRACT);
}

/*
initializer :	assign_expr | '{' initializer_list 'opt:,' '}'

initializer_list :	initializer initializer_list_tail 
				 | designation initializer initializer_list_tail | 'empty'
initializer_list_tail :	',' initializer_list

designation : designator_list '='
designator_list : designator designator_list_tail
designator_list_tail : designator designator_list_tail | 'empty'
designator : '[' const_expr ']' | '.' 'ident'
*/
void Parser::read_initializer(Type* type, vector<NodePtr>& init_list) {
    if(type->is_string_type() || pp->peek_token()->kind == '{') {
        read_initializer_list(init_list, type, 0);

        auto cmp = [&](const pair<int, int>& x, const pair<int, int>& y) {
            return (x.first == y.first ? (x.second < y.second) : x.first < y.first);
        };
        map<pair<int, int>, shared_ptr<InitNode>, 
            bool(*)(const pair<int, int>&, const pair<int, int>&)> init_map(cmp);
        for(int i = 0; i < init_list.size(); ++i) {
            assert(init_list[i]->kind == NK_INIT);
            shared_ptr<InitNode> init = dynamic_pointer_cast<InitNode>(init_list[i]);
            init_map[make_pair(init->offset, init->type->bitoff)] = init;
        }
        init_list.clear();
        for(auto init:init_map) {
            init_list.push_back(init.second);
        }
    }
    else {
        NodePtr init =convert(read_assign_expr());
        if(init->type->is_arith_type() && init->type->kind != type->kind)
            init = convert(init, type);
        init_list.push_back(make_init_node(init->first_token, type, init, 0));
    }
}

void Parser::read_initializer_list(std::vector<NodePtr>& init_list, 
    Type* type, int offset) {
    TokenPtr tok = pp->get_token();
    if(type->is_string_type()) {
        if(tok->kind == TSTRING) {
            assign_string(init_list, type, tok, offset);
            return;
        }
        if(tok->kind == '{' && pp->peek_token()->kind == TSTRING) {
            tok = pp->get_token();
            assign_string(init_list, type, tok, offset);
            if(!pp->next('}')) {
                parser_error("expected '}'");
            }
            return;
        }
    }
    
    // pp->unget_token(tok);
    if(type->kind == TK_STRUCT || type->kind == TK_UNION) {
        read_struct_initializer_list(init_list, type, offset);
    }
    else if(type->kind == TK_ARRAY) {
        read_array_initializer_list(init_list, type, offset);
    }
    else {
        Type *arrtype = make_array_type(type, 1);
        read_array_initializer_list(init_list, arrtype, offset);
    }
}

void Parser::assign_string(std::vector<NodePtr>& init_list, Type* type, 
    TokenPtr tok, int offset) {
    ArrayType* arrtype = dynamic_cast<ArrayType*>(type);
    shared_ptr<String> str = dynamic_pointer_cast<String>(tok);
    if(arrtype->length == -1) {
        arrtype->length = arrtype->size = strlen(str->value) + 1;
    }
    int i = 0;
    char* p = str->value;
    for(; i < arrtype->length && *p; ++i) {
        init_list.push_back(make_init_node(tok, type_char, 
            make_int_node(tok, type_char, *p++), offset + i));
    }
    if(*p) {
        warnt(tok, "initializer-string for array of chars is too long");
    }
    for(; i < arrtype->length; ++i) {
        init_list.push_back(make_init_node(tok, type_char, 
            make_int_node(tok, type_char, 0), offset + i));
    } 
}

void Parser::read_struct_initializer_list(vector<NodePtr>& init_list, Type* type, 
    int offset) {
    StructType* struct_type = dynamic_cast<StructType*>(type);
    size_t i = 0;
    while(true) {
        TokenPtr tok = pp->get_token();
        int offset_offset = 0;
        if(tok->kind == '}') {
            return;
        }
        Type* field_type = struct_type;
        if(tok->kind == '.') {
            bool change_i = true;
            while(true) {
                if(tok->kind == '.') {
                    if(field_type->kind != TK_STRUCT) {
                        errort(tok, "field name in non-struct initialzer");
                        return;
                    }
                    tok = pp->get_token();
                    if(tok->kind != TIDENT) {
                        errort(tok, "expected identifier");
                    }
                    char* name = tok->to_string();
                    int idx;
                    field_type = get_struct_field_type(
                            dynamic_cast<StructType*>(field_type)->fields, name, &idx);
                    if(change_i) {
                        i = idx + 1;
                        change_i = false;
                    }
                    offset_offset += field_type->offset;
                    if(!field_type) {
                        errort(tok, "unknown field ‘%s’ specified in initializer", name);
                        return;
                    }
                }
                else if(tok->kind == '[') {
                    if(field_type->kind != TK_ARRAY) {
                        errort(tok, "array index in non-array initializer");
                        return;
                    }
                    ArrayType* arrtype = dynamic_cast<ArrayType*>(field_type);
                    tok = pp->peek_token();
                    int idx = read_const_expr()->eval_int();
                    if(idx < 0 || (arrtype->length > 0 && idx >= arrtype->length)) {
                        errort(tok, "array index in initializer exceeds array bounds");
                        return;  
                    }
                    field_type = arrtype->elem_type;
                    offset_offset += field_type->size * idx;
                    if(!pp->next(']')) {
                        parser_error("expected ']");
                        return;
                    }
                }
                else {
                    pp->unget_token(tok);
                    break;
                }
                tok = pp->get_token();
            }
        }
        else {
            pp->unget_token(tok);
            if(i == struct_type->fields.size()) {
                errort(tok, "excess elements in struct or union initializer");
                return;
            }
            field_type = struct_type->fields[i++].second;
            offset_offset = field_type->offset;
        }

        read_designator_list_tail(init_list, field_type, offset + offset_offset);

        if(pp->next('}')) return;
        if(pp->next(',')) {
            if(pp->next('}')) return;
        }
        else {
            parser_error("expected ','");
            return;
        }
    }
}

void Parser::read_array_initializer_list(vector<NodePtr>& init_list, Type* type, 
    int offset) {
    ArrayType* array_type = dynamic_cast<ArrayType*>(type);
    int i = 0;
    while(true) {
        TokenPtr tok = pp->get_token();
        int offset_offset = 0;
        if(tok->kind == '}') {
            break;
        }
        Type* field_type = array_type;
        if(tok->kind == '[') {
            bool change_i = true;
            while(true) {
                if(tok->kind == '.') {
                    if(field_type->kind != TK_STRUCT) {
                        errort(tok, "field name in non-struct initialzer");
                        return;
                    }
                    tok = pp->get_token();
                    if(tok->kind != TIDENT) {
                        errort(tok, "expected identifier");
                    }
                    char* name = tok->to_string();
                    field_type = get_struct_field_type(
                            dynamic_cast<StructType*>(field_type)->fields, name, nullptr);
                    offset_offset += field_type->offset;
                    if(!field_type) {
                        errort(tok, "unknown field ‘%s’ specified in initializer", name);
                        return;
                    }
                }
                else if(tok->kind == '[') {
                    if(field_type->kind != TK_ARRAY) {
                        errort(tok, "array index in non-array initializer");
                        return;
                    }
                    ArrayType* arrtype = dynamic_cast<ArrayType*>(field_type);
                    tok = pp->peek_token();
                    int idx = read_const_expr()->eval_int();
                    if(idx < 0 || (arrtype->length > 0 && idx >= arrtype->length)) {
                        errort(tok, "array index in initializer exceeds array bounds");
                        return;  
                    }
                    if(change_i) {
                        i = idx;
                        change_i = false;
                    }
                    field_type = arrtype->elem_type;
                    offset_offset += field_type->size * idx;
                    if(!pp->next(']')) {
                        parser_error("expected ']");
                        return;
                    }
                }
                else {
                    pp->unget_token(tok);
                    break;
                }
                tok = pp->get_token();
            }
        }
        else {
            pp->unget_token(tok);
            if(i == array_type->length) {
                errort(tok, "excess elements in array initializer");
                return;
            }
            field_type = array_type->elem_type;
            offset_offset = field_type->size * i;
            i++;
        }
        read_designator_list_tail(init_list, field_type, offset + offset_offset);

        if(pp->next('}')) break;
        if(pp->next(',')) {
            if(pp->next('}')) break;
        }
        else {
            parser_error("expected ','");
            return;
        }
    }
    if(array_type->length < 0) {
        array_type->length = i;
        array_type->size = array_type->elem_type->size * i;
    }
}

// designator_list_tail
void Parser::read_designator_list_tail(vector<NodePtr>& init_list, Type* type,
    int offset) {
    TokenPtr tok = pp->peek_token();
    pp->next('=');
    if(type->kind == TK_STRUCT || type->kind == TK_UNION || type->kind == TK_ARRAY) {
        return read_initializer_list(init_list, type, offset);
    } 
    else if(pp->next('{')) {
        read_designator_list_tail(init_list, type, offset);
        if(!pp->next('}')) {
            parser_error("expected '}'");
            return;
        }
    }
    else {
        NodePtr value = convert(read_assign_expr());
        if(!is_assignable(type, value->type)) {
            errort(tok, "incompatible types when initializing type ‘%s’ using type ‘%s’",
                type->to_string(), value->type->to_string());
            return;
        }
        init_list.push_back(make_init_node(tok, type, value, offset));
    }
}

//---------------------------- Statements ----------------------------

NodePtr Parser::read_stmt() {
    TokenPtr tok = pp->get_token();
    switch(tok->kind) {
    case TIDENT: {
        if(pp->next(':')) {
            return read_label_stmt(tok);
        }
        break;
    }
    case '{': return read_compound_stmt(tok);
    case KW_IF:       return read_if_stmt(tok);
    case KW_SWITCH:   return read_switch_stmt(tok);
    case KW_CASE:     return read_case_stmt(tok);
    case KW_DEFAULT:  return read_default_stmt(tok);
    case KW_WHILE:    return read_while_stmt(tok);
    case KW_DO:       return read_do_stmt(tok);
    case KW_FOR:      return read_for_stmt(tok);
    case KW_GOTO:     return read_goto_stmt(tok);
    case KW_CONTINUE: return read_continue_stmt(tok);
    case KW_BREAK:    return read_break_stmt(tok);
    case KW_RETURN:   return read_return_stmt(tok);
    }
    pp->unget_token(tok);
    return read_expr_stmt();
}

NodePtr Parser::read_label_stmt(TokenPtr tok) {
    char* label = tok->to_string();
    if(labels.find(label) != labels.end()) {
        errort(tok, "duplicate label %s", label);
        return error_node;
    }
    NodePtr l = make_label_node(tok, label, make_label());
    labels[label] = l;
    return read_label_stmt_tail(l);
}

NodePtr Parser::read_case_stmt(TokenPtr tok) {
    char* label = make_label();
    int begin = read_const_expr()->eval_int(), end;
    if(pp->next(P_ELLIPSIS)) {
        end = read_const_expr()->eval_int();
        if(begin > end) {
            errort(tok, "case range is invalid: from %d to %d", begin, end);
            return error_node;
        }
    }
    else {
        end = begin;
    }
    if(!pp->next(':')) {
        parser_error("expected ':'");
        return error_node;
    }
    vector<CaseTuple>& cases = scope->get_cases();
    cases.push_back(CaseTuple{begin, end, label});
    // // C11 6.8.4.2p3: No two case constant expressions have the same value.
    CaseTuple x = cases.back();
    for(size_t i = 0; i < cases.size()-1; ++i) {
        CaseTuple y = cases[i];
        if(x.end < y.begin || y.end < x.begin) 
            continue;
        errort(tok, "duplicate case value: %d ... %d", x.begin, x.end);
    }

    return read_label_stmt_tail(make_label_node(tok, label, label));
}

NodePtr Parser::read_default_stmt(TokenPtr tok) {
    char* default_label = make_label();
    if(scope->get_default_label()) {
        errort(tok, "duplicate default");
        return error_node;
    }
    if(!pp->next(':')) {
        parser_error("expected ':'");
        return error_node;
    }
    scope->set_switch_default(default_label);
    return read_label_stmt_tail(make_label_node(tok, default_label, default_label));
}

NodePtr Parser::read_label_stmt_tail(NodePtr node) {
    vector<NodePtr> list;
    list.push_back(node);
    NodePtr stmt = read_stmt();
    if(stmt) {
        list.push_back(stmt);
    }
    return make_compound_stmt_node(node->first_token, list);
}

/*
compound_stmt :	'{' block_item_list '}'

block_item_list : block_item block_item_list | 'empty'

block_item : decl | stmt
*/
NodePtr Parser::read_compound_stmt(TokenPtr tok) {
    scope->in();
    vector<NodePtr> list;
    while(true) {
        if(pp->next('}')) break;
        read_block_item(list);
    }
    scope->out();
    if(list.empty()) return nullptr;
    return make_compound_stmt_node(tok, list);
}

void Parser::read_block_item(vector<NodePtr>& list) {
    TokenPtr tok = pp->peek_token();
    if(tok->kind == TEOF) {
        errort(tok, "expected declaration or statement at end of input");
        return;
    }
    if(is_type_name(tok) || tok->kind == KW_STATIC_ASSERT) {
        read_decl(list, false);
    }
    else {
        NodePtr stmt = read_stmt();
        if(stmt) list.push_back(stmt);
    }
}

// expression-statement
NodePtr Parser::read_expr_stmt() {
    if(pp->next(';')) {
        return nullptr;
    }
    NodePtr expr = read_expr();
    if(!pp->next(';')) {
        parser_error("expected ';'");
    }
    return expr;
}

// boolean-expr
NodePtr Parser::read_boolean_expr() {
    NodePtr cond = convert(read_expr());
    if(!cond->type->is_scalar_type()) {
        errort(cond->first_token, "scalar type is required");
        return error_node;
    }
    if(cond->type->is_float_type()) {
        cond = convert(cond, type_bool);
    }
    return cond;
}

// if-statement
NodePtr Parser::read_if_stmt(TokenPtr tok) {
    if(!pp->next('(')) {
        parser_error("expected '('");
        return error_node;
    }
    NodePtr cond = read_boolean_expr();
    if(!pp->next(')')) {
        parser_error("expected ')'");
        return error_node;
    }
    NodePtr then = read_stmt();
    if(!pp->next(KW_ELSE)) {
        return make_if_node(tok, cond, then, nullptr);
    }
    NodePtr els = read_stmt();
    return make_if_node(tok, cond, then, els);
}

// switch-statement
NodePtr Parser::read_switch_stmt(TokenPtr tok) {
    if(!pp->next('(')) {
        parser_error("expected '('");
        return error_node;
    }
    NodePtr expr = convert(read_expr());
    if(!expr->type->is_int_type()) {
        errort(expr->first_token, "switch quantity not an integer");
        return error_node;
    }
    if(!pp->next(')')) {
        parser_error("expected ')'");
        return error_node;
    }

    char* end = make_label();
    scope->in_switch(end);

    NodePtr body = read_stmt();
    vector<NodePtr> list;

    char* name = make_tmpname();
    NodePtr var = make_localvar_node(expr->first_token, expr->type, name, scope);
    list.push_back(make_binop(tok, '=', var, expr));
    vector<CaseTuple>& cases = scope->get_cases();
    for(size_t i = 0; i < cases.size(); ++i) {
        list.push_back(make_switch_jump(var, cases[i]));
    }
    char* default_label = scope->get_default_label();
    default_label = default_label ? default_label : end;
    list.push_back(make_jump_node(tok, default_label, default_label));
    if(body) {
        list.push_back(body);
    }
    list.push_back(make_label_node(tok, end, end));

    scope->out_switch();

    return make_compound_stmt_node(tok, list);
}

NodePtr Parser::make_switch_jump(NodePtr var, const CaseTuple& c) {
    NodePtr cond;
    TokenPtr tok = var->first_token;
    if(c.begin == c.end) {
        cond = make_binop(tok, P_EQ, var, make_int_node(tok, type_int, c.begin));
    }
    else {
        NodePtr cond1 = make_binop(tok, P_LE, make_int_node(tok, type_int, c.begin), var);
        NodePtr cond2 = make_binop(tok, P_LE, var, make_int_node(tok, type_int, c.end));
        cond = make_binop(tok, P_LOGAND, cond1, cond2);
    }
    return make_if_node(tok, cond, make_jump_node(tok, c.label, c.label), nullptr);
}

// while-statement
NodePtr Parser::read_while_stmt(TokenPtr tok) {
    if(!pp->next('(')) {
        parser_error("expected '('");
        return error_node;
    }
    char* begin = make_label();
    char* end = make_label();

    scope->in_loop(begin, end);

    NodePtr cond = read_boolean_expr();
    if(!pp->next(')')) {
        parser_error("expected ')'");
        return error_node;
    }
    NodePtr body = read_stmt();

    scope->out_loop();

    vector<NodePtr> list;
    list.push_back(make_label_node(tok, begin, begin));
    list.push_back(make_if_node(tok, cond, body, make_jump_node(tok, end, end)));
    list.push_back(make_jump_node(tok, begin, begin));
    list.push_back(make_label_node(tok, end, end));

    return make_compound_stmt_node(tok, list);
}

// do-statement
NodePtr Parser::read_do_stmt(TokenPtr tok) {
    char* begin = make_label();
    char* end = make_label();

    scope->in_loop(begin, end);

    NodePtr body = read_stmt();
    if(!pp->next(KW_WHILE)) {
        parser_error("expected 'while'");
        return error_node;
    }
    if(!pp->next('(')) {
        parser_error("expected '('");
        return error_node;
    }
    NodePtr cond = read_boolean_expr();
    if(!pp->next(')')) {
        parser_error("expected ')'");
        return error_node;
    }
    if(!pp->next(';')) {
        parser_error("expected ';'");
        return error_node;
    }

    scope->out_loop();

    vector<NodePtr> list;
    list.push_back(make_label_node(tok, begin, begin));
    if(body) 
        list.push_back(body);
    list.push_back(make_if_node(tok, cond, make_jump_node(tok, begin, begin), nullptr));
    list.push_back(make_label_node(tok, end, end));

    return make_compound_stmt_node(tok, list);
}

// for-statement
NodePtr Parser::read_for_stmt(TokenPtr tok) {
    if(!pp->next('(')) {
        parser_error("expected '('");
        return error_node;
    }
    char* begin = make_label();
    char* next = make_label();
    char* end = make_label();

    scope->in_loop(next, end);

    NodePtr init = read_for_init();
    NodePtr cond = nullptr;
    if(!pp->next(';')) {
        cond = read_boolean_expr();
        if(!pp->next(';')) {
            parser_error("expected ';'");
            return error_node;
        }
    }
    NodePtr step = nullptr;
    if(!pp->next(')')) {
        step = read_expr();
        if(!pp->next(')')) {
            parser_error("expected ')'");
            return error_node;
        }
    }   

    NodePtr body = read_stmt();

    scope->out_loop();

    vector<NodePtr> list;
    if(init) {
        list.push_back(init);
    }
    list.push_back(make_label_node(tok, begin, begin));
    if(cond) {
        list.push_back(make_if_node(tok, cond, nullptr, make_jump_node(tok, end, end)));
    }
    if(body) {
        list.push_back(body);
    }
    list.push_back(make_label_node(tok, next, next));
    if(step) {
        list.push_back(step);
    }
    list.push_back(make_jump_node(tok, begin, begin));
    list.push_back(make_label_node(tok, end, end));

    return make_compound_stmt_node(tok, list);
}

NodePtr Parser::read_for_init() {
    TokenPtr tok = pp->peek_token();
    vector<NodePtr> list;
    if(tok->kind == TEOF) {
        errort(tok, "expected declaration or statement at end of input");
        return error_node;
    }
    if(is_type_name(tok) || tok->kind == KW_STATIC_ASSERT) {
        read_decl(list, false);
    }
    else {
        NodePtr expr = read_expr_stmt();
        if(expr) list.push_back(expr);
        else return nullptr;
    }
    return make_compound_stmt_node(tok, list);
}

// goto-statement
NodePtr Parser::read_goto_stmt(TokenPtr tok) {
    if(pp->next('*')) {
        TokenPtr t = pp->peek_token();
        NodePtr expr = read_cast_expr();
        if(expr->type->kind != TK_PTR) {
            errort(t, "pointer expected for computed goto");
            return error_node;
        }
        return make_unary_oper_node(tok, NK_COMPUTED_GOTO, nullptr, expr);
    }
    TokenPtr ident = pp->get_token();
    if(ident->kind != TIDENT) {
        errort(ident, "expected identifire for goto");
        return error_node;
    }
    if(!pp->next(';')) {
        parser_error("expected ';'");
        return error_node;
    }
    NodePtr node = make_jump_node(ident, ident->to_string());
    gotos.push_back(node);
    return node;
}

// continue-statement
NodePtr Parser::read_continue_stmt(TokenPtr tok) {
    if(!pp->next(';')) {
        parser_error("expected ';'");
        return error_node;
    }
    if(!scope->is_in_loop()) {
        errort(tok, "continue statement not within a loop");
        return error_node;
    }
    char* continue_label = scope->get_continue_label();
    return make_jump_node(tok, continue_label, continue_label);
}

// break-statement
NodePtr Parser::read_break_stmt(TokenPtr tok) {
    if(!pp->next(';')) {
        parser_error("expected ';'");
        return error_node;
    }
    if(!scope->is_in_loop()) {
        errort(tok, "break statement not within loop or switch");
        return error_node;
    }
    char* break_label = scope->get_break_label();
    return make_jump_node(tok, break_label, break_label);
}

// return-statement
NodePtr Parser::read_return_stmt(TokenPtr tok) {
    NodePtr return_val = read_expr_stmt();
    if(return_val) {
        return make_return_node(tok, convert(return_val, scope->get_current_func_type()->return_type));
    }
    return make_return_node(tok, nullptr);
}    

//----------------------- External Definitions -------------------------

/*
extern_decl :	func_def | decl
*/
void Parser::read_extern_decl() {
    while(true) {
        if(pp->peek_token()->kind == TEOF)
            return;
        TokenPtr tok = pp->peek_token();
        if(tok->kind == KW_STATIC_ASSERT) {
            tok = pp->get_token();
            read_static_assert(tok);
            continue;
        }
        Type* basetype = read_decl_spec_opt();
        if(pp->next(';')) continue;

        // regard as function define
        scope->in();
        labels.clear();
        gotos.clear();

        char* name;
        vector<NodePtr> params;
        Type* type = read_declarator(&name, basetype, &params, DK_CONCRETE);
        tok = pp->peek_token();
        bool is_func = is_type_name(tok) || tok->kind == '{';  
        // function define      
        if(is_func) {
            FuncType* func_type = dynamic_cast<FuncType*>(type);
            if(func_type->has_var_param && func_type->param_types.size() == 0) {
                func_type->has_var_param = false;
            }
            if(func_type->is_old_style) {
                read_oldstyle_param_type(func_type, params);
            }

            make_globalvar_node(tok, func_type, name, scope);
            if(!pp->next('{')) {
                parser_error("expected '{'");
                return;
            }
            
            NodePtr func = read_func_body(tok, func_type, name, params);

            // Normalize the label-name of gotos and labels
            for(auto g:gotos) {
                assert(g->kind == NK_JUMP);
                shared_ptr<JumpNode> src = dynamic_pointer_cast<JumpNode>(g);
                char* label = src->origin_label;
                auto iter = labels.find(label);
                if(iter == labels.end()) {
                    errort(src->first_token, "label ‘%s’ used but not defined", label);
                    return;
                }
                shared_ptr<LabelNode> dst = dynamic_pointer_cast<LabelNode>(iter->second);
                src->normal_label = dst->normal_label;
            }

            scope->out();
            toplevers.push_back(func);
        }
        // declaration
        else {
            scope->out();
            while(true) {
                if(type->storage_class == KW_TYPEDEF) {
                    make_typedef_node(tok, type, name, scope);
                }
                else {
                    if(type->kind == TK_VOID) {
                        errort(tok, "type void is not allowed");
                        return;
                    }
                    NodePtr var = make_globalvar_node(tok, type, name, scope);
                    shared_ptr<DeclNode> decl_node = make_decl_node(tok, type, var);
                    if(pp->next('=')) {
                        read_initializer(type, decl_node->init_list);
                        toplevers.push_back(decl_node);
                    }
                    else if(type->storage_class != KW_EXTERN && type->kind != TK_FUNC) {
                        toplevers.push_back(decl_node);
                    }
                }
                if(pp->next(';')) break;
                if(!pp->next(',')) {
                    parser_error("';' or ',' are expected");
                    return;
                }
                char* name = nullptr;
                type = read_declarator(&name, copy_incomplete_type(basetype), nullptr, DK_CONCRETE);
            }
        }
    }
}

/*
func_def : decl_spec declarator func_def_tail
func_def_tail : decl_list compound_stmt | compound_stmt

decl_list : decl decl_list_tail
decl_list_tail : decl decl_list_tail | 'empty'
*/
// ------------- used in old version ---------------
// NodePtr Parser::read_func_def() {
//     TokenPtr tok = pp->peek_token();
//     Type* basetype = read_decl_spec_opt();

//     scope->in();
//     labels.clear();
//     gotos.clear();

//     char* name;
//     vector<NodePtr> params;
//     Type* type = read_declarator(&name, basetype, &params, DK_CONCRETE);
//     FuncType* func_type = dynamic_cast<FuncType*>(type);
//     if(func_type->has_var_param && func_type->param_types.size() == 0) {
//         func_type->has_var_param = false;
//     }
//     if(func_type->is_old_style) {
//         read_oldstyle_param_type(func_type, params);
//     }

//     make_globalvar_node(tok, func_type, name, scope);
//     if(!pp->next('{')) {
//         parser_error("expected '{'");
//         return error_node;
//     }
    
//     NodePtr func = read_func_body(tok, func_type, name, params);

//     // Normalize the label-name of gotos and labels
//     for(auto g:gotos) {
//         assert(g->kind == NK_JUMP);
//         shared_ptr<JumpNode> src = dynamic_pointer_cast<JumpNode>(g);
//         char* label = src->origin_label;
//         auto iter = labels.find(label);
//         if(iter == labels.end()) {
//             errort(src->first_token, "label ‘%s’ used but not defined", label);
//             return error_node;
//         }
//         shared_ptr<LabelNode> dst = dynamic_pointer_cast<LabelNode>(iter->second);
//         src->normal_label = dst->normal_label;
//     }

//     scope->out();
//     return func;
// }

// ------------- used in old version ---------------
// bool Parser::is_func_def() {
//     pp->open_undo_mode();
//     Type* basetype = read_decl_spec_opt();
//     char* name;
//     vector<NodePtr> params;
//     read_declarator(&name, basetype, &params, DK_OPTIONAL);
//     TokenPtr tok = pp->get_token();
//     bool is_func = is_type_name(tok) || tok->kind == '{';
//     pp->undo();
//     return is_func;
// }

void Parser::read_oldstyle_param_type(FuncType* func_type, vector<NodePtr>& params) {
    vector<NodePtr> list;
    scope->in();
    while(true) {
        TokenPtr tok = pp->peek_token();
        if(tok->kind == '{') break;
        if(!is_type_name(tok)) {
            errort(tok, "K&R-style declarator expected");
            return;
        }
        read_decl(list, false);
    }
    scope->out();

    for(auto item:list) {
        assert(item->kind == NK_DECL);
        shared_ptr<DeclNode> decl = dynamic_pointer_cast<DeclNode>(item);
        assert(decl->var->kind == NK_LOCAL_VAR);
        shared_ptr<LocalVarNode> var = dynamic_pointer_cast<LocalVarNode>(decl->var);
        for(size_t i = 0; i < params.size(); ++i) {
            assert(params[i]->kind == NK_LOCAL_VAR);
            shared_ptr<LocalVarNode> param_var = dynamic_pointer_cast<LocalVarNode>(params[i]);
            if(strcmp(param_var->var_name, var->var_name)) 
                continue;
            params[i]->type = var->type;
            func_type->param_types[i] = var->type;
            goto next;
        }
        errort(item->first_token, 
            "declaration for parameter ‘%s’ but no such parameter", var->var_name);
        next:;
    }
}

NodePtr Parser::read_func_body(TokenPtr tok, FuncType* func_type, char* fname, vector<NodePtr> params) {
    scope->in(func_type);
    scope->clear_local_var();

    NodePtr func_name =  make_string_node(tok, fname, ENC_NONE);
    scope->add("__func__", func_name);
    scope->add("__FUNCTION__", func_name);
    NodePtr body = read_compound_stmt(tok);
    NodePtr func = make_func_def_node(tok, func_type, fname, params, body, scope);

    scope->out();

    return func;
}

// parser's public function
vector<NodePtr>& Parser::get_ast() {
    read_extern_decl();
    return toplevers;
}