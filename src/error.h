#pragma once

#include "utils.h"
#include "token.h"

extern bool enable_warning;
extern bool has_error;

void errorf(char* file, int line, char* fmt, ...);

#define error(...) errorf(__FILE__, __LINE__, __VA_ARGS__)

void errorp(const Pos& pos, char* fmt, ...);

void warnp(const Pos& pos, char* fmt, ...);

void errorvp(const Pos& pos, char* fmt, va_list args);

void warnvp(const Pos& pos, char* fmt, va_list args);

void errort(std::shared_ptr<Token> tok, char* fmt, ...);
void warnt(std::shared_ptr<Token>  tok, char* fmt, ...);

