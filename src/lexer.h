#pragma once

#include <set>
#include <memory>
#include "file.h"
#include "error.h"
#include "token.h"

class Lexer {
public:
    Lexer() {}
    Lexer(char* filename);
    Lexer(std::vector<std::shared_ptr<Token> >& toks);

    void push_file(FILE* file, char* name) { fileset.push_file(file, name); }

    FileSet& get_fileset() { return fileset; }
    char* get_base_file() { return base_file; }

    std::shared_ptr<Token> get_token();
    void unget_token(std::shared_ptr<Token> token);

    std::shared_ptr<Token> get_token_from_string(char* str);
    void get_tokens_from_string(char* str, std::vector<std::shared_ptr<Token>>& res);

    std::shared_ptr<Token> peek_token();
    bool next(int kind);
private:

    Pos get_pos(int delta = 0) { 
        if(fileset.count() == 0) return Pos({"-", 1, 1});
        File& f = fileset.current_file();
        return Pos({f.name, f.row, f.col+delta}); 
    }

    void lexer_error(const Pos& pos, int c, char* fmt, ...);

    bool skip_space_aux();
    bool skip_space();

    int read_universal_char(int len);
    int read_octal_char(int c);
    int read_hex_char();
    int read_escape_char();

    std::shared_ptr<Token> read_ident(char c); // in lexer, keywords are treated as ident
    std::shared_ptr<Token> read_number(char c);
    std::shared_ptr<Token> read_char(int enc);
    std::shared_ptr<Token> read_string(int enc);

    std::shared_ptr<Token> read_token();

private:
    FileSet fileset;
    std::vector<std::shared_ptr<Token>> buffer;

    char* base_file = nullptr;
};