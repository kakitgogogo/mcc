
#include <stdio.h>
#include <stdlib.h>

enum types {
	INT,
	FLOAT,
};

int func() { return 3; }

int main() {
	typedef int arr[2];
	arr x = {1, 2};
	int b = func();
	printf("%d\n", x[0] + x[1] + b);
	for(int i = 0; i < 10; ++i) {
		printf("%d ", i);
	}
	printf("\n");
	char* str = (char*)malloc(11 * sizeof(char));
	for(int i = 0; i < 11; ++i) str[i] = '1';
	str[10] = '\0';
	printf("%s\n", str);
#ifdef RETURN_ZERO
	return 0;
#else
	return b;
#endif
}
