#include "unittest.h"
#include <math.h>

struct pair {
    float a;
    float b;
};

typedef struct floatstr floatstr;

struct floatstr {
    char* str;
    double val;
};

void test_float1() {
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

void test_float2() {
    EXPECT_DOUBLE(0.0, strtod("0", NULL));
    EXPECT_DOUBLE(0.0, strtod("-0", NULL));
    EXPECT_DOUBLE(0.0, strtod("-0.0", NULL));
    EXPECT_DOUBLE(1.0, strtod("1", NULL));
    EXPECT_DOUBLE(-1.0, strtod("-1", NULL));
    EXPECT_DOUBLE(1.5, strtod("1.5", NULL));
    EXPECT_DOUBLE(-1.5, strtod("-1.5", NULL));
    EXPECT_DOUBLE(3.1416, strtod("3.1416", NULL));
    EXPECT_DOUBLE(1E10, strtod("1E10", NULL));
    EXPECT_DOUBLE(1e10, strtod("1e10", NULL));
    EXPECT_DOUBLE(1E+10, strtod("1E+10", NULL));
    EXPECT_DOUBLE(1E-10, strtod("1E-10", NULL));
    EXPECT_DOUBLE(-1E10, strtod("-1E10", NULL));
    EXPECT_DOUBLE(-1e10, strtod("-1e10", NULL));
    EXPECT_DOUBLE(-1E+10, strtod("-1E+10", NULL));
    EXPECT_DOUBLE(-1E-10, strtod("-1E-10", NULL));
    EXPECT_DOUBLE(1.234E+10, strtod("1.234E+10", NULL));
    EXPECT_DOUBLE(1.234E-10, strtod("1.234E-10", NULL));
    EXPECT_DOUBLE(0.0, strtod("1e-10000", NULL)); /* must underflow */

    EXPECT_DOUBLE(1.0000000000000002, strtod("1.0000000000000002", NULL)); /* the smallest number > 1 */
    EXPECT_DOUBLE( 4.9406564584124654e-324, strtod("4.9406564584124654e-324", NULL)); /* minimum denormal */
    EXPECT_DOUBLE(-4.9406564584124654e-324, strtod("-4.9406564584124654e-324", NULL));
    EXPECT_DOUBLE( 2.2250738585072009e-308, strtod("2.2250738585072009e-308", NULL));  /* Max subnormal double */
    EXPECT_DOUBLE(-2.2250738585072009e-308, strtod("-2.2250738585072009e-308", NULL));
    EXPECT_DOUBLE( 2.2250738585072014e-308, strtod("2.2250738585072014e-308", NULL));  /* Min normal positive double */
    EXPECT_DOUBLE(-2.2250738585072014e-308, strtod("-2.2250738585072014e-308", NULL));
    EXPECT_DOUBLE( 1.7976931348623157e+308, strtod("1.7976931348623157e+308", NULL));  /* Max double */
    EXPECT_DOUBLE(-1.7976931348623157e+308, strtod("-1.7976931348623157e+308", NULL));
}

void test_float3() {
    floatstr* fs = malloc(sizeof(floatstr));
    char* end;
    fs->str = "3.14";
    fs->val = strtod(fs->str, &end);
    EXPECT_DOUBLE(fs->val, 3.14);

    float* d = malloc(sizeof(float));
    *d = 1;
    EXPECT_DOUBLE(*d, 1.0);
}

int main() {
    test_float1();
    test_float2();
    test_float3();
    print_result(); 
}