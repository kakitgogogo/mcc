#pragma once

#include <memory>
#include <set>
#include <map>
#include "token.h"
#include "lexer.h"

using TokenPtr = std::shared_ptr<Token>;

class MacroParamToken: public Token {
public:
    MacroParamToken(int p = 0, bool i = false):
        Token(TMACRO_PARAM), position(p), is_var_param(i) {}

public:
    // useful if the token is a macro parameter
    int position;
    bool is_var_param;
};

class Macro {
public:
    using BodyType = std::vector<std::shared_ptr<Token>>;
    Macro(BodyType toks): body(toks) {}
    virtual ~Macro() {}
    virtual void expand() {}
private:
    BodyType body;
};

class ObjectMacro: public Macro {
public:
    ObjectMacro(BodyType toks): Macro(toks) {}
    virtual void expand(); 
};

class FunctionMacro: public Macro {
public:
    FunctionMacro(BodyType toks, int n, bool h): Macro(toks), nargs(n), has_var_param(h) {}
    virtual void expand();
private:
    int nargs;
    bool has_var_param;
};

class PredefinedMacro: public Macro {

};

class Preprocessor {
public:
    Preprocessor(Lexer* lexer);

    std::shared_ptr<Token> get_token();
    void unget_token(std::shared_ptr<Token> tok);
    std::shared_ptr<Token> peek_token();
    bool next(int kind);

    void open_undo_mode() { allow_undo = true; }
    void undo();

private:
    // C11 6.10.1: conditional inclusion
    void read_if();
    void read_ifdef();
    void read_idndef();
    void read_elif();
    void read_else();
    void read_endif();

    // C11 6.10.2: source file include
    void read_include();

    // C11 6.10.3: macro replacement
    void read_object_macro(std::shared_ptr<Token> name);
    void read_function_macro(std::shared_ptr<Token> name);
    void read_define();
    void read_undef();

    // C11 6.10.4: line control
    void read_line();

    // C11 6.10.5: line directive
    void read_error();

    // C11 6.10.6: pragma directive
    void read_pragma();

    // C11 6.10.7: null directive
    void read_null();

    // C11 6.10.8: predefine macro names
    void read_predefined_macro();

    // C11 6.10.9: pragma operator
    void read_pragma_operator();

    // macro expansion
    Token* get_expand_token();

    // some aux function
    void init_keywords(std::map<char*, int, cstr_cmp>& keywords);
    TokenPtr maybe_convert_to_keyword(TokenPtr tok);

private:
    Lexer* lexer;
    std::map<char*, Macro, cstr_cmp> macros;
    std::set<char*, cstr_cmp> onces;

    std::map<char*, int, cstr_cmp> keywords;

    bool allow_undo = false;
    std::vector<std::shared_ptr<Token>> record;
};