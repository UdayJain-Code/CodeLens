#include "report.h"

#include <algorithm>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace codelens {

namespace {

const char* kDisclaimer =
    "Similarity is a review signal, not proof of copying. Two students can independently write\n"
    "similar standard solutions. Check the matched code and the assignment context before judging.";

std::string rule(char c = '-', std::size_t n = 78) { return std::string(n, c); }

std::string cleanLine(const std::string& in) {
    std::string out;
    for (char c : in) {
        if (c == '\t') out += "    ";
        else if (c != '\r') out += c;
    }
    if (out.size() > 110) out = out.substr(0, 107) + "...";
    return out;
}

void printExcerpt(std::ostream& os, const char* tag, const Submission& sub, int from, int to, int maxLines) {
    os << "    [" << tag << "] " << sub.name << "  lines " << from;
    if (to != from) os << "-" << to;
    os << "\n";
    int shown = 0;
    for (int ln = from; ln <= to; ++ln) {
        if (shown >= maxLines) {
            os << "           ... (" << (to - ln + 1) << " more line(s))\n";
            break;
        }
        const bool ok = ln >= 1 && static_cast<std::size_t>(ln) <= sub.lines.size();
        os << "    " << std::setw(6) << ln << " | " << (ok ? cleanLine(sub.lines[static_cast<std::size_t>(ln) - 1]) : std::string()) << "\n";
        ++shown;
    }
}

}  // namespace

std::string percent(double v, int decimals) {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(decimals) << v << "%";
    return ss.str();
}

std::string overlapLabel(double score) {
    if (score >= 80) return "very high";
    if (score >= 50) return "high";
    if (score >= 25) return "moderate";
    return "low";
}

void printSettings(std::ostream& os, const Settings& s) {
    os << "Settings: k-gram size = " << s.k << " tokens, winnowing window = " << s.window
       << " (shared runs of " << (s.k + s.window - 1) << "+ tokens are always detected)\n"
       << "          #include lines ignored: " << (s.ignoreIncludes ? "yes" : "no")
       << ", literals normalised: " << (s.normalizeLiterals ? "yes" : "no")
       << ", LCS score: " << (s.computeLcs ? "on" : "off") << "\n";
}

void printFiles(std::ostream& os, const std::vector<Submission>& subs) {
    os << "  #   Tokens  Status   Name\n";
    for (std::size_t i = 0; i < subs.size(); ++i) {
        const Submission& s = subs[i];
        os << std::setw(3) << (i + 1) << std::setw(9) << s.tokens.size() << "  " << std::left << std::setw(8)
           << (s.usable ? "ok" : "skipped") << std::right << " " << s.name;
        if (!s.usable && !s.note.empty()) os << "   (" << s.note << ")";
        os << "\n";
    }
}

void printSummary(std::ostream& os, const std::vector<Submission>& subs, const std::vector<PairResult>& pairs,
                  const Settings& s) {
    os << "\nRANKED FILE PAIRS (most similar first)\n" << rule() << "\n";
    os << " Rank  Similarity  Overlap     Coverage A/B   Pair\n" << rule() << "\n";
    std::size_t hidden = 0, shown = 0;
    for (std::size_t i = 0; i < pairs.size(); ++i) {
        const PairResult& p = pairs[i];
        if (!p.compared) continue;
        if (p.score < s.minShowScore) { ++hidden; continue; }
        os << std::setw(5) << (i + 1) << std::setw(12) << percent(p.score) << "  " << std::left << std::setw(10)
           << overlapLabel(p.score) << std::right << std::setw(7) << percent(p.coverageA, 0) << " /"
           << std::left << std::setw(6) << percent(p.coverageB, 0) << std::right << "  " << subs[p.a].name
           << " <-> " << subs[p.b].name << "\n";
        ++shown;
    }
    if (shown == 0) os << " (no pairs to show)\n";
    os << rule() << "\n";
    if (hidden > 0) os << hidden << " pair(s) below the display threshold of " << percent(s.minShowScore) << " are hidden.\n";

    bool anySkipped = false;
    for (const Submission& sub : subs) {
        if (sub.usable) continue;
        if (!anySkipped) os << "Not compared:\n";
        anySkipped = true;
        os << "  " << sub.name << " - " << sub.note << "\n";
    }
    os << "Similarity = Jaccard overlap of verified winnowing fingerprints (not a probability of copying).\n"
       << "Coverage   = % of each file's tokens that lie inside matched regions (A = first file, B = second).\n"
       << "Overlap labels are only a guide for deciding which pairs to read first.\n";
}

void printPairDetail(std::ostream& os, const std::vector<Submission>& subs, const PairResult& p,
                     const Settings& s, std::size_t rank) {
    const Submission& A = subs[p.a];
    const Submission& B = subs[p.b];
    os << "\n" << rule('=') << "\nPair #" << rank << ":  " << A.name << "  <->  " << B.name << "\n" << rule('=') << "\n";
    if (!p.compared) {
        os << "Not compared: " << p.note << "\n";
        return;
    }
    os << "Similarity : " << percent(p.score, 2) << "   (" << p.shared << " shared verified fingerprint(s) out of "
       << (p.distinctA + p.distinctB - p.shared) << " distinct; A has " << p.distinctA << ", B has " << p.distinctB << ")\n";
    os << "Coverage   : " << percent(p.coverageA) << " of A's tokens, " << percent(p.coverageB) << " of B's tokens\n";
    os << "Settings   : k = " << p.kUsed << ", window = " << p.windowUsed << "\n";
    if (p.shortFallback) {
        os << "Note       : SHORT-FILE FALLBACK - at least one file has fewer than " << (s.k + s.window - 1)
           << " tokens, so k was reduced and every token group was fingerprinted. Treat the score with extra care.\n";
    }
    if (p.lcsComputed) os << "Token order: " << percent(p.lcsScore) << " (LCS-based, 2*LCS / (tokens A + tokens B))\n";
    if (p.lcsSkipped) os << "Token order: skipped (files too large for the LCS size limit)\n";

    if (p.regions.empty()) {
        os << "\nNo matching sections were found.\n";
        return;
    }
    const std::size_t limit = std::min<std::size_t>(p.regions.size(), static_cast<std::size_t>(s.maxExcerpts));
    os << "\nMatching sections (" << p.totalRegions << " found, longest first):\n";
    for (std::size_t i = 0; i < limit; ++i) {
        const MatchRegion& m = p.regions[i];
        os << "\n  Match " << (i + 1) << " - " << m.tokens() << " tokens\n";
        printExcerpt(os, "A", A, m.aLineStart, m.aLineEnd, s.maxExcerptLines);
        printExcerpt(os, "B", B, m.bLineStart, m.bLineEnd, s.maxExcerptLines);
    }
    if (p.totalRegions > limit) {
        os << "\n  ... " << (p.totalRegions - limit) << " more matching section(s) not shown.\n";
    }
}

void writeFullReport(std::ostream& os, const std::vector<Submission>& subs, const std::vector<PairResult>& pairs,
                     const Settings& s, const std::vector<std::string>& warnings) {
    std::time_t t = std::time(nullptr);
    std::tm tmv{};
#ifdef _WIN32
    localtime_s(&tmv, &t);
#else
    localtime_r(&t, &tmv);
#endif
    char buf[64];
    std::strftime(buf, sizeof buf, "%Y-%m-%d %H:%M:%S", &tmv);

    os << rule('=') << "\nCodeLens - source-code similarity report\nGenerated: " << buf << "\n" << rule('=') << "\n\n";
    os << kDisclaimer << "\n\n";
    printSettings(os, s);
    os << "\nFILES\n" << rule() << "\n";
    printFiles(os, subs);
    if (!warnings.empty()) {
        os << "\nWARNINGS\n";
        for (const std::string& w : warnings) os << "  - " << w << "\n";
    }
    printSummary(os, subs, pairs, s);

    const std::size_t kMaxDetails = 50;
    std::size_t printed = 0, eligible = 0;
    for (std::size_t i = 0; i < pairs.size(); ++i) {
        const PairResult& p = pairs[i];
        if (!p.compared || p.shared == 0 || p.score < s.minShowScore) continue;
        ++eligible;
        if (printed < kMaxDetails) {
            printPairDetail(os, subs, p, s, i + 1);
            ++printed;
        }
    }
    if (eligible > printed) os << "\n(" << (eligible - printed) << " further pair(s) omitted from the detail section.)\n";
    os << "\n" << rule() << "\n" << kDisclaimer << "\n";
}

}  // namespace codelens
