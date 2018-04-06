#define f(x) g(x)
#define g(x) f(x)

f(1)

#define STR2(x) #x
#define STR(x) STR2(x)

STR2(__LINE__)
STR(__LINE__)

__FILE__
__DATE__
__TIME__
__TIMESTAMP__
__BASE_FILE__
__INCLUDE_LEVEL__