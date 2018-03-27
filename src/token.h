#pragma once

#include <memory>
#include <set>
#include "file.h"
#include "utils.h"

/*
According to C11 6.4:
token:
    keyword
    identifier
    constant
    string-literal
    punctuator

Here, punctuator is treated as keyword.
*/

class Token {
public:
    Token(int k): kind(k), leading_space(false), begin_of_line(false) {}
    virtual char* to_string();

    virtual ~Token() {}

    bool is_keyword(int k);
    bool is_ident(char* s);

    Pos get_pos() { return Pos{ filename, row, col }; }

public:
    int kind;
    char* filename;
    int row;
    int col;
    bool leading_space;
    bool begin_of_line;

    // hideset is used to prevent abuse of macro expansion.
    // only useful in preprocessor
    std::set<char*> hideset;
};

enum TokenKind {
    TKEYWORD = 256,

#define def(x, y) x,
#include "keywords.h"
#undef def

    TIDENT,
    TNUMBER,
    TCHAR,
    TSTRING,
    TEOF,

    // after preprocesssing,  the following will not appear
    TPP, // token whose kind > TPP will not appear in parser 
    TINVALID,
    TSPACE,
    TNEWLINE,
    TMACRO_PARAM,
};

class Keyword: public Token {
public:
    Keyword(int k): Token(k) {}
    virtual char* to_string();
};

class Ident: public Token {
public:
    Ident(char* n): Token(TIDENT), name(n) {}
    virtual char* to_string();
    char* name;
};

class Number: public Token {
public:
    Number(char* l): Token(TNUMBER), value(l) {}
    virtual char* to_string();
public:
    char* value;
};

enum EncodeMethod {
    ENC_NONE,
    ENC_CHAR16,
    ENC_CHAR32,
    ENC_UTF8,
    ENC_WCHAR
};

class Char: public Token {
public:
    Char(char c, int e): Token(TCHAR), character(c), encode_method(e) {}
    virtual char* to_string();
public:
    int character;
    int encode_method;
};

class String: public Token {
public:
    String(char* l, int e): Token(TSTRING), value(l), encode_method(e) {}
    virtual char* to_string();
public:
    char* value;
    int encode_method;
};

std::shared_ptr<Token> make_token(int kind, const Pos& pos);
std::shared_ptr<Token> make_keyword(int kind, const Pos& pos);
std::shared_ptr<Token> make_ident(char* name, const Pos& pos);
std::shared_ptr<Token> make_number(char* s, const Pos& pos);
std::shared_ptr<Token> make_char(char c, int enc, const Pos& pos);
std::shared_ptr<Token> make_string(char* s, int enc, const Pos& pos);