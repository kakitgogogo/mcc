#include "utiltest.h"

void test_iteration() {
    int a[5] = {0,1,2,3,4};
    int i = 0;
    for(i = 0; i < 100; ++i) {
        if(i == 5) break;
        EXPECT_INT(i, a[i]);
    }
    EXPECT_INT(i, 5);
}

int main() {
    test_iteration();
    print_result(); 
}