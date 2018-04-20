#include <stdio.h>

struct node {
    int val;
    struct node* left;
	struct node* right;
};

int get_node_val(struct node *root) {
    return root->val;
}

int eval_tree(struct node* root) {
    if(root == NULL) return 0;
    return root->val + eval_tree(root->left) + eval_tree(root->right);
}

int main() {
    struct node root = {1}, left = {2,NULL,NULL}, right = {3,NULL,NULL};
    root.left = &left;
    root.right = &right;
    printf("%d\n", get_node_val(&root));
    printf("%d\n", eval_tree(&root));
}