#pragma once

#include <set>
#include <memory>
#include "file.h"
#include "error.h"
#include "token.h"

class Lexer {
public:
    Lexer(char* filename);
    Lexer(std::vector<std::shared_ptr<Token> >& toks);

    std::shared_ptr<Token> get_token();
    void unget_token(std::shared_ptr<Token> token);

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
};