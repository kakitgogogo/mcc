#include "utiltest.h"

void test_array() {
    int a[3] = {1, 2, 3};
    EXPECT_INT(a[0], 1);
    EXPECT_INT(a[1], 2);
    EXPECT_INT(a[2], 3);

    a[0] = 0;
    EXPECT_INT(a[0], 0);

    *a = 1;
    *(a+1) = 1;
    EXPECT_INT(a[0], 1);
    EXPECT_INT(a[1], 1);

    int b[2][2] = {{0,1},{2,3}};
    EXPECT_INT(b[0][0], 0);
    EXPECT_INT(b[0][1], 1);
    EXPECT_INT(b[1][0], 2);
    EXPECT_INT(b[1][1], 3);
}

int main() {
    test_array();
    print_result(); 
}