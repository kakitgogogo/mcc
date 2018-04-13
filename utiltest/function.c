#include "utiltest.h"

int func1() {
    return 1;
}

int func2(int a) {
    if(a < 0) return -1;
    if(a > 0) return 1;
    return 0;
}

int factorial(int n) {
    if(n == 1) return 1;
    return n * factorial(n-1);
}

void test_func() {
    int a = 1;
    EXPECT_INT(func1(), 1);

    EXPECT_INT(func2(-1), -1);
    EXPECT_INT(func2(1), 1);
    EXPECT_INT(func2(0), 0);

    EXPECT_INT(factorial(5), 120);
}

int main() {
    test_func();
    print_result(); 
}