#include "unittest.h"

union union1 {
    char a;
    int b;
    long c;
};

void test_union1() {
    union union1 a;

    a.c = 0xFFFFFFFF;

    EXPECT_INT(a.a, -1);
    EXPECT_INT(a.b, -1);
    EXPECT_INT((unsigned long)a.c, 0xFFFFFFFF);
}

typedef union {
    unsigned char __c[8];
    long long __d;
} huge_val_type;

void test_union2() {
    huge_val_type a = {{0, 0, 0, 0, 0, 0, 0xf0, 0x7f}};

    EXPECT_INT(a.__c[0], 0);
    EXPECT_INT(a.__c[1], 0);
    EXPECT_INT(a.__c[2], 0);
    EXPECT_INT(a.__c[3], 0);
    EXPECT_INT(a.__c[4], 0);
    EXPECT_INT(a.__c[5], 0);
    EXPECT_INT(a.__c[6], 0xf0);
    EXPECT_INT(a.__c[7], 0x7f);

    EXPECT_INT(a.__d, 0x7ff0000000000000);
}

int main() {
    test_union1();
    test_union2();
    print_result(); 
}