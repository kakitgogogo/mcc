// #define a 111

#include <stdio.h>

#define x(a, b) a##b

x(xx, xx)

#define str(a) #a

str(xxxxxx)

// int main() {
//     int b = a;
//     int x(xx, xx);
// }