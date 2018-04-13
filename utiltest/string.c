#include "utiltest.h"

void test_string() {
    char* a = "a\n";
    EXPECT_STRING(a, "a\n");
    EXPECT_STRING("xxx", u8"xxx");
    EXPECT_INT("abc"[0], 'a');
    EXPECT_INT("abc"[3], 0);
    EXPECT_STRING("abcd", "ab" "cd");
    EXPECT_STRING("abcdef", "ab" "cd" "ef");
    EXPECT_STRING("哈哈哈", "\u54c8\u54c8\u54c8");
}

int main() {
    test_string();
    print_result(); 
}