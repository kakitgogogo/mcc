#include "unittest.h"
#include <stdarg.h>

struct node {
    int val;
    struct node* left;
	struct node* right;
};

int func1() {
    return 1;
}

int func2(int a) {
    if(a < 0) return -1;
    if(a > 0) return 1;
    return 0;
}

static int factorial(int n) {
    if(n == 1) return 1;
    return n * factorial(n-1);
}

int eval_tree(struct node *root) {
    if(root == NULL) return 0;
    return root->val + eval_tree(root->left) + eval_tree(root->right);
}

void test_func() {
    int a = 1;
    EXPECT_INT(func1(), 1);

    EXPECT_INT(func2(-1), -1);
    EXPECT_INT(func2(1), 1);
    EXPECT_INT(func2(0), 0);

    EXPECT_INT(factorial(5), 120);

    struct node root = {1}, left = {2,NULL,NULL}, right = {3,NULL,NULL};
    root.left = &left;
    root.right = &right;
    EXPECT_INT(eval_tree(&root), 6);
}

void test_multiarg(
    float v01, int v02, float v03, int v04, float v05, int v06, float v07, int v08,
    float v09, int v10, float v11, int v12, float v13, int v14, float v15, int v16,
    float v17, int v18, float v19, int v20, float v21, int v22, float v23, int v24,
    float v25, int v26, float v27, int v28, float v29, int v30, float v31, int v32,
    float v33, int v34, float v35, int v36, float v37, int v38, float v39, int v40
){
    EXPECT_DOUBLE(v01, 1.0);
    EXPECT_DOUBLE(v02, 2.0);
    EXPECT_DOUBLE(v03, 3.0);
    EXPECT_DOUBLE(v04, 4.0);
    EXPECT_DOUBLE(v05, 5.0);
    EXPECT_DOUBLE(v06, 6.0);
    EXPECT_DOUBLE(v07, 7.0);
    EXPECT_DOUBLE(v08, 8.0);
    EXPECT_DOUBLE(v09, 9.0);
    EXPECT_DOUBLE(v10, 10.0);
    EXPECT_DOUBLE(v11, 11.0);
    EXPECT_DOUBLE(v12, 12.0);
    EXPECT_DOUBLE(v13, 13.0);
    EXPECT_DOUBLE(v14, 14.0);
    EXPECT_DOUBLE(v15, 15.0);
    EXPECT_DOUBLE(v16, 16.0);
    EXPECT_DOUBLE(v17, 17.0);
    EXPECT_DOUBLE(v18, 18.0);
    EXPECT_DOUBLE(v19, 19.0);
    EXPECT_DOUBLE(v20, 20.0);
    EXPECT_DOUBLE(v21, 21.0);
    EXPECT_DOUBLE(v22, 22.0);
    EXPECT_DOUBLE(v23, 23.0);
    EXPECT_DOUBLE(v24, 24.0);
    EXPECT_DOUBLE(v25, 25.0);
    EXPECT_DOUBLE(v26, 26.0);
    EXPECT_DOUBLE(v27, 27.0);
    EXPECT_DOUBLE(v28, 28.0);
    EXPECT_DOUBLE(v29, 29.0);
    EXPECT_DOUBLE(v30, 30.0);
    EXPECT_DOUBLE(v31, 31.0);
    EXPECT_DOUBLE(v32, 32.0);
    EXPECT_DOUBLE(v33, 33.0);
    EXPECT_DOUBLE(v34, 34.0);
    EXPECT_DOUBLE(v35, 35.0);
    EXPECT_DOUBLE(v36, 36.0);
    EXPECT_DOUBLE(v37, 37.0);
    EXPECT_DOUBLE(v38, 38.0);
    EXPECT_DOUBLE(v39, 39.0);
    EXPECT_DOUBLE(v40, 40.0);
}

void test_vararg(char* s, ...) {
    va_list ap;
    va_start(ap, s);
    EXPECT_STRING(va_arg(ap, char*), "1");
    EXPECT_INT(va_arg(ap, int), 2);
    EXPECT_INT(va_arg(ap, char), '3');
    EXPECT_DOUBLE(va_arg(ap, double), 4.0);
    EXPECT_INT(*va_arg(ap, int*), 5);
    va_end(ap);
}


int main() {
    test_func();
    test_multiarg(
        1.0,  2,  3.0,  4,  5.0,  6,  7.0,  8,  9.0,  10,
        11.0, 12, 13.0, 14, 15.0, 16, 17.0, 18, 19.0, 20,
        21.0, 22, 23.0, 24, 25.0, 26, 27.0, 28, 29.0, 30,
        31.0, 32, 33.0, 34, 35.0, 36, 37.0, 38, 39.0, 40
    );
    int a = 5;
    test_vararg("1",2,'3',4.0,&a);
    print_result(); 
}