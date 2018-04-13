#include "utiltest.h"

void test_unary() {
    int a = 1;
    EXPECT_INT(!0, 1);
    EXPECT_INT(~0xFFFFFFFE, 1);
    EXPECT_INT(*&a, 1);
    EXPECT_INT(-a, -1);
    EXPECT_INT((int)1.1, 1);
    EXPECT_INT(++a, 2);
    EXPECT_INT(a++, 2);
}

void test_binary() {
    int a = 1, b = 2;
    EXPECT_INT(a+b, 3);
    EXPECT_INT(a-b, -1);
    EXPECT_INT(a*b, 2);
    EXPECT_INT(a/b, 0);
    EXPECT_INT(a%b, 1);
    EXPECT_INT(a&b, 0);
    EXPECT_INT(a|b, 3);
    EXPECT_INT(a^3, 2);

    EXPECT_FALSE(a&&0);
    EXPECT_TRUE(a||0);
    EXPECT_TRUE(a>0);
    EXPECT_TRUE(a<b);
    EXPECT_TRUE(a<=1);
    EXPECT_TRUE(a>=1);
    EXPECT_TRUE(a==1);

    EXPECT_INT(a=b=3, 3);
    EXPECT_INT((1,2,3,4), 4);
}

void test_ternary() {
    int a = 1, b = 2;
    EXPECT_DOUBLE(a>b?1.0:2.0, 2.0);
}

int main() {
    test_unary();
    test_binary();
    test_ternary();
    print_result(); 
}