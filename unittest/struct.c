#include "unittest.h"

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

struct node {
    int val;
    struct node* left;
	struct node* right;
};

struct compound {
    struct {
        int a;
        int b;
    }x;
    struct {
        float a;
        float b;
    }y;
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

    struct node root, left, right;
    root.val = 1;
    left.val = 2;
    right.val = 3;
    root.left = &left;
    root.right = &right;
    EXPECT_INT(root.val, 1);
    EXPECT_INT(root.left->val, 2);
    EXPECT_INT(root.right->val, 3);

    struct compound cmpd = {{1,1},.y = {2.0,2.0}};
    EXPECT_INT(sizeof(cmpd), 16);
    EXPECT_INT(cmpd.x.a, 1);
    EXPECT_INT(cmpd.x.b, 1);
    EXPECT_DOUBLE(cmpd.y.a, 2.0);
    EXPECT_DOUBLE(cmpd.y.b, 2.0);
}

int main() {
    test_struct();
    print_result(); 
}