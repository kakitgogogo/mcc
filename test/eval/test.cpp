#include <iostream>
#include <assert.h>
#include "lexer.h"
#include "preprocessor.h"
#include "parser.h"
using namespace std;

int main(int argc, char* argv[]) {
    assert(argc == 2);
    Lexer lexer0(argv[1]);
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

    Lexer lexer(argv[1]);
    Preprocessor preprocessor(&lexer);
    Parser parser(&preprocessor);

    while(preprocessor.peek_token()->kind != TEOF) {
        NodePtr left = parser.read_expr();
        // cerr << left->type->to_string() << endl;
        preprocessor.get_token();
        NodePtr right = parser.read_expr();
        preprocessor.get_token();
        assert(left->eval_int() == right->eval_int());
    }

}