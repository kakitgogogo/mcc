#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int test_count = 0;
int test_pass = 0;

#define EXPECT_BASE(equality, expect, actual, format) \
    do {\
        test_count++;\
        if (equality)\
            test_pass++;\
        else {\
            fprintf(stderr, "%s:%d: expect: " format " actual: " format "\n", __BASE_FILE__, __LINE__, expect, actual);\
        }\
    } while(0)

#define EXPECT_INT(expect, actual) EXPECT_BASE((expect) == (actual), expect, actual, "%lld")
#define EXPECT_DOUBLE(expect, actual) EXPECT_BASE((expect) == (actual), expect, actual, "%.17g")
#define EXPECT_STRING(expect, actual) \
    EXPECT_BASE(strcmp(expect, actual) == 0, expect, actual, "%s")
#define EXPECT_TRUE(actual) EXPECT_BASE((actual) != 0, "true", "false", "%s")
#define EXPECT_FALSE(actual) EXPECT_BASE((actual) == 0, "false", "true", "%s")

#define print_result() printf("%s: %d/%d passed\n", __BASE_FILE__, test_pass, test_count);