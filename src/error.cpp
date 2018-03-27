#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include "error.h"

bool has_error = false;
bool enable_warning = true;

static void error_format(char* file, int line, char* fmt, va_list args) {
    fprintf(stderr, "%s:%d: ", file, line);
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
}

void errorf(char* file, int line, char* fmt, ...) {
    has_error = true;
    va_list args;
    va_start(args, fmt);
    error_format(file, line, fmt, args);
    va_end(args);
    exit(1);
}

static void errorp_format(char* label, const Pos& pos, char* fmt, va_list args) {
    fprintf(stderr, isatty(fileno(stderr)) ? "\e[1;31m[%s]\e[0m " : "[%s] ", label);
    fprintf(stderr, "%s:%d:%d: ", pos.filename, pos.row, pos.col);
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
}

void errorp(const Pos& pos, char* fmt, ...) {
    has_error = true;
    va_list args;
    va_start(args, fmt);
    errorp_format("ERROR", pos, fmt, args);
    va_end(args);
}

void warnp(const Pos& pos, char* fmt, ...) {
    if(!enable_warning) return;
    va_list args;
    va_start(args, fmt);
    errorp_format("WARNING", pos, fmt, args);
    va_end(args);
}

void errorvp(const Pos& pos, char* fmt, va_list args) {
    has_error = true;
    errorp_format("ERROR", pos, fmt, args);
}

void warnvp(const Pos& pos, char* fmt, va_list args) {
    has_error = true;
    errorp_format("WARNING", pos, fmt, args);
}

void errort(std::shared_ptr<Token> tok, char* fmt, ...) {
    Pos pos({tok->filename, tok->row, tok->col});
    va_list args;
    va_start(args, fmt);
    errorvp(pos, fmt, args);
    va_end(args);
}

void warnt(std::shared_ptr<Token>  tok, char* fmt, ...) {
    Pos pos({tok->filename, tok->row, tok->col});
    va_list args;
    va_start(args, fmt);
    warnvp(pos, fmt, args);
    va_end(args);
}