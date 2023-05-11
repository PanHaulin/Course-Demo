#include <stdio.h>
#include <stdint.h>
#include<iostream>
#include<fstream>
#include "define.h"
// #include <mpi.h>

using namespace std;

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
// int my_rank;     //进程标识
// int num_process; //进程总数
const int N = N_1G;

int main(int argc, char *argv[])
{
    // MPI_Init(&argc, &argv);
    // MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
    // MPI_Comm_size(MPI_COMM_WORLD, &num_process);

    // double start_time = MPI_Wtime();
    //创建浮点数组并生成浮点数
    float *data = new float[N];
    matrix_gen(data, N, SEED);
    // double end_time = MPI_Wtime();
    // printf("float generate time:%.4fs \n", (end_time - start_time));
    printf("data generate finish");

    ofstream fout(FLOAT_FILE, ios::out | ios::binary);
    fout.write((char*)&data, N*sizeof(float));



    // start_time = MPI_Wtime();
    // 以二进制方式写入文件
    // FILE *fp = fopen(FLOAT_FILE, "wb+");
    // fwrite(data, sizeof(float), N, fp);
    // fclose(fp);
    // end_time = MPI_Wtime();
    // printf("file write time:%.4fs \n", (end_time - start_time));
    printf("file write finish");

    delete[] data;
    // MPI_Finalize();
}