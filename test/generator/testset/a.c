int f(int n) {
    return n*n;
}
struct struc {
    int a:3;
    int b:3;
    int c;
};

long long a = 1;
int b[2] = { 1, 2 };
int c = {1};
int (*fp)(int) = f;

int* g = (double*)&a;

struct struc h = { 1, 1 };

int* i = &c;

char* gstr = (char*)"xxx";

int* j = &h.c + 3;

struct struc k = (struct struc){1, 2};

int* l = &b[1];

void print_int(int n);
void print_str(char* s);

int main() {
    int a = 1;
    long b = 2;
    int c = a + b;
    short d = a > 1 ? d : b;

    print_int(c);
    print_int(d);
    print_int(a + b > 3 ? a : b);

    struct struc e = { .b = 1, .a = 8, .b = 3 };
    print_int(e.b);

    char* str = "哈哈";
    sizeof(str);
    print_str(str);

    print_str(gstr);

    print_int(fp(2));

    print_int(k.b);
    print_int(*l);

    
}