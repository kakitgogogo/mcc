#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main() {
	int a;
	srand((unsigned int)time(NULL));
	for(int i = 0; i < 100000000; ++i) {
		a = rand()+1;
	}
	// printf("%d\n", a);
}
