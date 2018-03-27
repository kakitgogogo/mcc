#include <iostream>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include "lexer.h"
using namespace std;

int main(int argc, char* argv[])
{
    vector<shared_ptr<Token>> toks;
    Pos pos{"1", 1, 1};
    for(int i = 0; i < 10; ++i) {
        toks.push_back(make_number(strdup(to_string(i).c_str()), pos));
    }
    for(auto t:toks) printf("%s ", t->to_string());
    cout << endl;
	Lexer lexer(toks);

	auto tok = lexer.get_token();
	while(tok->kind != TEOF) {
		cerr << tok->to_string() << endl;
        tok = lexer.get_token();
	}
	cerr << endl;
}
