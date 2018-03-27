#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include "buffer.h"

Buffer::Buffer(): size_(0), cap_(8) {
    data_ = (char*)malloc(cap_);
}

void Buffer::realloc() {
    cap_ += cap_ >> 1;  /* cap * 1.5 */
    data_ = (char*)::realloc(data_, cap_);
}

void Buffer::write(char c) {
    if(size_ >= cap_)
        realloc();
    data_[size_++] = c;
}

void Buffer::write(char* fmt, ...) {
    va_list args;
    while(true) {
        int room = cap_ - size_;
        va_start(args, fmt);
        int n = vsnprintf(data_ + size_, room, fmt, args);
        va_end(args);
        if(room <= n) {
            realloc();
            continue;
        }
        size_ += n;
        return;
    }
}

void Buffer::write(char* fmt, va_list args) {
    va_list args_dup;
    while(true) {
        int room = cap_ - size_;
        va_copy(args_dup, args);
        int n = vsnprintf(data_ + size_, room, fmt, args_dup);
        va_end(args_dup);
        if(room <= n) {
            realloc();
            continue;
        }
        size_ += n;
        return;
    }
}

void Buffer::append(char* s) {
    for(size_t i = 0; i < strlen(s); ++i) {
        write(s[i]);
    }
}