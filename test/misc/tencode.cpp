#include <iostream>
#include <string.h>
#include "encode.h"
using namespace std;

int main() {
    char* s = u8"哈哈哈";
    cout << s << endl;
    char* end = s + strlen(s);
    Buffer buf;
    while(s < end) {
        uint32_t u;
        s += decode_utf8(u, s, end);
        encode_utf8(buf, u);
    }
    cout << buf.to_string() << endl;
}