#include <stdio.h>

#define N 10


int split(int a[], int low, int high) 
{
    int part_element = a[low];

    for (;;) {
        while (low < high && part_element <= a[high]) {
            high--;
        }
        if (low >= high) break;
        a[low++] = a[high];

        while (low < high && a[low] <= part_element) {
            low++;
        }
        if (low >= high) break;
        a[high--] = a[low];
    }

    a[high] = part_element;
    return high;
}

void quicksort(int a[], int low, int high)
{
    int middle;

    if (low >= high) return;
    middle = split(a, low, high);
    quicksort(a, low, (middle-1));
    quicksort(a, (middle+1), high);
}


int main(void)
{
    int a[10] = {4,5,6,9,8,7,0,2,1,3}, i;

    quicksort(a, 0, 9);

    printf("In sorted order: ");
    for (i = 0; i < 10; i++) {
        printf("%d ", a[i]);
    }
    printf("\n");

    return 0;
}

