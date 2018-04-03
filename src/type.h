#pragma once

#include <map>
#include <vector>
#include <assert.h>
#include "error.h"

/*
C11 6.2.5 Types
*/

enum TypeKind {
    TK_VOID,
    TK_BOOL,
    TK_CHAR,
    TK_SHORT,
    TK_INT,
    TK_LONG,
    TK_LONG_LONG,
    TK_FLOAT,
    TK_DOUBLE,
    TK_LONG_DOUBLE,

    TK_ENUM,
    TK_PTR,
    TK_ARRAY,
    TK_STRUCT, 
    TK_UNION,
    TK_FUNC,

    TK_NULL,
};

class Type {
public:
    Type(int k): kind(k), is_unsigned(false) {}
    Type(int k, int sz, int a, int isu = false): 
        kind(k), size(sz), align(a), is_unsigned(isu) {}

    virtual ~Type() {}

    virtual char* to_string();

    virtual Type* copy() { 
        Type* type = new Type(kind, size, align, is_unsigned); 
        type->from_copy = true;
        return type;
    }

    Type* copy_aux(Type* type) {
        type->offset = offset;
        type->bitoff = bitoff;
        type->bitsize = bitsize;
        type->storage_class = storage_class;
        type->type_qualifier = type_qualifier;
        type->is_inline = is_inline;
        type->is_noreturn = is_noreturn;
    }

    bool is_int_type();
    bool is_float_type();
    bool is_arith_type();
    bool is_scalar_type();

    virtual bool is_string_type() { return false; }

    virtual bool is_compatible(Type* type);

    bool is_static() { return storage_class == KW_STATIC; }
public:
    int kind;
    int size;
    int align; 
    bool is_unsigned;

    // used in struct's field type
    int offset = 0;
    int bitoff = 0;
    int bitsize = 0;

    // qualifier
    int storage_class = 0;
    std::vector<int> type_qualifier;

    // func-spec
    bool is_inline = false;
    bool is_noreturn = false;

    // to avoid multiple copies
    bool from_copy = false;
};

class NumType: public Type {
public:
    NumType(int k, bool isu): Type(k) {
        assert(k >= TK_VOID && k <= TK_ENUM);
        is_unsigned = isu;
        switch(kind) {
        case TK_VOID: size = align = 0; break;
        case TK_BOOL: size = align = 1; break;
        case TK_CHAR: size = align = 1; break;
        case TK_SHORT: size = align = 2; break;
        case TK_INT: size = align = 4; break;
        case TK_LONG: size = align = 8; break;
        case TK_LONG_LONG: size = align = 8; break;
        case TK_FLOAT: size = align = 4; break;
        case TK_DOUBLE: size = align = 8; break;
        case TK_LONG_DOUBLE: size = align = 8; break;
        case TK_ENUM: size = align = 4; break;
        default: error("internal error: NumType()");
        }
    }

    virtual char* to_string();

    virtual Type* copy() { 
        Type* type = new NumType(kind, is_unsigned); 
        copy_aux(type);
        type->from_copy = true;
        return type;
    }
};

class PtrType: public Type {
public:
    PtrType(Type* ty): Type(TK_PTR, 8, 8), ptr_type(ty) {}
    virtual char* to_string();
    virtual bool is_compatible(Type* type);

    virtual Type* copy() { 
        Type* type = new PtrType(ptr_type); 
        copy_aux(type);
        type->from_copy = true;
        return type;
    }
public:
    Type* ptr_type; // Type of pointing
};

class ArrayType: public Type {
public:
    ArrayType(Type* ty, int len): Type(TK_ARRAY) {
        if(len < 0) // the size of array to be determined
            size = -1;
        else
            size = ty->size * len;
        elem_type = ty;
        length = len;
        align = ty->align;
    }

    bool is_string_type() { return elem_type->kind == TK_CHAR; }

    virtual char* to_string();
    virtual bool is_compatible(Type* type);

    virtual Type* copy() { 
        Type* type = new ArrayType(elem_type, length); 
        copy_aux(type);
        type->from_copy = true;
        return type;
    }
public:
    Type* elem_type; // Type of element
    int length;
};

class StructType: public Type {
public:
    StructType(int kind, char* name = ""): Type(kind), name(name) {
        assert(kind == TK_STRUCT || kind == TK_UNION);
    }
    virtual char* to_string();
    virtual bool is_compatible(Type* type);

    virtual Type* copy() { 
        StructType* type = new StructType(kind, name); 
        copy_aux(type);
        type->fields = fields;
        type->from_copy = true;
        return type;
    }
public:
    char* name;
    std::vector<std::pair<char*, Type*>> fields;
};

class FuncType: public Type {
public:
    FuncType(Type* rtpye, std::vector<Type*> ptypes, bool has_vparam, bool ostyle):
        Type(TK_FUNC), return_type(rtpye), param_types(ptypes), 
        has_var_param(has_vparam), is_old_style(ostyle) {}
    virtual char* to_string();
    virtual bool is_compatible(Type* type);

    virtual Type* copy() { 
        Type* type = new FuncType(return_type, param_types, has_var_param, is_old_style); 
        copy_aux(type);
        type->from_copy = true;
        return type;      
    }
public:
    Type* return_type;
    std::vector<Type*> param_types;
    bool has_var_param;
    bool is_old_style;
};

extern Type *type_void;
extern Type *type_bool;
extern Type *type_char;
extern Type *type_short;
extern Type *type_int;
extern Type *type_long;
extern Type *type_llong;
extern Type *type_uchar;
extern Type *type_ushort;
extern Type *type_uint;
extern Type *type_ulong;
extern Type *type_ullong;
extern Type *type_float;
extern Type *type_double;
extern Type *type_ldouble;
extern Type *type_enum;

Type* make_null_type();
Type* make_number_type(int kind, bool is_unsigned);
Type* make_ptr_type(Type* type);
Type* make_array_type(Type* type, int length);
Type* make_struct_type(int kind, char* name = "");
Type* make_func_type(Type* return_type, std::vector<Type*> param_types, bool has_var_arg, bool is_old_style);

