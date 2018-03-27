#include <iostream>
#include <assert.h>
#include <vector>
#include <map>
#include <algorithm>
#include "type.h"
using namespace std;

int main() {
    Type* i = make_number_type(TK_LONG_LONG, true);
    assert(i->is_int_type());
    assert(!i->offset && !i->bitoff && !i->bitsize);
    Type* f = make_number_type(TK_LONG_DOUBLE, false);
    assert(f->is_float_type());
    assert(!i->is_compatible(type_llong));
    assert(f->is_compatible(type_ldouble));
    cout << i->to_string() << endl;
    cout << f->to_string() << endl;

    StructType* s = new StructType(TK_STRUCT, "s");
    char* str = "";
    s->fields.push_back(make_pair(str, i));
    s->fields.push_back(make_pair(str, i));
    s->fields.push_back(make_pair(str, f));
    StructType* s1 = new StructType(TK_STRUCT, "s1");
    s1->fields.push_back(make_pair(str, i));
    s1->fields.push_back(make_pair(str, i));
    s1->fields.push_back(make_pair(str, type_ldouble));
    assert(s->is_compatible(s1));
    cout << s->to_string() << endl;

    Type* p = make_ptr_type(make_ptr_type(f));
    Type* p1 = make_ptr_type(make_ptr_type(type_ldouble));
    assert(p->is_compatible(p1));
    cout << p->to_string() << endl;

    Type* a0 = make_array_type(type_int, 5);
    Type* a1 = make_array_type(make_number_type(TK_INT, false), 5);
    assert(a0->is_compatible(a1));
    Type* a2 = make_array_type(a0, 5);
    Type* a3 = make_array_type(a0, 6);
    assert(!a2->is_compatible(a3));
    cout << a2->to_string() << endl;

    Type* func = make_func_type(type_int, vector<Type*>({p, f}), true, false);
    cout << func->to_string() << endl;

    cout << sizeof(Type) << endl;
}