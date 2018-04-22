#pragma once

#include <string.h>

struct Pos {
    char* filename;
    int row;
    int col;
};

char* format(char* fmt, ...);

char* quote_char(char c);
char* quote_string(char* s);
char* quote_string(char* s, int len);

struct cstr_cmp {
    bool operator()(const char* a, const char* b) {
        return ::strcmp(a, b) < 0;
    }
};