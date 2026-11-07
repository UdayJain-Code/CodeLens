// similarity.h - verified fingerprint comparison, Jaccard score, matching regions, ranking
#pragma once
#include <cstddef>
#include <functional>
#include <string>
#include <vector>

#include "fingerprint.h"
#include "types.h"

namespace codelens {

// A maximal run of identical normalised tokens shared by two files.
struct MatchRegion {
    std::size_t aStart = 0, aEnd = 0;   // token indices in file A (end exclusive)
    std::size_t bStart = 0, bEnd = 0;   // token indices in file B (end exclusive)
    int aLineStart = 0, aLineEnd = 0;   // original line numbers
    int bLineStart = 0, bLineEnd = 0;
    std::size_t tokens() const { return aEnd - aStart; }
};

struct PairResult {
    std::size_t a = 0, b = 0;           // indices into the submissions vector
    bool compared = false;
    std::string note;                   // why a pair was not compared
    bool shortFallback = false;         // short-file fallback was used
    int kUsed = 0, windowUsed = 0;
    std::size_t distinctA = 0, distinctB = 0, shared = 0;   // distinct fingerprints
    double score = 0;                   // Jaccard similarity, 0..100
    double coverageA = 0, coverageB = 0;// % of tokens inside matched regions
    std::vector<MatchRegion> regions;   // longest first (at most maxStoredRegions)
    std::size_t totalRegions = 0;
    bool lcsComputed = false, lcsSkipped = false;
    double lcsScore = 0;
};

// shared / (distinctA + distinctB - shared) * 100
double jaccardPercent(std::size_t shared, std::size_t distinctA, std::size_t distinctB);

// Compare two files. cachedA/cachedB are fingerprint indexes built with the
// same k/window as `s`; pass nullptr to let the function build them.
PairResult comparePair(const Submission& A, const Submission& B, const Settings& s,
                       const FpIndex* cachedA = nullptr, const FpIndex* cachedB = nullptr);

using ProgressFn = std::function<void(std::size_t done, std::size_t total)>;

// Compare every usable pair, return them ranked by similarity (highest first).
std::vector<PairResult> compareAll(const std::vector<Submission>& subs, const Settings& s,
                                   const ProgressFn& progress = nullptr);

}  // namespace codelens
