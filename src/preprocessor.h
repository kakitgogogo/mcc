#pragma once

#include <memory>
#include <set>
#include <map>
#include <functional>
#include "token.h"
#include "lexer.h"

class Macro;

using TokenPtr = std::shared_ptr<Token>;
using MacroPtr = std::shared_ptr<Macro>;

class MacroParamToken: public Token {
public:
    MacroParamToken(int pos, bool is_var_param):
        Token(TMACRO_PARAM), position(pos), is_var_param(is_var_param) {}

    virtual std::shared_ptr<Token> copy() {
        std::shared_ptr<Token> tok = std::shared_ptr<Token>(new MacroParamToken(position, is_var_param));
        copy_aux(tok);
        return tok;
    }

public:
    // useful if the token is a macro parameter
    int position;
    bool is_var_param;
};

enum MacroKind {
    MK_OBJECT,
    MK_FUNCTION,
    MK_PREDEFINE,
};

class Macro {
public:
    Macro(int kind): kind(kind) {}
    Macro(int kind, std::vector<TokenPtr> toks): kind(kind), body(toks) {}
    virtual ~Macro() {}
public:
    int kind;
    std::vector<TokenPtr> body;
};

class ObjectMacro: public Macro {
public:
    ObjectMacro(std::vector<TokenPtr> toks): Macro(MK_OBJECT, toks) {} 
};

class FunctionMacro: public Macro {
public:
    FunctionMacro(std::vector<TokenPtr> toks, int nargs, bool has_var_param): Macro(MK_FUNCTION, toks), nargs(nargs), has_var_param(has_var_param) {}
public:
    unsigned int nargs;
    bool has_var_param;
};

class PredefinedMacro: public Macro {
public:
    PredefinedMacro(std::function<TokenPtr(TokenPtr)> handler): Macro(MK_PREDEFINE), handler(handler) {}
public:
    std::function<TokenPtr(TokenPtr)> handler;
};

enum CondInclKind {
    CIK_IF,
    CIK_ELIF,
    CIK_ELSE,
};

struct CondInclCtx {
    int kind;
    bool is_true;
    // char* file;
};

class Preprocessor {
public:
    Preprocessor(Lexer* lexer);

    void set_lexer(Lexer* lex) { lexer = lex; }

    TokenPtr get_token();
    void unget_token(TokenPtr tok);
    TokenPtr peek_token();
    bool next(int kind);

    void add_include_path(char* path) { std_include_path.push_back(path); }

    void open_undo_mode() { allow_undo = true; }
    void undo();

private:
    // ------------------------ macro expansion ---------------------------
    bool read_one_args(TokenPtr ident, std::vector<std::vector<TokenPtr>>& args, bool is_ellipsis);
    void read_args(TokenPtr ident, std::vector<std::vector<TokenPtr>>& args, std::shared_ptr<FunctionMacro> macro);

    void glue_tokens(std::vector<TokenPtr>& lefts, TokenPtr right);
    
    void subst(MacroPtr macro, std::vector<std::vector<TokenPtr>>& args, std::set<char*, cstr_cmp>& hideset, std::vector<TokenPtr>& res);
    TokenPtr expand_aux();
    TokenPtr expand();
    
    void expand_all(TokenPtr templ, std::vector<TokenPtr>& tokens, std::vector<TokenPtr>& res);

    // -------------------- Preprocessing directives ----------------------
    void read_directive(TokenPtr hash);

    bool read_const_expr();
    void skip_cond_incl();
    // C11 6.10.1: conditional inclusion
    void read_if();
    void read_ifdef();
    void read_idndef();
    void read_elif(TokenPtr hash);
    void read_else(TokenPtr hash);
    void read_endif(TokenPtr hash);

    bool try_include(char* dir, char* filename);
    // C11 6.10.2: source file include
    void read_include(TokenPtr hash);

    // C11 6.10.3: macro replacement
    void read_object_macro(TokenPtr name);
    void read_function_macro(TokenPtr name);
    void read_define();
    void read_undef();

    // C11 6.10.4: line control
    void read_line();

    // C11 6.10.5: line directive
    void read_error(TokenPtr hash);

    void read_pragma_aux(std::vector<TokenPtr> toks);
    // C11 6.10.6: pragma directive
    void read_pragma();

    // C11 6.10.7: null directive
    // do not need handle

    // C11 6.10.8: predefine macro names
    void init_predefined_macro();

    // C11 6.10.9: pragma operator
    // set in init_predefined_macro();

private:
    // init function
    void init_keywords(std::map<char*, int, cstr_cmp>& keywords);
    void init_std_include_path();
    TokenPtr maybe_convert_to_keyword(TokenPtr tok);

private:
    Lexer* lexer;
    std::map<char*, MacroPtr, cstr_cmp> macros;
    std::vector<CondInclCtx> cond_incl_stack;
    std::vector<char*> std_include_path;
    std::set<char*, cstr_cmp> onces;

    std::map<char*, int, cstr_cmp> keywords;

    bool allow_undo = false;
    std::vector<TokenPtr> record;
};