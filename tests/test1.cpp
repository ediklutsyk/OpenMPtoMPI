#include <omp.h>
#include <stdio.h>

void testOpenMPDirectives() {
    int prod = 1;
#pragma omp parallel for reduction(*:prod)
    for (int i = 1; i <= 10; ++i) {
        prod *= i;
    }
#pragma omp master
    {
        printf("Prod: %d\n", prod);
    }
}

//int main() {
int main(int argc, char *argv[]) {
    testOpenMPDirectives();
    return 0;
}