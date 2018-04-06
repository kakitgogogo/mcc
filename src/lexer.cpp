#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdarg.h>
#include <algorithm>
#include "encode.h"
#include "lexer.h"
#include "buffer.h"

Lexer::Lexer(char* filename) {
    FILE* fp = fopen(filename, "r");
    if(!fp) {
        error("Fail to open %s: %s", filename, strerror(errno));
    }
    fileset.push_file(fp, filename);

    base_file = filename;
}

Lexer::Lexer(std::vector<std::shared_ptr<Token> >& toks) {
    std::reverse(toks.begin(), toks.end());
    buffer = toks;
}

/*
According to C11 6.4p1:
white space consists of comments (described later), 
or white-space characters (space, horizontal tab, new-line, 
vertical tab, and form-feed), or both.

In here, newline has other usage
*/

bool Lexer::skip_space_aux() {
    int c = fileset.get_chr();
    if(c == EOF) {
        return false;
    }
    else if(isspace(c) && c != '\n') {
        return true;
    }
    else if(c == '/') {
        if(fileset.next('/')) { // line comment
            while(c != '\n' && c != EOF) 
                c = fileset.get_chr();
            if(c == '\n') fileset.unget_chr(c);
            return true;
        }
        else if(fileset.next('*')) { // block comment
            Pos pos = get_pos(-2); // has read "/*"
            while(true) {
                c = fileset.get_chr();
                if(c == EOF) {
                    fileset.unget_chr(c);
                    errorp(pos, "unexpected end of block comment");
                    return false;
                }
                if(c == '*')
                    if(fileset.next('/'))
                        return true;
            }
        }
    }
    fileset.unget_chr(c);
    return false;
}

bool Lexer::skip_space() {
    if(!skip_space_aux())
        return false;
    while(skip_space_aux());
    return true;
}

/* C11 6.4.4: escape-sequence
 escape-sequence:
    simple-escape-sequence
    octal-escape-sequence
    hexadecimal-escape-sequence
    universal-character-name
*/
// doc/lexer/escape.dot
int Lexer::read_escape_char() {
    Pos pos = get_pos(-1); // has read "/"
    int c = fileset.get_chr();
    switch (c) {
    case '\'': case '"': case '?': case '\\':
        return c;
    case 'a': return '\a';
    case 'b': return '\b';
    case 'f': return '\f';
    case 'n': return '\n';
    case 'r': return '\r';
    case 't': return '\t';
    case 'v': return '\v';
    case 'e': return '\033';  // '\e' is GNU extension
    case '0' ... '7': return read_octal_char(c);
    case 'x': return read_hex_char();
    case 'u': return read_universal_char(4);
    case 'U': return read_universal_char(8);
    }
    warnp(pos, "unknown escape character: \\%c", c);
    return c;
}

// one of \0, \0o, \0oo
int Lexer::read_octal_char(int c) {
    int o = c - '0';
    int i = 1;
    while(i++ < 3) {
        c = fileset.get_chr();
        if(c < '0' || c > '7') {
            fileset.unget_chr(c);
            return o;
        }
        o = (o << 3) | (c - '0');
    }
    return o;
}

// \xdd...
int Lexer::read_hex_char() {
    Pos pos = get_pos(-2);
    int c = fileset.get_chr();
    if (!isxdigit(c)) {
        fileset.unget_chr(c);
        errorp(get_pos(0), "invalid hex character: %c", c == '\n' ? ' ' : c);
        return 0xD800;
    }
    int h = 0;
    while(true) {
        c = fileset.get_chr();
        switch (c) {
        case '0' ... '9': 
            h = (h << 4) | (c - '0'); break;
        case 'a' ... 'f': 
            h = (h << 4) | (c - 'a' + 10); break;
        case 'A' ... 'F': 
            h = (h << 4) | (c - 'A' + 10); break;
        default: 
            if(c > 0xFF) warnp(pos, "hex escape sequence out of range");
            fileset.unget_chr(c); return h;
        }
    }
}

// len is 4(\uxxxx) or 8(\UXXXXXXXX)
int Lexer::read_universal_char(int len) {
    Pos pos = get_pos(-2);
    unsigned int u = 0;
    for(int i = 0; i < len; ++i) {
        char c = fileset.get_chr();
        switch(c) {
        case '0' ... '9': 
            u = (u << 4) | (c - '0'); break;
        case 'a' ... 'f': 
            u = (u << 4) | (c - 'a' + 10); break;
        case 'A' ... 'F': 
            u = (u << 4) | (c - 'A' + 10); break;
        default: 
            fileset.unget_chr(c);
            errorp(get_pos(0), "invalid universal character: %c", c == '\n' ? ' ' : c);
            return 0xD800;
        }
    }
    // According to C11 6.4.3p2:
    // A universal character name shall not specify a character whose short identifier is less than
    // 00A0 other than 0024 ($), 0040 (@), or 0060 (‘), nor one in the range D800 through
    // DFFF inclusive.
    if((u >= 0xD800 && u <= 0xDFFF) || (u <= 0xA0 && (u != '$' || u != '@' || u != '`'))) {
        errorp(pos, "\\%c%0*x is not a valid universal character", (len == 4) ? 'u' : 'U', len, u);
        return 0xD800;
    }
    return u;
}

/*
C11 6.4.2.1:
identifier:
    identifier-nondigit
    identifier identifier-nondigit
    identifier digit
identifier-nondigit:
    nondigit
    universal-character-name
    other implementation-defined characters
*/
// doc/lexer/ident.dot
std::shared_ptr<Token> Lexer::read_ident(char c) {
    Pos pos = get_pos(-1);
    Buffer buf;
    bool has_invalid_char = false;
    if(c == '\\' && (fileset.peek() == 'u' || fileset.peek() == 'U')) {
        int u = read_escape_char();
        if(u == 0xD800) has_invalid_char = true;
        encode_utf8(buf, u);
    }
    else {
        buf.write(c);
    }
    while(true) {
        c = fileset.get_chr();
        if(isalnum(c) || c == '_') {
            buf.write(c);
            continue;
        }
        else if(c == '\\' && (fileset.peek() == 'u' || fileset.peek() == 'U')) {
            int u = read_escape_char();
            if(u == 0xD800) has_invalid_char = true;
            encode_utf8(buf, u);
            continue;
        }
        fileset.unget_chr(c);
        buf.write('\0');
        if(has_invalid_char) return make_token(TINVALID, pos);
        return make_ident(buf.data(), pos);
    }
}

std::shared_ptr<Token> Lexer::read_number(char c) {
    Pos pos = get_pos(-1);
    Buffer buf;
    buf.write(c);
    char last = c;
    while(true) {
        int c = fileset.get_chr();
        bool isfloat = strchr("eEpP", last) && strchr("+-", c);
        if(!isalnum(c) && c != '.' && !isfloat) {
            fileset.unget_chr(c);
            buf.write('\0');
            return make_number(buf.data(), pos);
        }
        buf.write(c);
        last = c;
    }
}

std::shared_ptr<Token> Lexer::read_char(int enc) {
    Pos pos = get_pos(-1);
    int c = fileset.get_chr();
    if(c == EOF || c == '\n') {
        fileset.unget_chr(c);
        errorp(pos, "missing character and '\''");
        return make_token(TINVALID, pos);
    }
    int chr = (c == '\\') ? read_escape_char() : c;
    c = fileset.get_chr();
    if(c != '\'') {
        while(c != '\n') c = fileset.get_chr();
        fileset.unget_chr(c);
        errorp(pos, "missing terminating ' character");
        return make_token(TINVALID, pos);
    }
    if(chr == 0xD800) // read_hex_char() or read_universal_char() occurs error
        return make_token(TINVALID, pos);
    if(enc == ENC_NONE)
        return make_char((char)chr, enc, pos);
    else 
        return make_char(chr, enc, pos);
}

std::shared_ptr<Token> Lexer::read_string(int enc) {
    Pos pos = get_pos(-1);
    Buffer buf;
    bool has_invalid_char = false;
    while(true) {
        int c = fileset.get_chr();
        if(c == EOF || c == '\n') {
            fileset.unget_chr(c);
            errorp(pos, "missing terminating \" character");
            return make_token(TINVALID, pos);
        }
        if(c == '"') {
            break;
        }
        if(c != '\\') {
            buf.write(c);
            continue;
        }
        bool isucn = (fileset.peek() == 'u' || fileset.peek() == 'U');
        c = read_escape_char();
        if(c == 0xD800) has_invalid_char = true;
        if(isucn) {
            encode_utf8(buf, c);
        }
        else {
            buf.write(c);
        }
    }
    buf.write('\0');
    if(has_invalid_char) // read_hex_char() or read_universal_char() occurs error
        return make_token(TINVALID, pos);
    return make_string(buf.data(), enc, pos);
}

std::shared_ptr<Token> Lexer::read_token() {
    Pos pos = get_pos(0);
    if(skip_space()) {
        return make_token(TSPACE, pos);
    }
    pos = get_pos(0);
    int c = fileset.get_chr();
    switch(c) {
    case '\n': return make_token(TNEWLINE, pos);

    // identifier
    case 'a' ... 't': case 'v' ... 'z': // u may be a string suffix
    case 'A' ... 'K': case 'M' ... 'T': case 'V' ... 'Z': // L and U may be a string suffix
    case '_': 
        return read_ident(c);
    case '\\':
        if(fileset.peek() == 'u' || fileset.peek() == 'U')
            return read_ident(c);
        errorp(pos, "stray ‘\\’ in program.");
        return make_token(TINVALID, pos);
    case 'u':
        if(fileset.next('\'')) return read_char(ENC_CHAR16);
        if(fileset.next('"')) return read_string(ENC_CHAR16);
        if(fileset.next('8')) {
            if(fileset.next('"'))
                return read_string(ENC_UTF8);
            fileset.unget_chr('8');
        }
        return read_ident(c);
    case 'U': case 'L': {
        int enc = (c == 'L') ? ENC_WCHAR : ENC_CHAR32;
        if(fileset.next('\'')) return read_char(enc);
        if(fileset.next('"')) return read_string(enc);
        return read_ident(c);
    }
    // number
    case '0' ... '9':
        return read_number(c);
    
    // character
    case '\'': 
        return read_char(ENC_NONE);

    // string
    case '"':
        return read_string(ENC_NONE);

    // punctuator
    // according to C11 6.4.6p3:
    // <: :> <% %> %: %:%: behave respectively, the same as:
    // [  ]  {  }  #  ##
    case '[': case ']': case '(': case ')': case '{':
    case '}': case '~': case '?': case ';': case ',':
        return make_keyword(c, pos);
    case '.':
        if(isdigit(fileset.peek())) {
            return read_number(c);
        }
        if(fileset.next('.')) {
            if(fileset.next('.'))
                return make_keyword(P_ELLIPSIS, pos);
            return make_ident("..", pos); // not a valid ident, but maybe useful in preprocessor
        }
        return make_keyword('.', pos);
    case '-':
        if(fileset.next('-')) return make_keyword(P_DEC, pos);
        if(fileset.next('>')) return make_keyword(P_ARROW, pos);
        if(fileset.next('=')) return make_keyword(P_ASSIGN_SUB, pos);
        return make_keyword('-', pos);
    case '+':
        if(fileset.next('+')) return make_keyword(P_INC, pos);
        if(fileset.next('=')) return make_keyword(P_ASSIGN_ADD, pos);
        return make_keyword('+', pos);
    case '&':
        if(fileset.next('&')) return make_keyword(P_LOGAND, pos);
        if(fileset.next('=')) return make_keyword(P_ASSIGN_AND, pos);
        return make_keyword('&', pos);
    case '*':
        if(fileset.next('=')) return make_keyword(P_ASSIGN_MUL, pos);
        return make_keyword('*', pos);
    case '!':
        if(fileset.next('=')) return make_keyword(P_NE, pos);
        return make_keyword('!', pos);
    case '/':
        if(fileset.next('=')) return make_keyword(P_ASSIGN_DIV, pos);
        return make_keyword('/', pos);
    case '%':
        if(fileset.next('=')) return make_keyword(P_ASSIGN_MOD, pos);
        if(fileset.next('>')) return make_keyword('}', pos);
        if(fileset.next(':')) {
            if(fileset.next('%')) {
                if(fileset.next(':'))
                    return make_keyword(P_HASHHASH, pos);
                fileset.unget_chr('%');
            }
            return make_keyword('#', pos);
        }
        return make_keyword('%', pos);
    case '<':
        if(fileset.next('<')) {
            if(fileset.next('=')) return make_keyword(P_ASSIGN_SAL, pos);
            else return make_keyword(P_SAL, pos);
        }
        if(fileset.next('=')) return make_keyword(P_LE, pos);
        if(fileset.next(':')) return make_keyword('[', pos);
        if(fileset.next('%')) return make_keyword('{', pos);
        return make_keyword('<', pos);
    case '>':
        if(fileset.next('>')) {
            if(fileset.next('=')) return make_keyword(P_ASSIGN_SAR, pos);
            else return make_keyword(P_SAR, pos);
        }
        if(fileset.next('=')) return make_keyword(P_GE, pos);
        return make_keyword('>', pos);
    case '=':
        if(fileset.next('=')) return make_keyword(P_EQ, pos);
        return make_keyword('=', pos);
    case '^':
        if(fileset.next('=')) return make_keyword(P_ASSIGN_XOR, pos);
        return make_keyword('^', pos);
    case '|':
        if(fileset.next('|')) return make_keyword(P_LOGOR, pos);
        if(fileset.next('=')) return make_keyword(P_ASSIGN_OR, pos);
        return make_keyword('|', pos);
    case ':':
        if(fileset.next('>')) return make_keyword(']', pos);
        return make_keyword(':', pos);
    case '#':
        if(fileset.next('#')) return make_keyword(P_HASHHASH, pos);
        return make_keyword('#', pos);

    // eof
    case EOF:
        return make_keyword(TEOF, pos);

    // error
    default:
        errorp(pos, "stray ‘%c’ in program", c);
        return make_token(TINVALID, pos);
    }
}

std::shared_ptr<Token> Lexer::get_token() {
    if(buffer.size() > 0) {
        auto tok = buffer.back();
        buffer.pop_back();
        return tok;
    }
    if(fileset.count() == 0) return make_token(TEOF, get_pos(0));
    bool bol = (fileset.current_file().col == 1);
    auto tok = read_token();
    if(tok->kind == TSPACE) {
        tok = read_token();
        tok->leading_space = true;
    }
    tok->begin_of_line = bol;
    return tok;
}

void Lexer::unget_token(std::shared_ptr<Token> token) {
    if(token->kind == EOF) return;
    buffer.push_back(token);
}

std::shared_ptr<Token> Lexer::get_token_from_string(char* str) {
    fileset.push_string(str);
    std::shared_ptr<Token> tok = get_token();
    next(TNEWLINE);
    Pos pos = get_pos(0);
    if(peek_token()->kind != TEOF) {
        errorp(pos, "unconsumed input: %s", str);
        error("internal error: stop mcc");
    }
    fileset.pop_file();
    return tok;
}

void Lexer::get_tokens_from_string(char* str, std::vector<std::shared_ptr<Token>>& res) {
    fileset.push_string(str);
    while(true) {
        std::shared_ptr<Token> tok = read_token();
        if(tok->kind == TEOF) {
            fileset.pop_file();
            return;
        }
    }
}

std::shared_ptr<Token> Lexer::peek_token() {
    std::shared_ptr<Token> tok = get_token();
    unget_token(tok);
    return tok;
}

bool Lexer::next(int kind) {
    std::shared_ptr<Token> tok = get_token();
    if(tok->kind == kind)
        return true;
    unget_token(tok);
    return false;
}