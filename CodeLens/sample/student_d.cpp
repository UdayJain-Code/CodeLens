#include <iostream>
using namespace std;

bool isPrime(int n) {
    if (n < 2) return false;
    for (int d = 2; d * d <= n; d++) {
        if (n % d == 0) return false;
    }
    return true;
}

int main() {
    int limit;
    cout << "Upper limit: ";
    cin >> limit;
    for (int n = 2; n <= limit; n++) {
        if (isPrime(n)) cout << n << "\n";
    }
    return 0;
}
