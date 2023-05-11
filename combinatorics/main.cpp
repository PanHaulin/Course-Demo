//#include <bits/stdc++.h>
#include"Combination_Q_S.h"
#include <iostream>
using namespace std;

#define choose_N 6
#define choose_R 3



int main() {
    CombinationQuestions q(choose_N,choose_R);
    CombinationSolution* s{new Solution_phl(choose_N, choose_R)};
    
    if(!q.justify_all(s))
        return 0;
        
    cout << "completed successfully !" << endl;
    // while (!q.empty()) {
    //     bool res = q.justify(s);
    //     if (!res) return 0;
    // }

}