#include <stdio.h>
#include <stdlib.h>

typedef struct floatstr floatstr;

struct floatstr {
    char* str;
    double val;
};

int main() {
    floatstr* fs = malloc(sizeof(floatstr));
    char* end;
    fs->str = "3.14";
    fs->val = strtod(fs->str, &end);
    printf("%s %f\n", fs->str, fs->val);
}
