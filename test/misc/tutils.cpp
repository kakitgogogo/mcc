#include <iostream>
#include "utils.h"
using namespace std;

int main() {
    char* s = format("%s%f", "asdfg", 1.1111);
    cout << s << endl;
}