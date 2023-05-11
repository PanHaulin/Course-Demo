#include <stdio.h>
#include <mpi.h>
#include "define.h"
#include<algorithm>

using namespace std;

int my_rank;     //进程标识
int num_process; //进程总数
const long long N = N_4G;

float rand_float(float s)
{
    return 4 * s * (1 - s);
}

void matrix_gen(float *a, uint64_t n, float seed)
{
    float s = seed;
    for (uint64_t i = 0; i < n; i++)
    {
        s = rand_float(s);
        a[i] = s;
    }
}

int cmpfunc(const void *a, const void *b)
{
    return (*(float *)a - *(float *)b);
}

int main(int argc, char *argv[])
{
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &num_process);

    // double start_time = MPI_Wtime();
    // 以二进制方式读取文件
    
    // FILE *fp = fopen(FLOAT_FILE, "rb");
    // fread(data, sizeof(float), N, fp);
    // double end_time = MPI_Wtime();
    // printf("file read time:%.4fs \n",(start_time - end_time));
    
    // 生成数组
    if(my_rank == 0){
        float *data = new float[N];
        matrix_gen(data, N, SEED);
        printf("data generation finish, number of gen: %lld \n", N);

        //qsort
        double start_time = MPI_Wtime();
        qsort(data, N, sizeof(float), cmpfunc);
        double end_time = MPI_Wtime();
        printf("qsort time:%.4fs \n", (end_time - start_time));

        delete[] data;
    }

    MPI_Finalize();
}
