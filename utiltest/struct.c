#include "utiltest.h"

struct struc1 {
    char a;
    int b;
    long c;
};

struct struc2 {
    int a:1;
    int b:2;
    int c:1;
};

struct struc3 {
    int a;
    struct {
        int b;
        long c;
    };
};

void test_struct() {
    struct struc1 a = {1,2,3};
    struct struc2 b = {1,2,3};
    struct struc3 c = {1,2,3};

    EXPECT_INT(a.a, 1);
    EXPECT_INT(a.b, 2);
    EXPECT_INT(a.c, 3);
    
    EXPECT_INT(b.a, 1);
    EXPECT_INT(b.b, 2);
    EXPECT_INT(b.c, 1);

    EXPECT_INT(c.a, 1);
    EXPECT_INT(c.b, 2);
    EXPECT_INT(c.c, 3);
}

int main() {
    test_struct();
    print_result(); 
}