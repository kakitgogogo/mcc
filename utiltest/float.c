#include "utiltest.h"

struct pair {
    float a;
    float b;
};

void test_float() {
    float a = 1.0;
    EXPECT_DOUBLE(a, 1.0);
    EXPECT_DOUBLE(-a, -1.0);
    EXPECT_DOUBLE((float)100, 100.0);
    EXPECT_DOUBLE((float)100 / 2, 50.0);
    EXPECT_DOUBLE((float)100 * 100, 10000.0);
    EXPECT_DOUBLE(1.0 / 4, 0.25);
    EXPECT_DOUBLE(.01, 0.01);
    EXPECT_DOUBLE(0000.01, 0.01);
    EXPECT_DOUBLE(1e4, 10000.0);
    EXPECT_DOUBLE(0x10.0p1, 32.0);

    struct pair d = {2.0, 2.0};
    EXPECT_DOUBLE(d.a, 2.0);
}

int main() {
    test_float();
    print_result(); 
}