#pragma once

#include <string>

class Buffer {
public:
    Buffer();

    int size() { return size_; }
    char* data() { return data_; }
    std::string to_string() { return std::string(data_, size_); }
    void write(char c);
    void write(char* fmt, ...);
    void write(char* fmt, va_list args);
    void append(char* s);

private:
    void realloc();

private:
    char* data_;
    int size_;
    int cap_;
};