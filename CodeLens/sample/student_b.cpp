#include <bits/stdc++.h>
using namespace std;

/* sorts list using adjacent swaps */
void sortList(vector<int> &data)
{
    int len = data.size();
    for (int x = 0; x < len - 1; x++)
    {
        for (int y = 0; y < len - x - 1; y++)
        {
            if (data[y] > data[y + 1])
            {
                int holder = data[y];
                data[y] = data[y + 1];
                data[y + 1] = holder;
            }
        }
    }
}

int main()
{
    int count;
    cout << "Enter count: ";
    cin >> count;
    vector<int> data(count);
    for (int x = 0; x < count; x++)
        cin >> data[x];
    sortList(data);
    cout << "Result: ";
    for (int x = 0; x < count; x++)
        cout << data[x] << ' ';
    cout << endl;
    return 0;
}
