#include <iostream>
#include "token.h"
#include "ast.h"
#include "type.h"
#include "scope.h"
using namespace std;

int main() {
    Scope scope;
    Pos pos;
    TokenPtr tok = make_token(TINVALID, pos);
    make_globalvar_node(tok, type_int, "a", &scope);
    scope.in();
    make_localvar_node(tok, type_int, "a", &scope);
    make_localvar_node(tok, type_int, "b", &scope);
    make_globalvar_node(tok, type_int, "c", &scope);
    scope.in();
    assert(scope.get("a") != nullptr);
    assert(scope.get("b") != nullptr);
    assert(scope.get("c") != nullptr);
    assert(scope.get("d") == nullptr);
    make_localvar_node(tok, type_int, "d", &scope);
    scope.out();

    assert(scope.get("a") != nullptr);
    assert(scope.get("b") != nullptr);
    assert(scope.get("c") != nullptr);
    assert(scope.get("d") == nullptr);

    scope.out();

    assert(scope.get("a") != nullptr);
    assert(scope.get("b") == nullptr);
    assert(scope.get("c") != nullptr);
    assert(scope.get("d") == nullptr);

    scope.in();
    make_localvar_node(tok, type_int, "x", &scope);
    make_localvar_node(tok, type_int, "y", &scope);
    scope.clear_local();
    assert(scope.get("x") == nullptr);
    assert(scope.get("y") == nullptr);
    scope.recover_local();
    assert(scope.get("x") != nullptr);
    assert(scope.get("y") != nullptr);
}