#include "fingerprint.h"

#include <algorithm>
#include <deque>

namespace codelens {

std::uint64_t mix64(std::uint64_t x) {   // splitmix64 finaliser
    x += 0x9e3779b97f4a7c15ULL;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    return x ^ (x >> 31);
}

std::vector<std::uint64_t> rollingKgramHashes(const std::vector<Token>& t, int k) {
    std::vector<std::uint64_t> out;
    if (k <= 0 || t.size() < static_cast<std::size_t>(k)) return out;
    const std::size_t K = static_cast<std::size_t>(k);

    std::uint64_t powK1 = 1;                       // B^(k-1)
    for (std::size_t i = 0; i + 1 < K; ++i) powK1 *= kHashBase;

    std::uint64_t h = 0;
    for (std::size_t i = 0; i < K; ++i) h = h * kHashBase + t[i].hash;
    out.reserve(t.size() - K + 1);
    out.push_back(mix64(h));
    for (std::size_t i = 0; i + K < t.size(); ++i) {
        h = (h - t[i].hash * powK1) * kHashBase + t[i + K].hash;   // drop t[i], add t[i+k]
        out.push_back(mix64(h));
    }
    return out;
}

std::vector<Fingerprint> winnow(const std::vector<std::uint64_t>& h, int window) {
    std::vector<Fingerprint> out;
    if (h.empty() || window <= 0) return out;
    const std::size_t w = std::min<std::size_t>(static_cast<std::size_t>(window), h.size());
    std::deque<std::size_t> dq;   // indices; hashes strictly increasing front -> back
    std::size_t last = static_cast<std::size_t>(-1);
    for (std::size_t i = 0; i < h.size(); ++i) {
        while (!dq.empty() && h[dq.back()] >= h[i]) dq.pop_back();   // keeps rightmost minimum
        dq.push_back(i);
        while (dq.front() + w <= i) dq.pop_front();                  // left the window
        if (i + 1 >= w) {
            const std::size_t m = dq.front();
            if (m != last) {
                out.push_back({h[m], m});
                last = m;
            }
        }
    }
    return out;
}

FpIndex buildIndex(const std::vector<Token>& tokens, int k, int window) {
    FpIndex idx;
    idx.k = k;
    idx.window = window;
    const std::vector<std::uint64_t> hashes = rollingKgramHashes(tokens, k);
    idx.kgrams = hashes.size();
    for (const Fingerprint& f : winnow(hashes, window)) idx.positions[f.hash].push_back(f.pos);
    return idx;
}

}  // namespace codelens
