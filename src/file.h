#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <vector>

struct File {
    char* name;
    int row;
    int col;
    int last_col;
    // one of file and stream will be null
    FILE* file;
    char* stream; // maybe read by stringstream
};

class FileSet {
public:
    FileSet() {}

    ~FileSet();

    int count() { return files.size(); }

    File& current_file() { return files.back(); }

    void push_file(FILE* file, char* name);
    void push_string(char* s);

    void pop_file();

    int get_chr();
    void unget_chr(int c);

    int peek();
    bool next(int c);

private:
    int get_chr_aux();

private:
    std::vector<File> files;
    std::vector<int> buf;
};