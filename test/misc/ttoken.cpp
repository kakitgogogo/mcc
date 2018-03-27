#include <iostream>
#include <string>
#include <stdio.h>
#include "token.h"
#include "file.h"
using namespace std;

int main() {
    Pos pos({ "1", 1, 2 });
    auto kw = make_keyword(KW_SHORT, pos);
    auto ident = make_ident("a", pos);
    auto num = make_number("123", pos);
    auto chr = make_char('\'', ENC_CHAR32, pos);
    auto str = make_string("abc\a\b\n\f\t\v\?\"积极", ENC_UTF8, pos);

    cout << kw->to_string() << endl;
    cout << ident->to_string() << endl;
    cout << num->to_string() << endl;
    cout << chr->to_string() << endl;
    cout << str->to_string() << endl;

    cout << kw->is_keyword(KW_SHORT) << endl;
    cout << ident->is_keyword(KW_SHORT) << endl;
    cout << ident->is_ident("a") << endl;    

    cout << sizeof(Token) << endl;
}