#include<iostream>
#include<vector>
#include<string>
using namespace std;

void init_times(vector<int>& times, int n) {
    times[0] = 1;
    for (int i = 1; i <= n; i++)
        times[i] = i * times[i - 1];
}

int cal_combinations(const vector<int> &times, int n, int r) {
    if (n == 0)
        return 0;
    return times[n] / (times[n-r] * times[r]);
}

int combination2index_pan(const vector<int> &times, vector<int> comb, int n, int r) {

    int index = 0;
    int cur = 1;
    //逐个扫描
    for (int i = 1; i <= r; i++) {
        while(comb[i-1] > cur) {
            index += cal_combinations(times, n - cur, r - i);
            cur++;
        }
        cur++;
    }
    return index;
}

vector<int> index2combination(const vector<int> &times, int index, int n, int r) {
    vector<int> comb(r, 1);

    int slot = 1;
    int n_comb = 0;
    for (int i = 1; i <= n; i++) {
        if (slot > r)
            return comb;
        int next_comb = cal_combinations(times, n - i, r - slot);
        if (next_comb == 0) {
            // 考虑最后一个数恰好是最后一个位置
            comb[slot - 1] = i;
            return comb;
        }
        if (n_comb + next_comb >= index + 1) {
            //说明当前位置要固定
            // if (next_comb != 1) {
            //     comb[slot - 1] = i;
            // }
            // else {
            //     //考虑非末尾的数是当前位置
            //     comb[slot - 1] = i;
            // }
            comb[slot - 1] = i;
            slot += 1;
        }
        else {
            n_comb += next_comb;
        }
    }
}

int main() {
    int n = 6;
    int r = 3;

    //初始化阶乘数组
    vector<int> times(n + 1, 1);
    init_times(times, n);
    

    vector<vector<int>> examples{ {1,2,3},{1,2,4},{1,2,5},{1,2,6},{1,3,4},{1,3,5},{1,3,6},{1,4,5},{1,4,6},{1,5,6},{2,3,4},{2,3,5},{2,3,6},{2,4,5},{2,4,6},{2,5,6},{3,4,5},{3,4,6},{3,5,6},{4,5,6} };
    for (auto example : examples) {
        cout << combination2index_pan(times, example, n, r) << "  ";
    }
    cout << endl;

    vector<int> indexs(20,0);
    for (int i = 0; i < 20; i++) {
        indexs[i] = i;
    }
    //vector<int> indexs{ 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19 };

    for (auto index : indexs) {
        vector<int> comb = index2combination(times, index, n, r);
        for (auto num : comb) {
            cout << num;
        }
        cout << "  ";
    }
    cout << endl;
    system("pause");
    return 0;
}