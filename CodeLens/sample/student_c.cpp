#include <iostream>
#include <vector>
using namespace std;

int findMinIndex(const vector<int>& v, int start) {
    int best = start;
    for (int i = start + 1; i < (int)v.size(); i++) {
        if (v[i] < v[best]) best = i;
    }
    return best;
}

int main() {
    int n;
    cout << "Size: ";
    cin >> n;
    vector<int> v(n);
    for (int i = 0; i < n; i++) cin >> v[i];
    for (int i = 0; i < n; i++) {
        int m = findMinIndex(v, i);
        swap(v[i], v[m]);
    }
    for (int x : v) cout << x << " ";
    cout << endl;
    return 0;
}
