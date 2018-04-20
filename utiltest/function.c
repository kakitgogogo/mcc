#include "utiltest.h"

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

int main() {
    test_func();
    print_result(); 
}