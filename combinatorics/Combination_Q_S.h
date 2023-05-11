#ifndef COMBINATION_Q_S
#define COMBINATION_Q_S
#pragma once
#include <iostream>
#include <queue>
#include <vector>
using namespace std;
// 在函数中填入对应的数字进行对比
class CombinationSolution
{
public:
    const int N; // the size of N
    const int R;
    CombinationSolution(const int n, const int r) : N(n), R(r){};

    inline virtual vector<int> index2vec(int index)
    {
        // vector<int> ans;
        // return ans;
    }

    inline virtual int vector2index(vector<int> comb)
    {
        // return index;
    }
};

class Solution_phl : public CombinationSolution
{
private:
    vector<int> factorials;

public:
    Solution_phl(const int n, const int r) : CombinationSolution(n, r)
    {
        // 初始化阶乘矩阵
        if (factorials.empty())
            factorials.push_back(1);
        for (int i = 1; i <= n; i++)
            factorials.push_back(factorials.back() * i);
    };

    inline vector<int> index2vec(int index) override
    {
        vector<int> ans(R, 1);

        int slot = 1;   //考虑到第几个空
        int n_comb = 0; // 累积了多少排列数
        for (int i = 1; i <= N; i++)
        {
            if (slot > R) //终止条件1：所有空位都已考虑
                return ans;
            int n_next = cal_combinations(N - i, R - slot); //当前数字在当前空的全排列数
            if (n_next == 0)
            {
                //最后一个数恰好是最后一个位置，填入
                ans[slot - 1] = i;
                return ans;
            }
            if (n_comb + n_next >= index + 1)
            {
                // i位于slot处
                ans[slot - 1] = i;
                slot += 1;
            }
            else
            {
                // 继续累积排列数，考虑下一个i
                n_comb += n_next;
            }
        }
    }

    inline int vector2index(vector<int> comb) override
    {
        int index = 0;
        int cur = 1;
        //逐个扫描
        for (int i = 1; i <= R; i++)
        {
            while (comb[i - 1] > cur)
            {
                index += cal_combinations(N - cur, R - i);
                cur++;
            }
            cur++;
        }
        return index;
    }

    inline int cal_combinations(int n, int r)
    {
        if (n == 0 || r > n)
            return 0;
        return factorials[n] / (factorials[n - r] * factorials[r]);
    }
};

class CombinationQuestions
{
public:
    queue<int> answers;           //代表组合对应的index
    queue<vector<int>> questions; //代表组合的所有元素
    int N, R;
    CombinationQuestions(int n, int r) : N(n), R(r)
    {
        /*create_questions();*/
    }
    void help(vector<int> cur, int &c, int k)
    {
        if (cur.size() == R)
        {
            answers.push(c);
            c++;
            questions.push(cur);
            return;
        }
        for (int i = k; i <= N - R + cur.size() + 1; i++)
        {
            cur.push_back(i);
            help(cur, c, i + 1);
            cur.pop_back();
        }
    }
    //用于创建所有的组合和排序
    void create_all_combination()
    {
        int start = 0;
        vector<int> k;
        help(k, start, 1);
    }
    void create_questions()
    {
        //这里可以根据个人的喜好去创建初始化的答案和结果,请个人选择进行初始化
        create_all_combination();
    }
    //用于创建一个的index对应combination的对
    void add_one_question_ans(vector<int> q, int a)
    {
        answers.push(a);
        questions.push(q);
    }

    bool justify(CombinationSolution s)
    {
        int id = answers.front();
        answers.pop();
        vector<int> comb = questions.front();
        questions.pop();

        vector<int> exp = s.index2vec(id);

        if (!justify_two_vec(exp, comb))
        {
            cout << "the solution id2vetor is not true, the id is:" << id << endl
                 << "exp: ";
            for (int i : comb)
                cout << i << " ";
            cout << endl
                 << "get: ";
            for (int i : exp)
                cout << i << " ";
            cout << endl;
            return false;
        }
        int res = s.vector2index(comb);
        if (res != id)
        {
            cout << "cur index is: ";
            for (int i : comb)
            {
                cout << i << " ";
            }
            cout << "the solution vector2idex is not true" << endl;
            cout << "expected id: " << id << ", but get the res: " << res << endl;
            return false;
        }
        return true;
    }

    bool justify_two_vec(vector<int> &a, vector<int> &b)
    {
        int sz = a.size();
        for (int i = 0; i < sz; i++)
        {
            if (a[i] != b[i])
                return false;
        }
        return true;
    }

    bool empty()
    {
        return answers.empty();
    }

    bool justify_all(CombinationSolution *s)
    {
        while (!empty())
        {
            bool res = justify(*s);
            if (!res)
                return 0;
        }
        return 1;
    }
};

#endif