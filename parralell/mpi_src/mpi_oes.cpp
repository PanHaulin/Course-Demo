#include<iostream>
#include"mpi.h"
#include<ctime>
#include<cmath>
#include"define.h"
#include<string.h>

using namespace std;
 
int myid, numprocs;//进程标识，进程总数
int part;//每个核心需排序元素个数
double start, finish;//计算时间
const int N = N_1M;//待排元素个数
const int INF = 0X7FFFFFFF;//设置一个无穷大值
 
void odd_even_sort(float *p,int num);//顺序执行的奇偶排序
void swap(float *p, int i, int j);//交换算法
void mergeSort(float *data, float *buffer, float *tmp, bool &sort);//两块排好序的元素进行排序合并
void parallelMergeSort(float *data);//奇偶排序式的 融合排序算法
 
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
	MPI_Comm_rank(MPI_COMM_WORLD, &myid);
	MPI_Comm_size(MPI_COMM_WORLD, &numprocs);
	int masterNode = 0;
	float *memoryPool = nullptr;
	
	if (myid == masterNode)
	{
		start = MPI_Wtime();
		// int padding = numprocs - N % numprocs;//若不能整除则需加上padding个无穷大的元素使能够整除
		
		memoryPool = new float[N];
        matrix_gen(memoryPool, N, SEED);
        finish = MPI_Wtime();
        cout<<"数据生成时间 "<<finish-start<<endl;
        part = N/numprocs;
		// srand(time(NULL));
		// for (int i = 0; i < N; ++i)
		// 	memoryPool[i] = (rand() % 1000)/10.0;
		
		// start = MPI_Wtime();//串行开始计时
		// /*for (int i = 0; i < N; ++i)
		// 	cout << memoryPool[i] << endl;*/
		// if (numprocs == 1)
		// {
		// 	odd_even_sort(memoryPool, N);
		// 	finish = MPI_Wtime();//串行停止计时
		// 	cout <<"串行时间"<< finish - start << endl;
		// 	MPI_Finalize();
		// 	return 0;
		// }
		// for (int i = 0; i < padding; ++i)
		// 	memoryPool[N + i] = INF;
		// part = (N + padding) / numprocs;
	}

	MPI_Bcast(&part, 1, MPI_INT, masterNode, MPI_COMM_WORLD);
	float *data = new float[part];
 
	//主进程向各进程播撒数据
	MPI_Scatter(memoryPool, part, MPI_FLOAT, data, part, MPI_FLOAT, masterNode, MPI_COMM_WORLD);

    start = MPI_Wtime();//并行开始计时
	//保证部分有序
	odd_even_sort(data, part);
 
	parallelMergeSort(data);
 
	//将全局有序的数据收集到主进程
	MPI_Gather(data, part, MPI_FLOAT, memoryPool, part, MPI_FLOAT, masterNode, MPI_COMM_WORLD);
	
	//输出排序后结果
	/*if (myid == masterNode)
	{
		
		for (int i = 0; i < N; ++i)
			cout << memoryPool[i] << endl;
	}*/

	finish = MPI_Wtime();//并行停止计算
		
	if(myid == masterNode){
        bool flag = true;
        for(int i=1; i<N; i++){
            if(memoryPool[i] < memoryPool[i-1])
            {
                flag = false;
                break;
            }
        }
        if(flag)
            cout<<"排序成功"<<endl;
        else
            cout<<"排序失败"<<endl;
	    cout << "调用进程数：" << numprocs << " 计算时间：" << finish - start << endl;
    }
        
    if (memoryPool) delete[]memoryPool;
	delete[]data;
	MPI_Finalize();
	return 0;
}
 
void odd_even_sort(float *p,int num)
{
	bool sort = false;
	while (!sort)//若一次奇数排序，一次偶数排序，均未发生数据交换，则认为已排好序
	{
		sort = true;
		for (int i = 1; i < num; i+=2)
		{
			if (p[i] < p[i - 1])//此处不能写等号
			{
				swap(p, i, i - 1);
				sort = false;
			}
		}
		for (int i = 2; i < num; i += 2)
		{
			if (p[i] < p[i - 1])
			{
				swap(p, i, i - 1);
				sort = false;
			}
		}
	}
}
inline void swap(float *p, int i, int j)
{
	float tmp = p[i];
	p[i] = p[j];
	p[j] = tmp;
}
void mergeSort(float *data, float *buffer,float *tmp, bool &sort)
{
	int i = 0, j = 0, k = 0;
	while (i != part && j != part)
	{
		if (data[i] <= buffer[j]) { tmp[k++] = data[i++]; }//此处等号一定要加
		else { tmp[k++] = buffer[j++]; }
	}
	if (j != 0)sort = false;
	if (i != part) { while (k != 2 * part)tmp[k++] = data[i++]; }
	if (j != part) { while (k != 2 * part)tmp[k++] = buffer[j++]; }
	memcpy(data, tmp, sizeof(float)*part);
	memcpy(buffer, tmp + part, sizeof(float)*part);
	
	
}
 
void parallelMergeSort(float *data)
{
	MPI_Status status;
	int left = myid - 1, right = myid + 1;
	if (left < 0)left = MPI_PROC_NULL;//虚拟进程，使所有进程都有左右，边界上无序特殊处理
	if (right > numprocs - 1)right = MPI_PROC_NULL;
 
	float *buffer = new float[part];
	float *tmp = new float[2 * part];
	bool sort = false;
	bool flag = false;
	
	while (!flag)
	{
		sort = true;
		if (myid % 2 == 0)
		{
			MPI_Send(data, part, MPI_FLOAT, left, 0, MPI_COMM_WORLD);
			MPI_Recv(data, part, MPI_FLOAT, left, 0, MPI_COMM_WORLD,&status);
		}
		else if(right != MPI_PROC_NULL)//如果接受的是虚拟进程的消息（空），那就不应该进行mergeSort
		{
			MPI_Recv(buffer, part, MPI_FLOAT, right, 0, MPI_COMM_WORLD, &status);
			mergeSort(data, buffer, tmp, sort);
			MPI_Send(buffer, part, MPI_FLOAT, right, 0, MPI_COMM_WORLD);
		}
 
		MPI_Barrier(MPI_COMM_WORLD);//屏障函数，显式将奇排序和偶排序分开，不加也可以
		
		if (myid % 2 == 1)
		{
			MPI_Send(data, part, MPI_FLOAT, left, 0, MPI_COMM_WORLD);
			MPI_Recv(data, part, MPI_FLOAT, left, 0, MPI_COMM_WORLD, &status);
		}
		else if (right != MPI_PROC_NULL)
		{
			MPI_Recv(buffer, part, MPI_FLOAT, right, 0, MPI_COMM_WORLD, &status);
			mergeSort(data, buffer, tmp, sort);
			MPI_Send(buffer, part, MPI_FLOAT, right, 0, MPI_COMM_WORLD);
		}
		
		
		//全规约函数，对每个进程中的sort标识求逻辑与，结果保存在flag中，flag=1 意味着每个进程的sort都为1，排序完成
		MPI_Allreduce(&sort, &flag, 1, MPI_C_BOOL, MPI_LAND, MPI_COMM_WORLD);
		
	}
	
	delete[]tmp;
	delete[]buffer;
}