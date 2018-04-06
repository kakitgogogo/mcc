#include "string.h"
#include "utils.h"
#include "token.h"

std::shared_ptr<Token> make_token(int kind, const Pos& pos) {
    Token* tok = new Token(kind);
    tok->filename =  pos.filename;
    tok->row = pos.row;
    tok->col = pos.col;
    return std::shared_ptr<Token>(tok);
}

std::shared_ptr<Token> make_keyword(int kind, const Pos& pos) {
    Token* tok = new Keyword(kind);
    tok->filename =  pos.filename;
    tok->row = pos.row;
    tok->col = pos.col;
    return std::shared_ptr<Token>(tok);
}

std::shared_ptr<Token> make_ident(char* name, const Pos& pos) {
    Token* tok = new Ident(name);
    tok->filename =  pos.filename;
    tok->row = pos.row;
    tok->col = pos.col;
    return std::shared_ptr<Token>(tok);
}

std::shared_ptr<Token> make_number(char* s, const Pos& pos) {
    Token* tok = new Number(s);
    tok->filename =  pos.filename;
    tok->row = pos.row;
    tok->col = pos.col;
    return std::shared_ptr<Token>(tok);
}

std::shared_ptr<Token> make_char(char c, int enc, const Pos& pos) {
    Token* tok = new Char(c, enc);
    tok->filename =  pos.filename;
    tok->row = pos.row;
    tok->col = pos.col;
    return std::shared_ptr<Token>(tok);
}

std::shared_ptr<Token> make_string(char* s, int enc, const Pos& pos) {
    Token* tok = new String(s, enc);
    tok->filename =  pos.filename;
    tok->row = pos.row;
    tok->col = pos.col;
    return std::shared_ptr<Token>(tok);
}

bool Token::is_keyword(int k) {
    return kind == k;
}

bool Token::is_ident(char* s) {
    return (kind == TIDENT) && (!strcmp(to_string(), s));
}

char* Token::to_string() {
    switch(kind) {
    case TSPACE: return "<space>";
    case TNEWLINE: return "<newline>";
    case TINVALID: return "<invalid>";
    case TEOF: return "<eof>";
    default: return "";
    }
}

char* Keyword::to_string() {
    switch(kind) {
#define def(id, str) case id: return str;
#include "keywords.h"
#undef def
    default: return format("%c", kind);
    }
}

char* Ident::to_string() {
    return name;
}

char* Number::to_string() {
    return value;
}

// According to C11 6.4.4.4p9 and 6.4.5p3:
static char *get_encode_str(int enc) {
    switch (enc) {
    case ENC_CHAR16: return "u";
    case ENC_CHAR32: return "U";
    case ENC_UTF8:   return "u8";
    case ENC_WCHAR:  return "L";
    }
    return "";
}

char* Char::to_string() {
    return format("%s'%s'", get_encode_str(encode_method), quote_char(character));
}

char* String::to_string() {
    return format("%s\"%s\"", get_encode_str(encode_method), quote_string(value));
}

