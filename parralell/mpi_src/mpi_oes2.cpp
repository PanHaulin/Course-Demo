typedef long long __int64;
#include "mpi.h"
#include <cstdio>
#include <algorithm>
#include <cmath>
using namespace std;
int Compute_partner(int phase,int my_rank,int comm_sz){//根据趟数的奇偶性以及当前编号的编号得到partner进程的编号
    int partner;
    if(!(phase&1)){
        if(my_rank&1){
            partner=my_rank-1;
        }
        else{
            partner=my_rank+1;
        }
    }
    else{
        if(my_rank&1){
            partner=my_rank+1;
        }
        else{
            partner=my_rank-1;
        }
    }
    if(partner==-1 || partner==comm_sz){
        partner=MPI_PROC_NULL;
    }
    return partner;
}
int main(int argc, char* argv[]){
    int my_rank=0, comm_sz=0;
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &comm_sz);
    int np,n,local_n;//分别为进行奇偶交换排序的趟数和读入的数据总量以及分成的每段的长度
    FILE* fp;
    if(my_rank==0){
        fp=fopen("Sort.txt","r");
        fscanf(fp,"%d%d",&np,&n);
        local_n=n/comm_sz;
    }
    MPI_Bcast(&np,1,MPI_INT,0,MPI_COMM_WORLD);
    MPI_Bcast(&local_n,1,MPI_INT,0,MPI_COMM_WORLD);
    int* keys;
    int* my_keys=new int[local_n];
    // if(my_rank==0){
    //     keys=new int[n];
    //     for(int i=0;i<n;++i){
    //         fscanf(fp,"%d",&keys[i]);
    //     }
    //     fclose(fp);
    // }
    double beginTime = MPI_Wtime();
    MPI_Scatter(keys,local_n,MPI_INT,my_keys,local_n,MPI_INT,0,MPI_COMM_WORLD);
 
    //sort(my_keys,my_keys+local_n);//串行快速排序
 
    for(int i=0;i<local_n;++i){//串行奇偶交换排序
        if(!(i&1)){
            for(int j=0;j+1<local_n;j+=2){
                if(my_keys[j]>my_keys[j+1]){
                    swap(my_keys[j],my_keys[j+1]);
                }
            }
        }
        else{
            for(int j=1;j+1<local_n;j+=2){
                if(my_keys[j]>my_keys[j+1]){
                    swap(my_keys[j],my_keys[j+1]);
                }
            }
        }
    }
 
    int* recv_keys=new int[local_n];
    int* temp_keys=new int[local_n];
    for(int i=0;i<np;++i){
        int partner=Compute_partner(i, my_rank, comm_sz);
        if (partner != MPI_PROC_NULL){
            MPI_Sendrecv(my_keys, local_n, MPI_INT, partner, 0, recv_keys, local_n, MPI_INT, partner, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            if(my_rank<partner){//编号小的进程留下归并时较小的一半
                int e=0,e1=0,e2=0;
                int* temp_keys=new int[local_n];
                while(e<local_n){
                    if(my_keys[e1]<=recv_keys[e2]){
                        temp_keys[e]=my_keys[e1];
                        ++e;
                        ++e1;
                    }
                    else{
                        temp_keys[e]=recv_keys[e2];
                        ++e;
                        ++e2;
                    }
                }
                for(int j=0;j<local_n;++j){
                    my_keys[j]=temp_keys[j];
                }
            }
            else{//编号大的进程留下归并时较大的一半
                int e=local_n-1,e1=local_n-1,e2=local_n-1;
                while(e>=0){
                    if(my_keys[e1]>=recv_keys[e2]){
                        temp_keys[e]=my_keys[e1];
                        --e;
                        --e1;
                    }
                    else{
                        temp_keys[e]=recv_keys[e2];
                        --e;
                        --e2;
                    }
                }
                for(int j=0;j<local_n;++j){
                    my_keys[j]=temp_keys[j];
                }
            }
        }
    }
    MPI_Gather(my_keys, local_n, MPI_INT, keys, local_n, MPI_INT, 0, MPI_COMM_WORLD);
    double endTime = MPI_Wtime();
    if (my_rank == 0){
        for(int i=0;i<n;++i){
            printf("%d ",keys[i]);
        }
        puts("");
        printf("spent time = %lf second\n", endTime - beginTime);
    }
    delete[] keys;
    delete[] my_keys;
    delete[] recv_keys;
    delete[] temp_keys;
    MPI_Finalize();
    return 0;
}