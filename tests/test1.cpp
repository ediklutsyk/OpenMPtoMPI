#include <omp.h>
#include <stdio.h>

void testOpenMPDirectives() {
#pragma omp parallel
    {
        printf("Hello from thread!\n");
    }
    int arr[20];
    for (int i = 0; i < 20; ++i) {
        arr[i] = i + 1;
    }
    int res[20];
#pragma omp parallel for
    for (int i = 0; i < 20; ++i) {
        res[i] = arr[i] * i;
    }

    //    int N = 10;
    // #pragma omp parallel for
    //    {
    //        for (int i = 0; i <= N; ++i) {
    //            printf("Iteration %d\n", i);
    //        }
    //    }
    //
    int prod = 0;
#pragma omp parallel for reduction(*:prod)
    for (int i = 0; i < 11; ++i) {
        prod *= i;
    }

#pragma omp single
    {
        printf("This is single section\n");
    }

#pragma omp master
    {
        printf("This is master section\n");
    }

#pragma omp barrier
}

int main() {
//     int main(int argc, char* argv[]) {
    testOpenMPDirectives();
    return 0;
}