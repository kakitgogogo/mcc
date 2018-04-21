#include "unittest.h"

void test_string() {
    char* a = "a\n";
    EXPECT_STRING(a, "a\n");
    EXPECT_STRING("xxx", u8"xxx");
    EXPECT_INT("abc"[0], 'a');
    EXPECT_INT("abc"[3], 0);
    EXPECT_STRING("abcd", "ab" "cd");
    EXPECT_STRING("abcdef", "ab" "cd" "ef");
    EXPECT_STRING("哈哈哈", "\u54c8\u54c8\u54c8");

    EXPECT_STRING("\x24", "\u0024");         /* Dollar sign U+0024 */
    EXPECT_STRING("\xC2\xA2", "\u00A2");     /* Cents sign U+00A2 */
    EXPECT_STRING("\xE2\x82\xAC", "\u20AC"); /* Euro sign U+20AC */
}

int main() {
    test_string();
    print_result(); 
}