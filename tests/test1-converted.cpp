#include <mpi.h>
#include <omp.h>
#include <stdio.h>

int world_rank;
int world_size;

void testOpenMPDirectives() {
  printf("Hello from thread!\n");
  int arr[20];
  for (int i = 0; i < 20; ++i) {
    arr[i] = i + 1;
  }
  int res[20];
  int global_start = 0;
  int global_end = 20;
  int iterations = global_end - global_start;
  int chunk = iterations / world_size;
  int start = global_start + world_rank * chunk;
  int end = (world_rank == world_size - 1) ? global_end : start + chunk;
  // Розподіл масиву arr між процесами
  int local_arr[chunk];
  MPI_Scatter(arr, chunk, MPI_INT, local_arr, chunk, MPI_INT, 0,
              MPI_COMM_WORLD);
  // Підготовка локального буфера для запису результатів у масив res
  int local_res[chunk];

  for (int i = start; i < end; ++i) {
    local_res[i - start] = local_arr[i - start] * i;
  }
  // Збір результатів з процесів у масив res
  MPI_Gather(local_res, chunk, MPI_INT, res, chunk, MPI_INT, 0, MPI_COMM_WORLD);

  //    int N = 10;
  // #pragma omp parallel for
  //    {
  //        for (int i = 0; i <= N; ++i) {
  //            printf("Iteration %d\n", i);
  //        }
  //    }
  //
  int prod = 0;
  global_start = 0;
  global_end = 11;
  iterations = global_end - global_start;
  chunk = iterations / world_size;
  start = global_start + world_rank * chunk;
  end = (world_rank == world_size - 1) ? global_end : start + chunk;
  /* Скалярна змінна prod модифікується в циклі.
     Розгляньте можливість використання MPI_Reduce для збору результатів. */

  int local_prod = 0;
  for (int i = start; i < end; ++i) {
    local_prod *= i;
  }
  MPI_Reduce(&local_prod, &prod, 1, MPI_INT, MPI_PROD, 0, MPI_COMM_WORLD);

  if (world_rank == 0) {
    printf("This is single section\n");
  }

  if (world_rank == 0) {
    printf("This is master section\n");
  }

  MPI_Barrier(MPI_COMM_WORLD);
}

int main() {
  MPI_Init(NULL, NULL);
  MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
  MPI_Comm_size(MPI_COMM_WORLD, &world_size);
  //     int main(int argc, char* argv[]) {
  testOpenMPDirectives();
  MPI_Finalize();
  return 0;
}