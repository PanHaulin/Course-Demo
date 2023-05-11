#include <iostream>
#include <mpi.h>
#include <fstream>
#include <cstdlib>
#include <algorithm>
#include <cmath>
#include <queue>
#include <numeric>
#include <string>
#include "define.h"
using namespace std;

#define BLOCK_LOW(my_rank, comm_sz, T) ((my_rank)*(T)/(comm_sz))
#define BLOCK_HIGH(my_rank, comm_sz, T) (BLOCK_LOW(my_rank+1,comm_sz,T)-1)
#define BLOCK_SIZE(my_rank, comm_sz, T) (BLOCK_HIGH(my_rank,comm_sz,T) - BLOCK_LOW(my_rank,comm_sz,T) + 1)
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

struct mmdata {
    // 待归并数组序号
    int stindex;
    // 在数组中的序号
    int index;
    float stvalue;
    mmdata(int st=0, int id=0, float stv = 0):stindex(st),index(id),stvalue(stv){}
};

bool operator<( const mmdata & One, const mmdata & Two) {
    return One.stvalue > Two.stvalue;
}

// 各进程regularSamples二维数组，各进程regularSamples长度，待归并数组数量，结果数组，待归并总数据量
void multiple_merge(float* starts[], const int lengths[], const int Number, float newArray[], const int newArrayLength) {
    priority_queue< mmdata> priorities;

    // 将每个待归并数组的第一个数加入优先队列，同时保存它所在待归并数组序号和数字在数组中的序号
    for(int i=0; i<Number;i++) {
        if (lengths[i]>0) {
            priorities.push(mmdata(i,0,starts[i][0]));
        }
    }

    int newArrayindex = 0;
    while (!priorities.empty() && (newArrayindex<newArrayLength)) {
        // 拿下最小的数据
        mmdata xxx = priorities.top();
        priorities.pop();

        // 将拿下的数据加入到结果数组中
        newArray[newArrayindex++] = starts[xxx.stindex][xxx.index];

        // 如果被拿下数据不是所在的待归并数组的最后一个元素，将下一个元素push进优先队列
        if ( lengths[xxx.stindex]>(xxx.index+1)) {
            priorities.push(mmdata(xxx.stindex, xxx.index+1, starts[xxx.stindex][xxx.index+1]));
        }
    }
}

int main(int argc,char* argv[]) {
    int comm_sz, my_rank;

    MPI_Init(&argc,&argv);
    MPI_Comm_size(MPI_COMM_WORLD, &comm_sz);
    MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);

    // 1. 需要排序数据总数量的是2的指数，和文件指针
    // int power = strtol(argv[1], NULL, 10);
    unsigned long dataLength = N_1G;
    // float allDataLength;
    // ifstream fin(argv[2], ios::binary);

    // 2. 各进程获得自己开始读取数据的位置，是根据power而不是dataLength判断
    unsigned long myDataStart = BLOCK_LOW(my_rank, comm_sz, dataLength);
    unsigned long myDataLength = BLOCK_SIZE(my_rank, comm_sz, dataLength);
    // fin.seekg((myDataStart+1)*sizeof(float), ios::beg);

    // 3. 各进程获取数据
    float *myData = new float[myDataLength];
    // for(float i=0; i<myDataLength; i++)
    //     fin.read((char*)&myData[i], sizeof(float));
    // fin.close();

    // 3.1 主进程读取文件并分发
    float *all_data;
    if(my_rank == 0){
        all_data = new float[dataLength];
        matrix_gen(all_data, dataLength, SEED);
    }
    MPI_Scatter(all_data, myDataLength, MPI_FLOAT, myData, myDataLength, MPI_FLOAT, 0, MPI_COMM_WORLD);

    if(my_rank ==0){
        delete[] all_data;
    }

    // 记录T_p
    double startTime, endTime;
    startTime = MPI_Wtime();
    // 4. 各进程排序自己的数据
    sort(myData, myData+myDataLength);

    // 5. 获取Regular samples，每个进程抽取comm_sz个
    float *regularSamples = new float[comm_sz];
    for(int index=0; index<comm_sz; index++)
        regularSamples[index] = myData[(index*myDataLength)/comm_sz];
    
    // 6. 0号进程接收所有regularSamples，共有comm_sz*comm_sz个
    float *gatherRegSam;
    if(my_rank == 0)
        gatherRegSam = new float[comm_sz*comm_sz];
    // sendbuf, sendcount, sendtype, recvbuf, recvcount, recvtype, root, comm
    MPI_Gather(regularSamples, comm_sz, MPI_FLOAT, gatherRegSam, comm_sz, MPI_FLOAT, 0, MPI_COMM_WORLD);

    // 7. 0号进程对各进程发来的regularSamples进行归并排序，并抽出comm_sz-1个数做privot
    float *privots = new float[comm_sz];
    if(my_rank == 0) {
        // start用于存储gatherRegSam中各进程RegularSamples开始下标，相当于二维数组
        float **starts = new float* [comm_sz];
        // gatherRegSam中各进程RegularSamples长度，都一样是comm_sz
        int *lengths = new int[comm_sz];
        for(int i=0; i<comm_sz; i++) {
            starts[i] = &gatherRegSam[i*comm_sz];
            lengths[i] = comm_sz;
        }
        
        // 因为各进程的的ragularSamples就是有序的，因此只需要将gatherRegSam中的各进程数据归并即可
        float *sortedGatRegSam = new float[comm_sz*comm_sz];
        multiple_merge(starts, lengths, comm_sz, sortedGatRegSam, comm_sz*comm_sz);

        // 抽出主元
        for(int i=0; i<comm_sz-1; i++)
            privots[i] = sortedGatRegSam[(i+1)*comm_sz];

        delete []starts;
        delete []lengths;
        delete []sortedGatRegSam;
    }

    // 8.广播主元
    MPI_Bcast(privots, comm_sz-1, MPI_FLOAT, 0, MPI_COMM_WORLD);

    // 9.各进程将自己的数据按照主元分段，partStartIndex保存每段开始下标
    // 一个进程同样有溢出风险
    int *partStartIndex = new int[comm_sz];
    // 只有一个进程的时候int有溢出风险，因为有2^32个数。如果有两个进程，除非有一段长度是(2^31)-1也不会溢出
    int *partLength = new int[comm_sz];

    unsigned long dataIndex = 0;
    for(int partIndex = 0; partIndex<comm_sz-1; partIndex++) {
        partStartIndex[partIndex] = dataIndex;
        partLength[partIndex] = 0;

        while((dataIndex<myDataLength) && (myData[dataIndex]<=privots[partIndex])) {
            dataIndex++;
            partLength[partIndex]++;
        }
    }
    // 最后一段数据补齐（主元会溢出）
    partStartIndex[comm_sz-1] = dataIndex;
    partLength[comm_sz-1] = myDataLength - dataIndex;

    // 9.ALLTOALL操作，进程i知道所有进程第i段的长度 
    // 溢出是指元素超过int范围
    int *recvRankPartLen = new int[comm_sz];
    MPI_Alltoall(partLength, 1, MPI_INT, recvRankPartLen, 1, MPI_INT, MPI_COMM_WORLD);

    // 10. ALLTOALLV操作，进程i收集所有进程第i段数据
    // 计算接收总数据两和象征性设置开始位置离接收位置偏移量
    // 两个进程溢出风险很大，因此可能尽量杜绝两个进程运算，线性运算也没辙
    int rankPartLenSum = 0;
    int *rankPartStart = new int[comm_sz];
    for(int i=0; i<comm_sz; i++) {
        rankPartStart[i] = rankPartLenSum;
        rankPartLenSum += recvRankPartLen[i];
    }
    // 接收各进程i段的数组
    float *recvPartData = new float[rankPartLenSum];
    MPI_Alltoallv(myData, partLength, partStartIndex, MPI_FLOAT,
                    recvPartData, recvRankPartLen, rankPartStart, MPI_FLOAT, MPI_COMM_WORLD);

    // 11.归并comm_sz个段
    // 创造二维数组
    float **mulPartStart = new float*[comm_sz];
    for(int i=0; i<comm_sz; i++)
        mulPartStart[i] = &recvPartData[rankPartStart[i]];

    // 结果数组
    float *sortedRes = new float[rankPartLenSum];
    multiple_merge(mulPartStart, recvRankPartLen, comm_sz, sortedRes, rankPartLenSum);

    endTime = MPI_Wtime();

    // 12.各进程判断自己的数据是否有序
    bool sorted = true;
    for(int i=0; i<rankPartLenSum-1; i++)
        if(!(sortedRes[i] <= sortedRes[i+1])) {
            sorted = false;
            break;
        }
    string state = sorted ? "success" : "fail";
    cout << "rank " << my_rank << " sort " << state << "!!"<<" time:"<<endTime-startTime<< endl;

    // 13. 聚集
    if(my_rank == 0)
        all_data = new float[rankPartLenSum*8];
    MPI_Gather(sortedRes, rankPartLenSum, MPI_FLOAT, all_data, rankPartLenSum, MPI_FLOAT, 0, MPI_COMM_WORLD);


    if(my_rank == 0) {
        ofstream fout(argv[3], ios::app);
        fout << "processors:" << comm_sz;
        fout << " Tp:" << endTime-startTime << endl;
    }

    // 最后处理
    delete []myData;
    delete []regularSamples;
    if(my_rank==0)
        delete []gatherRegSam;
    delete []partStartIndex;
    delete []partLength;
    delete []recvRankPartLen;
    delete []rankPartStart;
    delete []recvPartData;
    delete []mulPartStart;
    delete []sortedRes;
    MPI_Finalize();
    
    return 0;
}