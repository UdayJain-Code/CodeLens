// fingerprint.h - k-grams, rolling hash and winnowing
#pragma once
#include <cstdint>
#include <unordered_map>
#include <vector>

#include "types.h"

namespace codelens {

constexpr std::uint64_t kHashBase = 1000003ULL;

struct Fingerprint {
    std::uint64_t hash;
    std::size_t pos;   // index of the first token of the k-gram
};

// Fingerprints of one file: hash -> every token position that was selected.
struct FpIndex {
    int k = 0;
    int window = 0;
    std::size_t kgrams = 0;   // total number of k-grams in the file
    std::unordered_map<std::uint64_t, std::vector<std::size_t>> positions;
};

std::uint64_t mix64(std::uint64_t x);

// Rolling (Rabin-Karp style) hash of every window of k tokens: O(n) overall.
std::vector<std::uint64_t> rollingKgramHashes(const std::vector<Token>& tokens, int k);

// Winnowing with a monotonic deque: the minimum hash (rightmost on ties) of
// every window of `window` consecutive hashes. Any shared run of at least
// window + k - 1 tokens is guaranteed to yield a shared fingerprint.
std::vector<Fingerprint> winnow(const std::vector<std::uint64_t>& hashes, int window);

FpIndex buildIndex(const std::vector<Token>& tokens, int k, int window);

}  // namespace codelens
