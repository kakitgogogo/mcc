#include "unittest.h"

void test_iteration() {
    int a[6] = {0,1,2,3,4,5};
    int i = 0, j = 0;
    for(i = 0; i < 100; ++i) {
        EXPECT_INT(i, a[i]);
        if(i == 5) break;
    }
    EXPECT_INT(i, 5);

    int b[6] = {0,10,20,30,40,50};
    for(i = 0, j = 0; i < 100; i += 10) {
        EXPECT_INT(b[j++], i);
        if(i == 50) break;
    }
    EXPECT_INT(i, 50);
}

int main() {
    test_iteration();
    print_result(); 
}