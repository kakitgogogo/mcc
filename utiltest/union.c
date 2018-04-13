#include "utiltest.h"

union union1 {
    char a;
    int b;
    long c;
};

void test_union() {
    union union1 a;

    a.c = 0xFFFFFFFF;

    EXPECT_INT(a.a, -1);
    EXPECT_INT(a.b, -1);
    EXPECT_INT((unsigned long)a.c, 0xFFFFFFFF);
}

int main() {
    test_union();
    print_result(); 
}