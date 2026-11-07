#include "similarity.h"

#include <algorithm>
#include <climits>

namespace codelens {

namespace {

struct PosPair {
    std::size_t pa, pb;
};

bool equalRun(const std::vector<Token>& a, std::size_t pa, const std::vector<Token>& b, std::size_t pb,
              std::size_t len) {
    for (std::size_t j = 0; j < len; ++j) {
        if (a[pa + j].norm != b[pb + j].norm) return false;
    }
    return true;
}

double lcsPercent(const std::vector<Token>& a, const std::vector<Token>& b) {
    const std::size_t n = a.size(), m = b.size();
    std::vector<std::uint32_t> prev(m + 1, 0), cur(m + 1, 0);
    for (std::size_t i = 1; i <= n; ++i) {
        for (std::size_t j = 1; j <= m; ++j) {
            if (a[i - 1].hash == b[j - 1].hash) cur[j] = prev[j - 1] + 1;
            else cur[j] = std::max(prev[j], cur[j - 1]);
        }
        std::swap(prev, cur);
    }
    return 200.0 * static_cast<double>(prev[m]) / static_cast<double>(n + m);
}

}  // namespace

double jaccardPercent(std::size_t shared, std::size_t distinctA, std::size_t distinctB) {
    const std::size_t total = distinctA + distinctB - shared;
    if (total == 0) return 0.0;
    return 100.0 * static_cast<double>(shared) / static_cast<double>(total);
}

PairResult comparePair(const Submission& A, const Submission& B, const Settings& s, const FpIndex* cachedA,
                       const FpIndex* cachedB) {
    PairResult r;
    const std::vector<Token>& ta = A.tokens;
    const std::vector<Token>& tb = B.tokens;
    if (!A.usable || !B.usable || ta.size() < s.minTokens || tb.size() < s.minTokens) {
        r.note = "insufficient code in one or both files";
        return r;
    }

    // Short-file fallback: below k + window - 1 tokens the winnowing guarantee
    // no longer holds, so shrink k to fit and keep every k-gram (window = 1).
    const std::size_t minSize = std::min(ta.size(), tb.size());
    const std::size_t guarantee = static_cast<std::size_t>(s.k) + static_cast<std::size_t>(s.window) - 1;
    int k = s.k, w = s.window;
    const bool fallback = minSize < guarantee;
    if (fallback) {
        k = static_cast<int>(std::min<std::size_t>(static_cast<std::size_t>(s.k), minSize));
        w = 1;
    }
    r.kUsed = k;
    r.windowUsed = w;
    r.shortFallback = fallback;

    FpIndex localA, localB;
    const FpIndex* ia = cachedA;
    const FpIndex* ib = cachedB;
    if (fallback || !ia || !ib) {
        localA = buildIndex(ta, k, w);
        localB = buildIndex(tb, k, w);
        ia = &localA;
        ib = &localB;
    }
    r.distinctA = ia->positions.size();
    r.distinctB = ib->positions.size();

    // Verify every shared hash against the real token groups, so a hash
    // collision can never count as a match.
    const std::size_t K = static_cast<std::size_t>(k);
    const std::size_t kMaxPairs = 200000;
    std::vector<PosPair> verified;
    for (const auto& entry : ia->positions) {
        auto it = ib->positions.find(entry.first);
        if (it == ib->positions.end()) continue;
        const std::vector<std::size_t>& posA = entry.second;
        const std::vector<std::size_t>& posB = it->second;
        const std::size_t ca = std::min(posA.size(), s.maxPositionsPerHash);
        const std::size_t cb = std::min(posB.size(), s.maxPositionsPerHash);
        bool ok = false;
        for (std::size_t x = 0; x < ca; ++x) {
            for (std::size_t y = 0; y < cb; ++y) {
                if (equalRun(ta, posA[x], tb, posB[y], K)) {
                    ok = true;
                    if (verified.size() < kMaxPairs) verified.push_back({posA[x], posB[y]});
                }
            }
        }
        if (ok) ++r.shared;
    }
    r.compared = true;
    r.score = jaccardPercent(r.shared, r.distinctA, r.distinctB);

    // Turn verified k-gram pairs into maximal matching regions. Pairs on the
    // same diagonal (posA - posB) are merged by extending left and right.
    if (!verified.empty()) {
        std::sort(verified.begin(), verified.end(), [](const PosPair& x, const PosPair& y) {
            const long long dx = static_cast<long long>(x.pa) - static_cast<long long>(x.pb);
            const long long dy = static_cast<long long>(y.pa) - static_cast<long long>(y.pb);
            if (dx != dy) return dx < dy;
            return x.pa < y.pa;
        });
        std::vector<MatchRegion> regions;
        long long curDiag = LLONG_MIN;
        std::size_t curEnd = 0;   // end (exclusive, in A) of the last region on this diagonal
        for (const PosPair& p : verified) {
            const long long d = static_cast<long long>(p.pa) - static_cast<long long>(p.pb);
            if (d != curDiag) { curDiag = d; curEnd = 0; }
            if (p.pa < curEnd) continue;                       // already inside a region
            std::size_t sa = p.pa, sb = p.pb;
            while (sa > curEnd && sb > 0 && ta[sa - 1].norm == tb[sb - 1].norm) { --sa; --sb; }
            std::size_t len = (p.pa - sa) + K;
            while (sa + len < ta.size() && sb + len < tb.size() && ta[sa + len].norm == tb[sb + len].norm) ++len;
            curEnd = sa + len;
            MatchRegion m;
            m.aStart = sa; m.aEnd = sa + len;
            m.bStart = sb; m.bEnd = sb + len;
            m.aLineStart = ta[m.aStart].line; m.aLineEnd = ta[m.aEnd - 1].endLine;
            m.bLineStart = tb[m.bStart].line; m.bLineEnd = tb[m.bEnd - 1].endLine;
            regions.push_back(m);
        }

        std::vector<char> coverA(ta.size(), 0), coverB(tb.size(), 0);
        for (const MatchRegion& m : regions) {
            for (std::size_t x = m.aStart; x < m.aEnd; ++x) coverA[x] = 1;
            for (std::size_t x = m.bStart; x < m.bEnd; ++x) coverB[x] = 1;
        }
        r.coverageA = 100.0 * static_cast<double>(std::count(coverA.begin(), coverA.end(), 1)) / static_cast<double>(ta.size());
        r.coverageB = 100.0 * static_cast<double>(std::count(coverB.begin(), coverB.end(), 1)) / static_cast<double>(tb.size());

        std::stable_sort(regions.begin(), regions.end(), [](const MatchRegion& x, const MatchRegion& y) {
            if (x.tokens() != y.tokens()) return x.tokens() > y.tokens();
            return x.aStart < y.aStart;
        });
        r.totalRegions = regions.size();
        if (regions.size() > s.maxStoredRegions) regions.resize(s.maxStoredRegions);
        r.regions = std::move(regions);
    }

    if (s.computeLcs) {
        if (ta.size() * tb.size() > s.lcsMaxCells) {
            r.lcsSkipped = true;
        } else {
            r.lcsComputed = true;
            r.lcsScore = lcsPercent(ta, tb);
        }
    }
    return r;
}

std::vector<PairResult> compareAll(const std::vector<Submission>& subs, const Settings& s,
                                   const ProgressFn& progress) {
    std::vector<std::size_t> use;
    for (std::size_t i = 0; i < subs.size(); ++i) if (subs[i].usable) use.push_back(i);

    std::vector<PairResult> results;
    if (use.size() < 2) return results;

    std::vector<FpIndex> index(subs.size());
    for (std::size_t i : use) index[i] = buildIndex(subs[i].tokens, s.k, s.window);

    const std::size_t total = use.size() * (use.size() - 1) / 2;
    std::size_t done = 0;
    results.reserve(total);
    for (std::size_t x = 0; x < use.size(); ++x) {
        for (std::size_t y = x + 1; y < use.size(); ++y) {
            PairResult r = comparePair(subs[use[x]], subs[use[y]], s, &index[use[x]], &index[use[y]]);
            r.a = use[x];
            r.b = use[y];
            results.push_back(std::move(r));
            ++done;
            if (progress) progress(done, total);
        }
    }

    std::stable_sort(results.begin(), results.end(), [](const PairResult& p, const PairResult& q) {
        if (p.score != q.score) return p.score > q.score;
        if (p.shared != q.shared) return p.shared > q.shared;
        if (p.a != q.a) return p.a < q.a;
        return p.b < q.b;
    });
    return results;
}

}  // namespace codelens
