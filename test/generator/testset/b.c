int main() {
    int x = ({ int a = 3; int b = 2; a+b; });
    print_int(x);
}
