#include <iostream>
#include "lexer.h"
using namespace std;

int main(int argc, char* argv[])
{
	if(argc < 2) 
	{
		exit(1);
	}
	Lexer lexer(argv[1]);

	auto tok = lexer.get_token();
	while(tok->kind != TEOF)
	{
		if(tok->kind == TNEWLINE) goto gettok;
		if(tok->kind == TINVALID) goto gettok;
		if (tok->begin_of_line)
            cerr << "\n";
        if (tok->leading_space)
            cerr << " ";
		cerr << tok->to_string();
gettok:
		tok = lexer.get_token();
	}
	cerr << endl;
}
