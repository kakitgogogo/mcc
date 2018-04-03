#include <iostream>
#include <fstream>
#include <assert.h>
#include "ast.h"
#include "lexer.h"
#include "preprocessor.h"
#include "parser.h"
using namespace std;

int main(int argc, char* argv[]) {
    assert(argc == 2);
    Lexer lexer0(argv[1]);

#if 1

    TokenPtr tok = lexer0.get_token();
    while(tok->kind != TEOF) {
        if(tok->kind > TPP) goto next;
        if (tok->begin_of_line)
            cerr << "\n";
        if (tok->leading_space)
            cerr << " ";
        cerr << tok->to_string();
next:
        tok = lexer0.get_token();
    }
    cerr << endl;

#endif

    Lexer lexer(argv[1]);
    Preprocessor preprocessor(&lexer);
    Parser parser(&preprocessor);

    vector<NodePtr> ast = parser.get_ast();
	
    dump_ast(argv[1], ast);
}