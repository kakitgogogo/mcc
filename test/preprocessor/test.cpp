#include <iostream>
#include "lexer.h"
#include "preprocessor.h"
#include "parser.h"
using namespace std;

using TokenPtr = shared_ptr<Token>;

int main(int argc, char* argv[]) {
    assert(argc == 2);

//     cerr << "--------------------- source ---------------------" << endl;

//     Lexer lexer0(argv[1]);
//     TokenPtr tok = lexer0.get_token();
//     while(tok->kind != TEOF) {
//         if(tok->kind > TPP) goto next;
//         if (tok->begin_of_line)
//             cerr << "\n";
//         if (tok->leading_space)
//             cerr << " ";
//         cerr << tok->to_string();
// next:
//         tok = lexer0.get_token();
//     }
//     cerr << endl;

    cerr << "------------------ after preprocess ---------------" << endl;

    Lexer lexer(argv[1]);
    Preprocessor preprocessor(&lexer);

    while(true) {
        TokenPtr tok = preprocessor.get_token();
        if(tok->kind == TEOF) break;
        if (tok->begin_of_line)
            cerr << "\n";
        if (tok->leading_space)
            cerr << " ";
        cerr << tok->to_string();
    }
    cerr << endl;
}