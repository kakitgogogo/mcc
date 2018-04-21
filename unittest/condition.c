#include "unittest.h"

void test_iteration() {
    int i = 0;
    if(i >= 0) {
        i = 10;
    }
    else {
        i = 20;
    }
    EXPECT_INT(i, 10);

    switch(i) {
    case 1: i = 100;
    case 2: i = 200;
    default: i = 300;
    }

    EXPECT_INT(i, 300);
}

int main() {
    test_iteration();
    print_result(); 
}