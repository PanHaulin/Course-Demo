#include <iostream>
#include <mpi.h>
#include <algorithm>
#include <cmath>
#include <queue>
#include <numeric>
#include <string>
#include "define.h"

using namespace std;

int my_rank;     //进程标识
int num_process; //进程总数
int my_length;   //每个核需要排序的元素个数
const int N = N_256M;

struct mmdata
{
    //待归并数组序号
    int stindex;
    //在数组中的序号
    int index;
    float stvalue;
    mmdata(int st = 0, int id = 0, float stv = 0.0) : stindex(st), index(id), stvalue(stv){};
    bool operator<(const mmdata &a) const
    {
        return stvalue > a.stvalue;
    }
};

void merge(float *starts[], const int lengths[], const int number, float merge_array[], const int merge_array_length)
{
    priority_queue<mmdata> priorities;

    //将每个待归并数组的第一个数加入优先队列，同时保存其待归并序号和数字在数组中的序号
    for (int i = 0; i < number; i++)
    {
        if (lengths[i] > 0)
            priorities.push(mmdata(i, 0, starts[i][0]));
    }

    int merge_array_index = 0;
    while (!priorities.empty() && merge_array_index < merge_array_length)
    {
        //取最小的数
        mmdata xxx = priorities.top();
        priorities.pop();

        //加入到归并数组中
        merge_array[merge_array_index++] = starts[xxx.stindex][xxx.index];

        // 如果被拿出的数据不是所在归并数组的最后一个元素，将下一个元素push进优先队列
        if (lengths[xxx.stindex] > xxx.index + 1)
            priorities.push(mmdata(xxx.stindex, xxx.index + 1, starts[xxx.stindex][xxx.index + 1]));
    }
}

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

int main(int argc, char *argv[])
{
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &num_process);
    int master_node = 0;
    double start_time, end_time;
    // FILE *fp;
    float* all_data;


    // 各进程读取数据
    
    
    // fp = fopen(FLOAT_FILE, "rb");
    // fseek(fp, my_rank * my_length * sizeof(float), 0);
    // fread(my_data, sizeof(float), my_length, fp);
    // fp.close();

    // 生成浮点数
    if(my_rank == master_node){
        all_data = new float[N];
        matrix_gen(all_data, N, SEED);
        printf("data generation finish, number of gen: %d \n", N); 
    }


    // 广播数据
    my_length = N / num_process;
    float *my_data = new float[my_length];
    MPI_Scatter(all_data, my_length, MPI_FLOAT, my_data, my_length, MPI_FLOAT, master_node, MPI_COMM_WORLD);
    if(my_rank==master_node)
        printf("scatter finish\n");
    // 开始计时
    start_time = MPI_Wtime();


    // 局部排序
    sort(my_data, my_data + my_length);

    // 获取regular samples，每个进程获得num_progress个
    float *regular_samples = new float[num_process]{0};
    for (int index = 0; index < num_process; index++)
        regular_samples[index] = my_data[(index * my_length) / num_process];

    // 0号进程接受所有的regular samples
    float *gather_regular_samples;
    if (my_rank == 0)
        gather_regular_samples = new float[num_process * num_process];

    // sendbuf, sendcount, sendtype, recvbuf, recvcount, recvtype, root, comm
    MPI_Gather(regular_samples, num_process, MPI_FLOAT, gather_regular_samples, num_process, MPI_FLOAT, 0, MPI_COMM_WORLD);

    // 0号进程对各进程发来的regular_samples进行归并排序，并抽出num_progress-1个作为privot
    float *privots = new float[num_process - 1];
    if (my_rank == 0)
    {
        // 将gather_regular_samples转为二维数组后做归并
        // start用于存储各进程regular_samples的下标，即二维数组
        float **starts = new float *[num_process];
        int *lengths = new int[num_process];
        for (int i = 0; i < num_process; i++)
        {
            starts[i] = &gather_regular_samples[i * num_process];
            lengths[i] = num_process;
        }
        // 各进程的regular_samples已经有序，直接归并
        float *sorted_gat_reg_samples = new float[num_process * num_process];
        merge(starts, lengths, num_process, sorted_gat_reg_samples, num_process * num_process);

        //抽出主元
        for (int i = 0; i < num_process - 1; i++)
            privots[i] = sorted_gat_reg_samples[(i + 1) * num_process];

        delete[] starts;
        delete[] lengths;
        delete[] sorted_gat_reg_samples;
    }

    // 广播主元
    MPI_Bcast(privots, num_process - 1, MPI_FLOAT, 0, MPI_COMM_WORLD);

    // 各进程按照主元分段
    // part_start_index保存每段开始的下标，part_length保存每段长度
    int *part_start_index = new int[num_process];
    int *part_length = new int[num_process];

    int data_index = 0;
    for(int part_index=0; part_index<num_process-1; part_index++){
        part_start_index[part_index] = data_index;
        part_length[part_index] = 0;

        while((data_index<my_length) && (my_data[data_index] <= privots[part_index])){
            // 统计该段长度，并找到下一段起始
            ++data_index;
            ++part_length[part_index];
        }
    }
    part_start_index[num_process-1] = data_index;
    part_length[num_process-1] = my_length-data_index;

    // 全局交换
    // 将自身每段的数量告诉别的进程
    int *recv_rank_part_len = new int[num_process];
    // sendbuff, sendcount, sendtype, recvbuf, recvcount, recvtype, comm
    MPI_Alltoall(part_length, 1, MPI_INT, recv_rank_part_len, 1, MPI_INT, MPI_COMM_WORLD);

    // 进程i收集第i段数据（往不同结点发送的数据量不一样）
    // 计算接收数据量和，初始化设置每个结点发送数据需要存放的位置
    int rank_part_len_sum = 0;
    int *rank_part_start = new int[num_process];
    for(int i=0; i<num_process; i++){
        rank_part_start[i] = rank_part_len_sum;
        rank_part_len_sum += recv_rank_part_len[i];
    }
    // 接收各进程第i段数据
    // sendbuf, sendcounts， sdispls(相对于缓冲区偏移量), sendtype, recvbuf, recvcounts, rdispls, recvtype, comm
    float *recv_part_data = new float[rank_part_len_sum];
    MPI_Alltoallv(my_data, part_length, part_start_index, MPI_FLOAT, recv_part_data, recv_rank_part_len, rank_part_start, MPI_FLOAT, MPI_COMM_WORLD);

    // 归并收到的num_progress个数据段（包括自己给自己发送的）
    float ** mul_part_start = new float* [num_process];
    for(int i=0; i<num_process; i++)
        mul_part_start[i] = &recv_part_data[rank_part_start[i]];
    
    //直接归并
    float *sorted_res = new float[rank_part_len_sum];
    merge(mul_part_start, recv_rank_part_len, num_process, sorted_res, rank_part_len_sum);

    end_time = MPI_Wtime();
    if(my_rank==master_node)
        printf("psrs finish\n");
    // 将全局有序的数据收集到主进程
    // if(my_rank == 0)
    //     all_data = new float[N];
    MPI_Gather(sorted_res, rank_part_len_sum, MPI_FLOAT, all_data, rank_part_len_sum, MPI_FLOAT, master_node, MPI_COMM_WORLD);

    if(my_rank==master_node)
        printf("gather finish\n");
    if (my_rank == master_node){
        bool sorted = true;
        for(int i=0; i<N-1; i++){
            if(all_data[i] < all_data[i+1]){
                sorted = false;
                break;
            }
        }
        if(sorted)
            cout<<"sort successfully"<<endl;
        else
            cout<<"sort failed"<<endl;
        
        delete[] all_data;
    }
    cout << "number of process: " << num_process<<","<< "time used: " << end_time - start_time << "s" << endl;

    // 最后处理
    delete[] my_data;
    delete[] regular_samples;
    if (my_rank == 0)
        delete[] gather_regular_samples;
    delete[] part_start_index;
    delete[] part_length;
    delete[] recv_rank_part_len;
    delete[] rank_part_start;
    delete[] recv_part_data;
    delete[] mul_part_start;
    delete[] sorted_res;
    MPI_Finalize();
    return 0;
}