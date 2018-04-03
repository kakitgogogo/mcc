#include <iostream>
#include "lexer.h"
#include "parser.h"
#include "generator.h"
using namespace std;

char* replace_suffix(char* filename, char suffix) {
    char* res = format("%s", filename);
    char* end = res + strlen(res) - 1;
    if (*end != 'c')
        error("filename suffix is not .c");
    *end = suffix;
    return res;
}

int main(int argc, char* argv[]) {
    Lexer lexer(argv[1]);
    Preprocessor preprocessor(&lexer);
    Parser parser(&preprocessor);
    Generator generator(replace_suffix(argv[1], 's'), &parser);
    generator.run();
}