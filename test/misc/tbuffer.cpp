#include <iostream>
#include <string>
#include <stdarg.h>
#include "buffer.h"
using namespace std;

void write_buf(Buffer& b, char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    b.write(fmt, args);
    va_end(args);
}

int main() {
    Buffer b;
    b.write('a');
    b.write("%s%d", "bcd", 1);
    b.append("efgsdfsgfdsgsdgfdgdfgfdg\0\0\0");
    write_buf(b, "%c%f", '#', 1.1111);
    cout << b.to_string() << endl;
    cout << b.data() << endl;
    cout << b.size() << endl;
}