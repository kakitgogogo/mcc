#include <stdio.h>

struct pair {
    float a;
    float b;
};

typedef enum { LEPT_NULL, LEPT_FALSE, LEPT_TRUE, LEPT_NUMBER, LEPT_STRING, LEPT_ARRAY, LEPT_OBJECT } lept_type;

typedef struct lept_value lept_value;
typedef struct lept_member lept_member;

struct lept_value {
    union {
        struct { lept_member* m; size_t size; }o;   /* object: members, member count */
        struct { lept_value* e; size_t size; }a;    /* array:  elements, element count */
        struct { char* s; size_t len; }s;           /* string: null-terminated string, string length */
        double n;                                   /* number */
    }u;
    lept_type type;
};

struct lept_member {
    char* k; size_t klen;   /* member key string, key string length */
    lept_value v;           /* member value */
};

enum
  {
    FP_NAN =
# define FP_NAN 0
      FP_NAN,
    FP_INFINITE =
# define FP_INFINITE 1
      FP_INFINITE,
    FP_ZERO =
# define FP_ZERO 2
      FP_ZERO,
    FP_SUBNORMAL =
# define FP_SUBNORMAL 3
      FP_SUBNORMAL,
    FP_NORMAL =
# define FP_NORMAL 4
      FP_NORMAL
  };

int main() {
    struct pair d = {2.0, 2.0};
    printf("%f\n", d.a);

    lept_member lm;
    printf("%d\n", sizeof(lept_value));
    printf("%d\n", sizeof(lept_member));
    printf("%d\n", sizeof(lm.v));
    lept_value lv;
    lv.type = LEPT_ARRAY;

    for(;;) {
        break;
    }
    for(int i=0;;) {
        break;
    }
    for(int i=0;;++i) {
        break;
    }
}
