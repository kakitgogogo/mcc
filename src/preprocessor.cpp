#include <stdlib.h>
#include <assert.h>
#include "preprocessor.h"

void Preprocessor::init_keywords(std::map<char*, int, cstr_cmp>& keywords) {
#define def(id, str) keywords[str] = id;
#include "keywords.h"
#undef def
}

Preprocessor::Preprocessor(Lexer* lexer): lexer(lexer) {
    init_keywords(keywords);
}

TokenPtr Preprocessor::maybe_convert_to_keyword(TokenPtr tok) {
    if(tok->kind != TIDENT) return tok;
    char* name = tok->to_string();
    auto iter = keywords.find(name);
    if(iter != keywords.end()) {
        return make_keyword(iter->second, tok->get_pos());
    }
    return tok;
}

TokenPtr Preprocessor::get_token() {
    TokenPtr tok = lexer->get_token();
    while(tok->kind > TPP) tok = lexer->get_token();
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