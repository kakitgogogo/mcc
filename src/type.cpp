#include <stdlib.h>
#include <assert.h>
#include "utils.h"
#include "buffer.h"
#include "type.h"

// builtin type
Type *type_void = new NumType(TK_VOID, false);
Type *type_bool = new NumType(TK_BOOL, true);
Type *type_char = new NumType(TK_CHAR, false);
Type *type_short = new NumType(TK_SHORT, false);
Type *type_int = new NumType(TK_INT, false);
Type *type_long = new NumType(TK_LONG, false);
Type *type_llong = new NumType(TK_LONG_LONG, false);
Type *type_uchar = new NumType(TK_CHAR, true);
Type *type_ushort = new NumType(TK_SHORT, true);
Type *type_uint = new NumType(TK_INT, true);
Type *type_ulong = new NumType(TK_LONG, true);
Type *type_ullong = new NumType(TK_LONG_LONG, true);
Type *type_float = new NumType(TK_FLOAT, false);
Type *type_double = new NumType(TK_DOUBLE, false);
Type *type_ldouble = new NumType(TK_LONG_DOUBLE, false);
Type *type_enum = new NumType(TK_ENUM, false);

// make type
Type* make_null_type() {
    Type* ty = new Type(TK_NULL, 0, 0);
    return ty;
}

Type* make_number_type(int kind, bool is_unsigned) {
    Type* ty = new NumType(kind, is_unsigned);
    return ty;
}

Type* make_ptr_type(Type* type) {
    Type* ty = new PtrType(type);
    return ty;
}

Type* make_array_type(Type* type, int length) {
    Type* ty = new ArrayType(type, length);
    return ty;
}

Type* make_struct_type(int kind, char* name) {
    assert(kind == TK_STRUCT || kind == TK_UNION);
    Type* ty = new StructType(kind, name);
    return ty;
}

Type* make_func_type(Type* return_type, std::vector<Type*> param_types, bool has_var_param, bool is_old_style) {
    Type* ty = new FuncType(return_type, param_types, has_var_param, is_old_style);
    return ty;
}

bool Type::is_int_type() {
    return (kind >= TK_BOOL && kind <= TK_LONG_LONG);
}

bool Type::is_float_type() {
    return (kind >= TK_FLOAT && kind <= TK_LONG_DOUBLE);
}

// C11 6.2.5p18: Integer and floating types are collectively called arithmetic types.
bool Type::is_arith_type() {
    return (kind >= TK_BOOL && kind <= TK_LONG_DOUBLE);
}

char* Type::to_string() {
    return "";
}

char* NumType::to_string() {
    auto decorate_int = [&](char* type_name){ 
        return format("%s%s", (is_unsigned ? "unsigned " : ""), type_name); 
    };

    switch(kind) {
    case TK_VOID: return "void";
    case TK_BOOL: return "_Bool";
    case TK_CHAR: return decorate_int("char");
    case TK_SHORT: return decorate_int("short");
    case TK_INT:  return decorate_int("int");
    case TK_LONG: return decorate_int("long");
    case TK_LONG_LONG: return decorate_int("long long");
    case TK_FLOAT: return "float";
    case TK_DOUBLE: return "double";
    case TK_LONG_DOUBLE: return "long double";
    case TK_ENUM: return "enum";
    default: error("internal error: NumType::to_string()");
    }
    return "";
}

char* PtrType::to_string() {
    return format("%s*", ptr_type->to_string());
}

char* ArrayType::to_string() {
    return format("%s[%d]", elem_type->to_string(), length);
}

char* StructType::to_string() {
    return format("%s %s", (kind == TK_STRUCT ? "struct" : "union"), name ? name : "(anonymous)");
}

char* FuncType::to_string() {
    Buffer buf;
    buf.write(return_type->to_string());
    buf.write('(');
    for(size_t i = 0; i < param_types.size(); ++i) {
        buf.write(param_types[i]->to_string());
        if(i < param_types.size() - 1)
            buf.write(',');
    }
    if(has_var_param) buf.write(",...");
    buf.write(')');
    buf.write('\0');
    return buf.data();
}

// C11 6.2.7p1: Two types have compatible type if their types are the same
bool Type::is_compatible(Type* type) {
    return (type->kind == kind) && (type->is_unsigned == is_unsigned);
}

bool PtrType::is_compatible(Type* type) {
    if(type->kind != TK_PTR) return false;
    PtrType* ty = dynamic_cast<PtrType*>(type);
    return ptr_type->is_compatible(ty->ptr_type);
}

bool ArrayType::is_compatible(Type* type) {
    if(type->kind != TK_ARRAY) return false;
    ArrayType* ty = dynamic_cast<ArrayType*>(type);
    return (length == ty->length) && (elem_type->is_compatible(ty->elem_type));
}

bool StructType::is_compatible(Type* type) {
    if(type->kind != kind) return false;
    StructType* ty = dynamic_cast<StructType*>(type);
    if(ty->fields.size() != fields.size()) return false;
    auto iter1 = fields.begin();
    auto iter2 = ty->fields.begin();
    for(; iter1 != fields.end(); ++iter1, ++iter2) {
        if(iter1->second->kind == TK_PTR && iter2->second->kind == TK_PTR) 
            continue;
        if(!iter1->second->is_compatible(iter2->second))
            return false;
    }
    return true;
}

bool FuncType::is_compatible(Type* type) {
    if(type->kind != kind) return false;
    FuncType* ty = dynamic_cast<FuncType*>(type);
    if(!return_type->is_compatible(ty->return_type)) {
        return false;
    }
    if(param_types.size() != ty->param_types.size()) {
        return false;
    }
    for(size_t i = 0; i < param_types.size(); ++i) {
        if(!param_types[i]->is_compatible(ty->param_types[i])) {
            return false;
        }
    }
    return true;
}

// C11 6.3.5p21 Arithmetic types and pointer types are collectively called scalar types. 
bool Type::is_scalar_type() {
    return is_arith_type() || kind == TK_PTR; 
}

// Array and structure types are collectively called aggregate types.

// C11 6.3.5p22 An array type of unknown size is an incomplete type.

// C11 6.7.2.1p3 A structure or union shall not contain a member with incomplete or function type (hence,
// a structure shall not contain an instance of itself, but may contain a pointer to an instance
// of itself), except that the last member of a structure with more than one named member
// may have incomplete array type; such a structure (and any union containing, possibly
// recursively, a member that is such a structure) shall not be a member of a structure or an
// element of an array.
