#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <libgen.h>
#include "buffer.h"
#include "preprocessor.h"
#include "parser.h"
using namespace std;

#define errort_old errort

#define errort(...) { errort_old(__VA_ARGS__); error("stop mcc"); }

// ------------------------------- macro expansion --------------------------------

// arg is set to an array because an arg may contain multiple tokens
bool Preprocessor::read_one_args(TokenPtr ident, vector<vector<TokenPtr>>& args, bool is_ellipsis) {
    int lparen = 0;
    vector<TokenPtr> arg;
    while(true) {
        TokenPtr tok = lexer->get_token();
        if(tok->kind == TEOF) {
            errort(ident, "unterminated argument list invoking macro '%s'", 
                ident->to_string());
        }
        if(tok->kind == TNEWLINE) {
            continue;
        }
        if(tok->begin_of_line && tok->is_keyword('#')) {
            read_directive(tok);
            continue;
        }
        if(lparen == 0 && tok->is_keyword(')')) {
            lexer->unget_token(tok);
            args.push_back(arg);
            return true;
        }
        if(lparen == 0 && tok->is_keyword(',') && !is_ellipsis) {
            args.push_back(arg);
            return false;
        }
        if(tok->is_keyword('(')) {
            ++lparen;
        }
        else if(tok->is_keyword(')')) {
            --lparen;
        }
        if(tok->begin_of_line) {
            tok = tok->copy();
            tok->begin_of_line = false;
            tok->leading_space = true;
        }
        arg.push_back(tok);
    }
}

void Preprocessor::read_args(TokenPtr ident, vector<vector<TokenPtr>>& args, std::shared_ptr<FunctionMacro> macro) {
    if(macro->nargs == 0 && lexer->peek_token()->is_keyword(')')) {
        return; 
    }
    bool finish = false;
    while(!finish) {
        bool is_ellipsis = (macro->has_var_param && args.size() + 1 == macro->nargs);
        finish = read_one_args(ident, args, is_ellipsis);
    }
    if(macro->has_var_param && args.size() + 1 == macro->nargs) {
        args.push_back(vector<TokenPtr>{});
    }
    if(args.size() != macro->nargs) {
        errort(ident, "macro '%s' requires %d arguments, but only %d given", 
            ident->to_string(), macro->nargs, args.size());
    }
}

void Preprocessor::glue_tokens(vector<TokenPtr>& lefts, TokenPtr right) {
    Buffer buf;
    TokenPtr left = lefts.back();
    lefts.pop_back();
    buf.write("%s", left->to_string());
    buf.write("%s", right->to_string());
    buf.write("\0");
    Lexer new_lexer;
    TokenPtr tok = new_lexer.get_token_from_string(buf.data());
    left->copy_aux(tok);
    lefts.push_back(tok);
}

static TokenPtr stringize(TokenPtr templ, vector<TokenPtr>& args) {
    Buffer buf;
    for(int i = 0; i < args.size(); ++i) {
        if(buf.size() > 0 && args[i]->leading_space) {
            buf.write(" ");
        }
        buf.write("%s", args[i]->to_string());
    }
    TokenPtr str = make_string(buf.data(), ENC_NONE, templ->get_pos());
    templ->copy_aux(str);
    return str;
}

static void set_union(set<char*, cstr_cmp>& tok_hideset, set<char*, cstr_cmp>& hideset) {
    for(auto item:hideset) {
        tok_hideset.insert(item);
    }
}

static void set_intersection(set<char*, cstr_cmp>& tok_hideset, set<char*, cstr_cmp>& hideset) {
    set<char*, cstr_cmp> res;
    for(auto item:tok_hideset) {
        if(hideset.find(item) != hideset.end()) {
            res.insert(item);
        }
    }
    swap(hideset, res);
}

static void hideset_add(vector<TokenPtr>& toks, set<char*, cstr_cmp>& hideset) {
    for(int i = 0; i < toks.size(); ++i) {
        toks[i] = toks[i]->copy();
        set_union(toks[i]->hideset, hideset);
    }
}

void Preprocessor::subst(MacroPtr macro, std::vector<std::vector<TokenPtr>>& args, 
    std::set<char*, cstr_cmp>& hideset, std::vector<TokenPtr>& res) {
    for(int i = 0; i < macro->body.size(); ++i) {
        TokenPtr left = macro->body[i];
        TokenPtr right = (i == (macro->body.size() - 1)) ? nullptr : macro->body[i+1];
        bool left_is_param = (left->kind == TMACRO_PARAM);
        bool right_is_param = (right && right->kind == TMACRO_PARAM);
        shared_ptr<MacroParamToken> left_param, right_param;
        if(left_is_param) left_param = dynamic_pointer_cast<MacroParamToken>(left);
        if(right_is_param) right_param = dynamic_pointer_cast<MacroParamToken>(right);

        if(left->is_keyword('#') && right_is_param) {
            res.push_back(stringize(left, args[right_param->position]));
            ++i;
            continue;
        }
        // [GNU] [,##__VA_ARG__] is expanded to the empty token sequence
        // if __VA_ARG__ is empty. Otherwise it's expanded to
        // [,<tokens in __VA_ARG__>].
        if(left->is_keyword(P_HASHHASH) && right_is_param) {
            vector<TokenPtr> arg = args[right_param->position];
            if(right_param->is_var_param && res.size() > 0 && res.back()->is_keyword(',')) {
                if(arg.size() == 0) {
                    res.pop_back();
                }
                else {
                    for(auto tok:arg) {
                        res.push_back(tok);
                    }
                }
            }
            else if(arg.size() > 0) {
                glue_tokens(res, arg[0]);
                for(int i = 1; i < arg.size(); ++i) {
                    res.push_back(arg[i]);
                }
            }
            ++i;
            continue;
        }

        if(left->is_keyword(P_HASHHASH) && right) {
            glue_tokens(res, right);
            ++i;
            continue;
        }

        if(left_is_param && right && right->is_keyword(P_HASHHASH)) {
            vector<TokenPtr> arg = args[left_param->position];
            if(arg.size() == 0) ++i;
            else {
                for(auto tok:arg) {
                    res.push_back(tok);
                }
            }
            continue;
        }

        if(left_is_param) {
            vector<TokenPtr> arg = args[left_param->position];
            expand_all(left, arg, res);
            continue;
        }

        res.push_back(left);
    }
    hideset_add(res, hideset);
}

TokenPtr Preprocessor::expand_aux() {
    TokenPtr tok = lexer->get_token();
    if(tok->kind != TIDENT) return tok;
    char* name = tok->to_string();
    auto iter = macros.find(name);
    if(iter == macros.end() || tok->hideset.find(name) != tok->hideset.end()) {
        return tok;
    }
    MacroPtr macro = iter->second;

    auto unget_all = [&](vector<TokenPtr>& toks) {
        for(int i = toks.size() - 1; i >= 0; --i) {
            lexer->unget_token(toks[i]);
        }
    };

    switch(macro->kind) {
    case MK_OBJECT: {
        set<char*, cstr_cmp> hideset = tok->hideset;
        hideset.insert(name);
        auto args = vector<vector<TokenPtr>>{};
        vector<TokenPtr> toks;
        subst(macro, args, hideset, toks);
        if(toks.size() > 0) {
            toks[0]->leading_space = tok->leading_space;
            toks[0]->begin_of_line = tok->begin_of_line;
        }
        unget_all(toks);
        return expand();
    }
    case MK_FUNCTION: {
        if(!lexer->next('(')) {
            return tok;
        }
        vector<vector<TokenPtr>> args;
        read_args(tok, args, dynamic_pointer_cast<FunctionMacro>(macro));
        
        TokenPtr rparen = lexer->get_token();
        if(!rparen->is_keyword(')')) {
            errort(rparen, "expected ')'");
        }
        set<char*, cstr_cmp> hideset = tok->hideset;
        set_intersection(hideset, rparen->hideset);
        hideset.insert(name);
        vector<TokenPtr> toks;
        subst(macro, args, hideset, toks);
        if(toks.size() > 0) {
            toks[0]->leading_space = tok->leading_space;
            toks[0]->begin_of_line = tok->begin_of_line;
        }
        unget_all(toks);
        return expand();
    }
    case MK_PREDEFINE: {
        TokenPtr subst_tok = dynamic_pointer_cast<PredefinedMacro>(macro)->handler(tok);
        if(subst_tok != nullptr) return subst_tok;
        return expand();
    }
    default:
        error("internal error: unknown macro type");
    }
}

TokenPtr Preprocessor::expand() {
    while(true) {
        TokenPtr tok = expand_aux();
        if(tok->kind != TNEWLINE) {
            return tok;
        }
    }
}

void Preprocessor::expand_all(TokenPtr templ, std::vector<TokenPtr>& toks, 
    std::vector<TokenPtr>& res) {
    int size = res.size();
    Lexer* old_lexer = this->lexer;
    Lexer* new_lexer = new Lexer(toks);
    set_lexer(new_lexer);
    while(true) {
        TokenPtr tok = expand();
        if(tok->kind == TEOF) break;
        res.push_back(tok);
    }
    if(res.size() > size) {
        res[size] = res[size]->copy();
        res[size]->leading_space = templ->leading_space;
        res[size]->begin_of_line = templ->begin_of_line;
    }
    set_lexer(old_lexer);
}

// ------------------------- Preprocessing directives ---------------------------

void Preprocessor::read_directive(TokenPtr hash) {
    TokenPtr tok = lexer->get_token();
    char* name = tok->to_string();
    if(tok->kind == TNEWLINE) return;
    if(tok->kind != TIDENT) goto invalid_diretive;

    if(!strcmp(name, "if")) read_if();
    else if(!strcmp(name, "ifdef")) read_ifdef();
    else if(!strcmp(name, "ifndef")) read_idndef();
    else if(!strcmp(name, "elif")) read_elif(hash);
    else if(!strcmp(name, "else")) read_else(hash);
    else if(!strcmp(name, "endif")) read_endif(hash);
    else if(!strcmp(name, "include")) read_include(hash);
    else if(!strcmp(name, "define")) read_define();
    else if(!strcmp(name, "undef")) read_undef();
    else if(!strcmp(name, "line")) read_line();
    else if(!strcmp(name, "error")) read_error(hash);
    else if(!strcmp(name, "pragma")) read_pragma();
    else goto invalid_diretive;

    return;
invalid_diretive:
    errort(tok, "invalid preprocessing directive #%s", tok->to_string());
}

static TokenPtr make_const_zero_token(Pos pos) {
    return make_number("0", pos);
}

static TokenPtr make_const_one_token(Pos pos) {
    return make_number("1", pos);
}

// C11 6.10.1: conditional inclusion
bool Preprocessor::read_const_expr() {
    vector<TokenPtr> toks;
    while(true) {
        TokenPtr tok = expand_aux();
        if(tok->kind == TNEWLINE) break;
        if(tok->is_ident("defined")) {
            TokenPtr ident = lexer->get_token();
            if(ident->is_keyword('(')) {
                ident = lexer->get_token();
                if(!lexer->next(')')) {
                    errort(lexer->peek_token(), "expected ')'");
                }
            }
            if(ident->kind != TIDENT) {
                errort(ident, "expected identifier");
            }
            toks.push_back((macros.find(ident->to_string()) != macros.end()) ? 
                make_const_one_token(tok->get_pos()) : make_const_zero_token(tok->get_pos()));
        }
        else if(tok->kind == TIDENT) {
            toks.push_back(make_const_zero_token(tok->get_pos()));
        }
        else {
            toks.push_back(tok);
        }
    }

    Lexer new_lexer(toks);
    Preprocessor pp(&new_lexer);
    Parser parser(&pp);
    return parser.read_expr()->eval_int();
} 

void Preprocessor::skip_cond_incl() {
    int level = 0;
    while(true) {
        TokenPtr tok = lexer->get_token();
        if(!tok->begin_of_line || !tok->is_keyword('#')) 
            continue;
        
        TokenPtr hash = tok;
        tok = lexer->get_token();
        if(level == 0 && (tok->is_ident("elif") || tok->is_ident("else") || tok->is_ident("endif"))) {
            lexer->unget_token(tok);
            lexer->unget_token(hash);
            return;
        }
        if(tok->is_ident("if") || tok->is_ident("ifdef") || tok->is_ident("ifndef")) {
            ++level;
        }
        else if(tok->is_ident("endif")) {
            --level;
        }
    }
}

void Preprocessor::read_if() {
    TokenPtr tok = lexer->peek_token();
    bool is_true = read_const_expr();
    cond_incl_stack.push_back((CondInclCtx){CIK_IF, is_true});
    if(!is_true) {
        skip_cond_incl();
    }
}

void Preprocessor::read_ifdef() {
    TokenPtr tok = lexer->get_token();
    if(tok->kind != TIDENT) {
        errort(tok, "expected identifier");
    }
    if(!lexer->next(TNEWLINE)) {
        errort(lexer->peek_token(), "expected newline");
    }
    bool is_true = (macros.find(tok->to_string()) != macros.end());
    cond_incl_stack.push_back((CondInclCtx){CIK_IF, is_true});
    if(!is_true) {
        skip_cond_incl();
    }
}

void Preprocessor::read_idndef() {
    TokenPtr tok = lexer->get_token();
    if(tok->kind != TIDENT) {
        errort(tok, "expected identifier");
    }
    if(!lexer->next(TNEWLINE)) {
        errort(lexer->peek_token(), "expected newline");
    }
    bool is_true = (macros.find(tok->to_string()) == macros.end());
    cond_incl_stack.push_back((CondInclCtx){CIK_IF, is_true});
    if(!is_true) {
        skip_cond_incl();
    }
}

void Preprocessor::read_elif(TokenPtr hash) {
    if(cond_incl_stack.size() == 0) {
        errort(hash, "#elif without #if");
    }
    CondInclCtx& ci = cond_incl_stack.back();
    if(ci.kind == CIK_ELSE) {
        errort(hash, "#elif after #else");
    }
    bool is_true = read_const_expr();
    ci.kind = CIK_ELIF;
    if(ci.is_true || !is_true) {
        skip_cond_incl();
        return;
    }
    ci.is_true = is_true;
}

void Preprocessor::read_else(TokenPtr hash) {
    if(cond_incl_stack.size() == 0) {
        errort(hash, "#else without #if");
    }
    CondInclCtx& ci = cond_incl_stack.back();
    if(ci.kind == CIK_ELSE) {
        errort(hash, "#else after #else");
    }
    if(!lexer->next(TNEWLINE)) {
        errort(lexer->peek_token(), "expected newline");
    }
    ci.kind = CIK_ELSE;
    if(ci.is_true) {
        skip_cond_incl();
    }
}

void Preprocessor::read_endif(TokenPtr hash) {
    if(cond_incl_stack.size() == 0) {
        errort(hash, "#endif without #if");
    }
    if(!lexer->next(TNEWLINE)) {
        errort(lexer->peek_token(), "expected newline");
    }
    cond_incl_stack.pop_back();
}


// C11 6.10.2: source file include
static char* get_abs_path(char* path) {
    static char abs_path[256];
    if(realpath(path, abs_path) != nullptr) {
        return strdup(abs_path);
    }
    return nullptr;
}

bool Preprocessor::try_include(char* dir, char* filename) {
    char* path = get_abs_path(format("%s/%s", dir, filename));
    if(!path) return false;
    if(onces.find(path) != onces.end()) {
        return true;
    }
    FILE* fp = fopen(path, "r");
    if(!fp) {
        error("Fail to open %s: %s", filename, strerror(errno));
    }
    lexer->push_file(fp, path);
    return true;
}

void Preprocessor::read_include(TokenPtr hash) {
    char* filename;
    bool is_std = false;
    TokenPtr tok = expand_aux();
    switch(tok->kind) {
    case TSTRING: {
        filename = dynamic_pointer_cast<String>(tok)->value;
        break;
    }
    case '<': {
        vector<TokenPtr> toks;
        while(true) {
            TokenPtr t = lexer->get_token();
            if(t->kind == TNEWLINE) {
                errort(t, "expected '>'");
            }
            else if(t->kind == '>') {
                Buffer buf;
                for(auto tok:toks) {
                    buf.write("%s", tok->to_string());
                }
                buf.write("\0");
                filename = buf.data();
                break;
            }
            toks.push_back(t);
        }
        is_std = true;
        break;
    }
    default:
        errort(tok, "#include expects \"FILENAME\" or <FILENAME>");
    }

    if(!lexer->next(TNEWLINE)) {
        errort(lexer->peek_token(), "expected newline");
    }

    if(filename[0] == '/') {
        if(try_include("", filename)) {
            return;
        }
        goto include_failed;
    }
    // if(include_path) {
    //     if(try_include(include_path, filename)) {
    //         return;
    //     }
    // }
    if(!is_std) {
        char* dir;
        if(hash->filename)
            // X will change the parameter, so the parameter must be copied first
            dir = dirname(strdup(hash->filename));
        else   
            dir = ".";
        if(try_include(dir, filename)) {
            return;
        } 
    }
    for(auto path:std_include_path) {
        if(try_include(path, filename)) {
            return;
        } 
    }

include_failed:
    errort(hash, "no such file or directory: %s", filename);
}


// C11 6.10.3: macro replacement
static void hashhash_check(vector<TokenPtr>& body) {
    if(body.size() == 0)
        return;
    if(body[0]->is_keyword(P_HASHHASH))
        errort(body[0], "'##' cannot appear at start of macro expansion");
    if(body.back()->is_keyword(P_HASHHASH))
        errort(body.back(), "'##' cannot appear at end of macro expansion");
}

void Preprocessor::read_object_macro(TokenPtr name) {
    vector<TokenPtr> body;
    while(true) {
        TokenPtr tok = lexer->get_token();
        if(tok->kind == TNEWLINE) break;
        body.push_back(tok);
    }
    hashhash_check(body);
    macros[name->to_string()] = 
        shared_ptr<ObjectMacro>(new ObjectMacro(body));
}

void Preprocessor::read_function_macro(TokenPtr name) {
    vector<TokenPtr> body;
    map<char*, shared_ptr<MacroParamToken>, cstr_cmp> params;
    int position = 0;
    bool has_var_param = false;

    auto make_macro_param_token = [](int pos, bool is_var_param) {
        return shared_ptr<MacroParamToken>(new MacroParamToken(pos, is_var_param));
    };

    // read macro parameters
    while(true) {
        TokenPtr tok = lexer->get_token();
        if(tok->is_keyword(')'))
            break;
        if(position > 0) {
            if(!tok->is_keyword(',')) {
                errort(tok, "expected ','");
            }
            tok = lexer->get_token();
        }
        if(tok->kind == TNEWLINE) {
            errort(tok, "missing ')' in macro parameter list");
        }
        if(tok->is_keyword(P_ELLIPSIS)) {
            has_var_param = true;
            params["__VA_ARGS__"] = make_macro_param_token(position, true);
            break;
        }
        if(tok->kind != TIDENT) {
            errort(tok, "expected identifier");
        }
        char* param_name = tok->to_string();
        if(lexer->next(P_ELLIPSIS)) {
            if(!lexer->next(')')) {
                errort(lexer->peek_token(), "expected ')'");
            }
            has_var_param = true;
            params[param_name] = make_macro_param_token(position, true);
            break;
        }
        params[param_name] = make_macro_param_token(position++, false);
    }

    // read macro body
    while(true) {
        TokenPtr tok = lexer->get_token();
        if(tok->kind == TNEWLINE) break;
        if(tok->kind == TIDENT) {
            auto iter = params.find(tok->to_string());
            if(iter != params.end()) {
                TokenPtr subst_tok = iter->second->copy();
                subst_tok->leading_space = tok->leading_space;
                body.push_back(subst_tok);
                continue;
            }
        }
        body.push_back(tok);
    }

    hashhash_check(body);
    macros[name->to_string()] = 
        shared_ptr<FunctionMacro>(new FunctionMacro(body, params.size(), has_var_param));
}

void Preprocessor::read_define() {
    TokenPtr name = lexer->get_token();
    if(name->kind != TIDENT) {
        errort(name, "expected identifier");
    }
    TokenPtr tok = lexer->get_token();
    if(tok->is_keyword('(') && !tok->leading_space) {
        read_function_macro(name);
        return;
    }
    lexer->unget_token(tok);
    read_object_macro(name);
}

void Preprocessor::read_undef() {
    TokenPtr name = lexer->get_token();
    if(name->kind != TIDENT) {
        errort(name, "expected identifier");
    }
    macros.erase(name->to_string());
}


// C11 6.10.4: line control
static bool is_digit_sequence(char* p) {
    for(; *p; p++)
        if(!isdigit(*p))
            return false;
    return true;
}

void Preprocessor::read_line() {
    TokenPtr tok = expand_aux();
    if(tok->kind != TNUMBER || !is_digit_sequence(dynamic_pointer_cast<Number>(tok)->value)) {
        errort(tok, "number expected after #line");
    }
    int line = atoi(dynamic_pointer_cast<Number>(tok)->value);
    tok = expand_aux();
    char* filename = nullptr;
    if(tok->kind == TSTRING) {
        filename = dynamic_pointer_cast<String>(tok)->value;
        if(!lexer->next(TNEWLINE)) {
            errort(lexer->peek_token(), "expected newline");
        }
    }
    else if(tok->kind != TNEWLINE) {
        errort(tok, "expected newline or a source name");
    }
    File& current_file = lexer->get_fileset().current_file();
    current_file.row = line;
    if(filename)
        current_file.name = filename;
}


// C11 6.10.5: line directive
void Preprocessor::read_error(TokenPtr hash) {
    Buffer buf;
    while(true) {
        TokenPtr tok = lexer->get_token();
        if(tok->kind == TNEWLINE) {
            break;
        }
        if(buf.size() > 0 && tok->leading_space) {
            buf.write(" ");
        }
        buf.write("%s", tok->to_string());
    }
    errort(hash, "#error: %s", buf.data());
}


// C11 6.10.6: pragma directive
void Preprocessor::read_pragma_aux(vector<TokenPtr> toks) {
    char* oper = toks[0]->to_string();
    if(!strcmp(oper, "once")) {
        char* path = get_abs_path(toks[0]->filename);
        if(path) {
            onces.insert(path);
        }
    }
    else if(!strcmp(oper, "message")) {
        fprintf(stderr, isatty(fileno(stderr)) ? "\n\e[1;34m[NOTE]\e[0m " : "[NOTE] ");
        fprintf(stderr, "%s:%d:%d: ", toks[0]->filename, toks[0]->row, toks[0]->col);
        fprintf(stderr, "#pragma");
        for(auto tok:toks) {
            if(tok->leading_space) fprintf(stderr, " ");
            fprintf(stderr, "%s", tok->to_string());
        }
    }
    else {
        // errort(toks[0], "unknown #pragma: %s", oper);
        // ignore
    }
}

void Preprocessor::read_pragma() {
    TokenPtr tok = lexer->get_token();
    vector<TokenPtr> toks;
    if(tok->kind == TNEWLINE) {
        errort(tok, "invalid preprocessing directive #prgma");
    }
    while(tok->kind != TNEWLINE) {
        toks.push_back(tok);
        tok = lexer->get_token();
    }
    read_pragma_aux(toks);
}

// C11 6.10.7: null directive
// do not need handle

// C11 6.10.8: predefine macro names
void Preprocessor::init_predefined_macro() {
    auto make_predefined_macro = [](std::function<TokenPtr(TokenPtr)> handler) {
        return shared_ptr<PredefinedMacro>(new PredefinedMacro(handler));
    };
    auto subst_string = [&](char* str, TokenPtr tok) {
        TokenPtr subst_tok = make_string(str, ENC_NONE, tok->get_pos());
        tok->copy_aux(subst_tok);
        return subst_tok;
    };
    auto subst_number = [&](int val, TokenPtr tok) {
        TokenPtr subst_tok = make_number(format("%d", val), tok->get_pos());
        tok->copy_aux(subst_tok);
        return subst_tok;
    };

    macros["__DATE__"] = make_predefined_macro([&](TokenPtr tok) {
        char buf[20];
        struct tm now;
        time_t timet = time(NULL);
        localtime_r(&timet, &now);
        strftime(buf, sizeof(buf), "%b %e %Y", &now);
        return subst_string(strdup(buf), tok);
    });
    macros["__TIME__"] = make_predefined_macro([&](TokenPtr tok) {
        char buf[10];
        struct tm now;
        time_t timet = time(NULL);
        localtime_r(&timet, &now);
        strftime(buf, sizeof(buf), "%T", &now);
        return subst_string(strdup(buf), tok);
    });
    macros["__TIMESTAMP__"] = make_predefined_macro([&](TokenPtr tok) {
        char buf[30];
        time_t timet = time(NULL);
        strftime(buf, sizeof(buf), "%a %b %e %T %Y", localtime(&timet));
        return subst_string(strdup(buf), tok);
    });
    macros["__FILE__"] = make_predefined_macro([&](TokenPtr tok) {
        return subst_string(tok->filename, tok);
    });
    macros["__LINE__"] = make_predefined_macro([&](TokenPtr tok) {
        return subst_number(tok->row, tok);
    });
    macros["__BASE_FILE__"] = make_predefined_macro([&](TokenPtr tok) {
        return subst_string(lexer->get_base_file(), tok);
    });
    macros["__COUNTER__"] = make_predefined_macro([&](TokenPtr tok) {
        static int counter = 0;
        return subst_number(counter++, tok);
    });
    macros["__INCLUDE_LEVEL__"] = make_predefined_macro([&](TokenPtr tok) {
        int level = lexer->get_fileset().count() - 1;
        return subst_number(level, tok);
    });

    // pragma operator
    macros["_Pragma"] = make_predefined_macro([&](TokenPtr tok) {
        TokenPtr t = lexer->get_token();
        if(!t->is_keyword('(')) {
            errort(t, "expected '('");
        }
        TokenPtr operand = lexer->get_token();
        if(operand->kind == TSTRING) {
            vector<TokenPtr> toks;
            Lexer new_lexer;
            new_lexer.get_tokens_from_string(dynamic_pointer_cast<String>(operand)->value, toks);
            for(auto tok:toks) {
                tok->filename = operand->filename;
                tok->row = operand->row;
                tok->col = operand->col;
            }
            read_pragma_aux(toks);
        }
        if(!t->is_keyword(')')) {
            errort(t, "expected ')'");
        }
        return nullptr;
    });
}

// C11 6.10.9: pragma operator
// set in init_predefined_macro();

// init preprocessor
void Preprocessor::init_keywords(std::map<char*, int, cstr_cmp>& keywords) {
#define def(id, str) keywords[str] = id;
#include "keywords.h"
#undef def
}

void Preprocessor::init_std_include_path() {
    std_include_path.push_back("/usr/local/mcc/include");
    std_include_path.push_back("/usr/local/include");
    std_include_path.push_back("/usr/include");
    std_include_path.push_back("/usr/include/linux");
    std_include_path.push_back("/usr/include/x86_64-linux-gnu");

    FILE* fp = fopen("/usr/local/mcc/include/mcc.h", "r");
    if(!fp) {
        error("no such file or directory: /usr/local/mcc/include/mcc.h");
    }
    lexer->push_file(fp, "mcc.h");
}

Preprocessor::Preprocessor(Lexer* lexer): lexer(lexer) {
    setlocale(LC_ALL, "C");
    init_keywords(keywords);
    init_std_include_path();
    init_predefined_macro();
}

TokenPtr Preprocessor::maybe_convert_to_keyword(TokenPtr tok) {
    if(tok->kind != TIDENT) return tok;
    char* name = tok->to_string();
    auto iter = keywords.find(name);
    if(iter != keywords.end()) {
        TokenPtr kw = make_keyword(iter->second, tok->get_pos());
        tok->copy_aux(kw);
        return kw;
    }
    return tok;
}

TokenPtr Preprocessor::get_token() {
    TokenPtr tok;
    while(true) {
        tok = expand();
        if(tok->begin_of_line && tok->is_keyword('#')) {
            read_directive(tok);
            continue;
        }
        if(tok->kind == TINVALID) {
            while(tok->kind != TEOF) {
                tok = expand();
            }
            exit(1);
        }
        assert(tok->kind < TPP);
        break;
    }
    tok = maybe_convert_to_keyword(tok);
    if(allow_undo) {
        record.push_back(tok);
    }
    return tok;
}

void Preprocessor::unget_token(TokenPtr tok) {
    lexer->unget_token(tok);
    if(allow_undo) {
        assert(record.size() > 0);
        record.pop_back();
    }
}

TokenPtr Preprocessor::peek_token() {
    TokenPtr tok = get_token();
    unget_token(tok);
    return tok;
}

bool Preprocessor::next(int kind) {
    TokenPtr tok = get_token();
    if(tok->kind == kind) 
        return true;
    unget_token(tok);
    return false;
}

void Preprocessor::undo() {
    allow_undo = false;
    while(!record.empty()) {
        unget_token(record.back());
        record.pop_back();
    }
}