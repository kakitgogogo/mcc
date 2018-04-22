#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include "utils.h"
#include "buffer.h"

char* format(char* fmt, ...) {
    Buffer b;
    va_list args;
    va_start(args, fmt);
    b.write(fmt, args);
    va_end(args);
    return b.data();
}

/*
According to C11 6.4.4.4:
simple-escape-sequence: one of
    \' \" \? \\
    \a \b \f \n \r
    \t
    \v
*/
static char* quote(char c) {
    switch(c) {
    case '\'': return "\\'";
    case '"': return "\\\"";
    case '\?': return "\\?";
    case '\\': return "\\\\";
    case '\a': return "\\a";
    case '\b': return "\\b";
    case '\f': return "\\f";
    case '\n': return "\\n";
    case '\r': return "\\r";
    case '\t': return "\\t";
    case '\v': return "\\v";
    }
    return NULL;
}

char* quote_string(char* s) {
    Buffer b;
    while(*s) {
        char* q = quote(*s);
        if(q) {
            b.write("%s", q);
        }
        else if(isprint(*s)) {
            b.write("%c", *s);
        }
        else {
            b.write("\\x%02x", *s);
        }
        ++s;
    }
    return b.data();
}

char* quote_string(char* s, int len) {
    Buffer b;
    for(int i = 0; i < len-1; ++i) {
        if(s[i] == '\0') {
            b.write("\\0");
            continue;
        }
        char* q = quote(s[i]);
        if(q) {
            b.write("%s", q);
        }
        else if(isprint(s[i])) {
            b.write("%c", s[i]);
        }
        else {
            b.write("\\x%02x", s[i]);
        }
    }
    return b.data();
}

char* quote_char(char c) {
    char* q = quote(c);
    if(q) {
        return format("%s", q);
    }
    return format("%c", c);
}