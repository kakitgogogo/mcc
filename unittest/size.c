#include "unittest.h"

void test_builtin_size() {
    EXPECT_INT(sizeof(char), 1);
    EXPECT_INT(sizeof(_Bool), 1);
    EXPECT_INT(sizeof(short), 2);
    EXPECT_INT(sizeof(int), 4);
    EXPECT_INT(sizeof(unsigned int), 4);
    EXPECT_INT(sizeof(long long), 8);
    EXPECT_INT(sizeof(long), 8);

    EXPECT_INT(sizeof(float), 4);
    EXPECT_INT(sizeof(double), 8);
    EXPECT_INT(sizeof(long double), 8);

    EXPECT_INT(sizeof(float*), 8);
}

struct struc1 {
    char a;
    int b;
    long c;
};

struct struc2 {
    int a:1;
    int b:2;
};

struct struc3 {
    int a;
    struct {
        int b;
        long c;
    };
};

#define key_type 	unsigned int

struct node
{
	int color;
	key_type key;
	struct node* left;
	struct node* right;
	struct node* parent;
};

struct item
{
	int val;
	struct node node;
};

void test_struct_size() {
    EXPECT_INT(sizeof(struct struc1), 16);
    EXPECT_INT(sizeof(struct struc2), 4);
    EXPECT_INT(sizeof(struct struc3), 24);
    EXPECT_INT(sizeof(struct node), 32);
    struct item i;
    EXPECT_INT(sizeof(i), 40);
}

union union1 {
    char a;
    int b;
    long c;
};

void test_union_size() {
    EXPECT_INT(sizeof(union union1), 8);
}

void test_array_size() {
    int a[] = {1,2,3};
    EXPECT_INT(sizeof(a), 12);
}

int main() {
    test_builtin_size();
    test_struct_size();
    test_union_size();
    print_result(); 
}