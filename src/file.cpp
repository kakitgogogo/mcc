#include <unistd.h>
#include <string.h>
#include "file.h"

void FileSet::push_file(FILE* file, char* name) {
    File f;
    f.file = file;
    f.name = name;
    f.row = 1;
    f.col = 1;
    f.last_col = 1;
    files.push_back(f);
}

void FileSet::push_string(char* s) {
    File f;
    f.file = NULL;
    f.stream = s;
    f.name = "-";
    f.row = 1;
    f.col = 1;
    files.push_back(f);
}

void FileSet::pop_file() {
    File& f = files.back();
    if(f.file) {
        fclose(f.file);
    }
    files.pop_back();
}

FileSet::~FileSet() {
    for(auto f:files) {
        if(f.file) {
            fclose(f.file);
        }
    }
}

int FileSet::get_chr_aux() {
    int c;
    File& f = current_file();

    // buffer have elements
    if(buf.size() > 0) { // from buffer
        c = buf.back();
        buf.pop_back();
    }
    else if(f.file) { // from file
        c = ::getc(f.file);
        // "\r\n" or "\r" are considered as "\n"
        if(c == '\r') {
            int c1 = ::getc(f.file);
            if(c1 != '\n')
                ::ungetc(c1, f.file);
            c = '\n';
        }
    } 
    else { // from stringstream
        c = *f.stream;
        if(c == '\0') c = EOF;
        else if(c == '\r') {
            ++f.stream;
            if(*f.stream == '\n')
                ++f.stream;
            c = '\n';
        }
        else {
            ++f.stream;
        }
    }

    if(c == '\n') {
        ++f.row;
        f.last_col = f.col;
        f.col = 1;
    }
    else if(c != EOF) {
        ++f.col;
    }
    return c;
}

int FileSet::get_chr() {
    while(true) {
        int c = get_chr_aux();
        if(c == EOF) {
            if(count() == 1)
                return c;
            pop_file();
            return '\n';
        }
        if(c != '\\')
            return c;
        else {
            int c1 = get_chr_aux();
            // '\' immediately followed by a new-line character is deleted
            if(c1 == '\n') 
                continue;
            unget_chr(c1);
            return c;
        }
    }
}

void FileSet::unget_chr(int c) {
    if(c == EOF)
        return;
    buf.push_back(c);

    File& f = current_file();
    if(c == '\n') {
        --f.row;
        f.col = f.last_col;
    }
    else if(c != EOF) {
        --f.col;
    }
}

int FileSet::peek() {
    int c = get_chr();
    unget_chr(c);
    return c;
}

// test if the next character is expected 
bool FileSet::next(int expected) {
    int c = get_chr();
    if(c == expected)
        return true;
    unget_chr(c);
    return false;
}