#include <iostream>
#include <mpi.h>
#include "define.h"
#include <cmath>
#include <ctime>
#include <string.h>

using namespace std;

int my_rank;     //进程标识
int num_process; //进程总数
int n_part;      //每个核需要排序的元素个数
const int N = N_1M;

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

inline void swap(float *p, int i, int j)
{
    float tmp = p[i];
    p[i] = p[j];
    p[j] = tmp;
}

void odd_even_sort(float* my_data)
{
    bool sort = false;
    while (!sort)
    {
        // 若一次奇数排序，一次偶数排序，均未发生数据交换，则认为已排好序
        sort = true;
        for (int i = 1; i < n_part; i += 2)
        {
            // 奇排序
            if (my_data[i] < my_data[i - 1])
            {
                swap(my_data, i, i - 1);
                sort = false;
            }
        }

        for (int i = 2; i < n_part; i += 2)
        {
            // 偶排序
            if (my_data[i] < my_data[i - 1])
            {
                swap(my_data, i, i - 1);
                sort = false;
            }
        }
    }
}

void merge(float *my_data, float *right_data, float *tmp, bool &sort)
{
    // 完成两个数组的合并
    int i = 0, j = 0, k = 0;
    while (i != n_part && j != n_part)
    {
        if (my_data[i] <= right_data[j])
            tmp[k++] = my_data[i++];
        else
            tmp[k++] = right_data[j++];
    }
    if (j != 0)
        sort = false; //说明左右有元素交换，仍需要进行奇偶排序

    if (i != n_part)
    {
        while (i < n_part)
            tmp[k++] = my_data[i++];
    }

    if (j != n_part)
    {
        while (j < n_part)
            tmp[k++] = my_data[j++];
    }

    memcpy(my_data, tmp, sizeof(float) * n_part);
    memcpy(right_data, tmp + n_part, sizeof(float) * n_part);
}

void parallel_odd_even_sort(float *my_data)
{
    MPI_Status status;
    int left = my_rank - 1;
    int right = my_rank + 1;

    // 边界处理，虚拟进程，不会做任何事
    if (left < 0)
        left = MPI_PROC_NULL;
    if (right > num_process)
        right = MPI_PROC_NULL;

    float *right_data = new float[n_part]{0};
    float *tmp = new float[2 * n_part]{0};
    bool sort = false;
    bool flag = false;

    while (!flag)
    {
        sort = true;
        // 偶排序(偶数传给左边的奇数)
        if (my_rank % 2 == 0)
        {
            MPI_Send(my_data, n_part, MPI_FLOAT, left, 0, MPI_COMM_WORLD);
            MPI_Recv(my_data, n_part, MPI_FLOAT, left, 0, MPI_COMM_WORLD, &status);
        }
        else if (right != MPI_PROC_NULL)
        {
            // 只有右边不为空，才能收到消息
            MPI_Recv(right_data, n_part, MPI_FLOAT, right, 0, MPI_COMM_WORLD, &status);
            merge(my_data, right_data, tmp, sort);
            MPI_Send(right_data, n_part, MPI_FLOAT, right, 0, MPI_COMM_WORLD); //大数回传
        }
        // else，本身是奇数，右边为空，不做任何事

        MPI_Barrier(MPI_COMM_WORLD); //阻塞，将奇偶排序分开

        if (my_rank % 2 == 1)
        {
            MPI_Send(my_data, n_part, MPI_FLOAT, left, 0, MPI_COMM_WORLD);
            MPI_Recv(my_data, n_part, MPI_FLOAT, left, 0, MPI_COMM_WORLD, &status);
        }
        else if (right != MPI_PROC_NULL)
        {
            MPI_Recv(right_data, n_part, MPI_FLOAT, right, 0, MPI_COMM_WORLD, &status);
            merge(my_data, right_data, tmp, sort);
            MPI_Send(right_data, n_part, MPI_FLOAT, right, 0, MPI_COMM_WORLD);
        }

        //flag是每个进程sort标识的逻辑与(MPI_LAND)，只有当所有进程的sort都为true（即一轮奇偶排序未交换元素）才停止。
        MPI_Allreduce(&sort, &flag, 1, MPI_C_BOOL, MPI_LAND, MPI_COMM_WORLD);
    }

    delete[] tmp;
    delete[] right_data;
}

int main(int argc, char *argv[])
{
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &num_process);
    int master_node = 0;
    float *all_data;
    double start_time, end_time;

    // 各进程
    n_part = N / num_process;
    float *my_data = new float[n_part]{0};

    // 生成浮点数
    if(my_rank==master_node)
        all_data = new float[N];
        matrix_gen(all_data, N, SEED);
        printf("data generation finish, number of gen: %d \n", N); 

    // 广播数据
    MPI_Scatter(all_data, n_part, MPI_FLOAT, my_data, n_part, MPI_FLOAT, master_node, MPI_COMM_WORLD);
    printf("data scatter finish");

    start_time = MPI_Wtime(); //并行计时

    // 串行奇偶排序，保证各自数据有序
    odd_even_sort(my_data);
    printf("serial sorted finish");

    // 并行奇偶排序
    parallel_odd_even_sort(my_data);
    printf("parallel sort finish");

    end_time = MPI_Wtime(); //结束计时
    // 聚合数据
    // sendbuf, sendcount, sendtype, recvbuf, recvcount, recvtype, root, comm
    if(my_rank == 0)
        all_data = new float[N];
    MPI_Gather(my_data, n_part, MPI_FLOAT, all_data, n_part, MPI_FLOAT, master_node, MPI_COMM_WORLD);

    if (my_rank == master_node)
    {
        bool sorted = true;
        for (int i = 0; i < N - 1; i++)
        {
            if (all_data[i] < all_data[i + 1])
            {
                sorted = false;
                break;
            }
        }
        if (sorted)
            cout << "sort successfully" << endl;
        else
            cout << "sort failed" << endl;

    }

    cout << "number of process: " << num_process<<","<< "time used: " << end_time - start_time << "s" << endl;

    // 最后处理
    delete[] my_data;
    if(my_rank == master_node)
        delete[] all_data;
    MPI_Finalize();
    return 0;
}