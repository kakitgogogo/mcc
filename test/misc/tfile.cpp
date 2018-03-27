#include <iostream>
#include <string.h>
#include "file.h"
using namespace std;

int main() {
    FileSet fs;

    FILE* f1 = fopen("1", "r");
    FILE* f2 = fopen("2", "r");
    FILE* f3 = fopen("3", "r");
    fs.push_string("4444\n");
    fs.push_file(f3, "3");
    fs.push_file(f2, "2");
    fs.push_file(f1, "1");
    int c = fs.get_chr();
    fs.unget_chr(c);
    while(c != EOF) {
        File& f = fs.current_file();
        cout << f.name << " " << f.row << " " << f.col << endl;
        c = fs.get_chr();
    }
}