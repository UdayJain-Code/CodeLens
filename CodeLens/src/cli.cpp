#include "cli.h"

#include <cstdlib>
#include <fstream>
#include <iostream>

#include "file_loader.h"
#include "pipeline.h"
#include "report.h"
#include "similarity.h"

namespace codelens {

namespace {

void printUsage(std::ostream& os) {
    os << "CodeLens - C++ source-code similarity detector\n\n"
          "Usage:\n"
          "  codelens                        interactive menu\n"
          "  codelens FILE_A FILE_B [options]   compare two files\n"
          "  codelens --dir FOLDER [options]    compare every C/C++ file in a folder\n\n"
          "Options:\n"
          "  -k N              k-gram size in tokens (2-30, default 7)\n"
          "  -w N              winnowing window (1-30, default 4)\n"
          "  --lcs             also compute the token-order (LCS) score\n"
          "  --keep-includes   do not ignore #include lines\n"
          "  --keep-literals   do not normalise numbers/strings/chars\n"
          "  --min-score X     hide pairs below X percent\n"
          "  --excerpts N      matching sections shown per pair (default 5)\n"
          "  --details N       number of top pairs to show in detail (default 3, or 1 for two files)\n"
          "  --report FILE     also save a full text report\n"
          "  -h, --help        show this help\n";
}

bool parseInt(const std::string& v, int lo, int hi, int& out) {
    char* end = nullptr;
    const long x = std::strtol(v.c_str(), &end, 10);
    if (v.empty() || *end != '\0' || x < lo || x > hi) return false;
    out = static_cast<int>(x);
    return true;
}

bool parseDouble(const std::string& v, double lo, double hi, double& out) {
    char* end = nullptr;
    const double x = std::strtod(v.c_str(), &end);
    if (v.empty() || *end != '\0' || x < lo || x > hi) return false;
    out = x;
    return true;
}

int usageError(const std::string& msg) {
    std::cerr << "Error: " << msg << "\n\n";
    printUsage(std::cerr);
    return 1;
}

}  // namespace

int runCli(const std::vector<std::string>& args) {
    Settings s;
    std::string dir, reportPath;
    std::vector<std::string> files;
    int details = -1;

    for (std::size_t i = 0; i < args.size(); ++i) {
        const std::string& a = args[i];
        auto next = [&](std::string& out) {
            if (i + 1 >= args.size()) return false;
            out = args[++i];
            return true;
        };
        std::string v;
        if (a == "-h" || a == "--help") { printUsage(std::cout); return 0; }
        else if (a == "-k") { if (!next(v) || !parseInt(v, 2, 30, s.k)) return usageError("-k needs a whole number from 2 to 30"); }
        else if (a == "-w") { if (!next(v) || !parseInt(v, 1, 30, s.window)) return usageError("-w needs a whole number from 1 to 30"); }
        else if (a == "--lcs") s.computeLcs = true;
        else if (a == "--keep-includes") s.ignoreIncludes = false;
        else if (a == "--keep-literals") s.normalizeLiterals = false;
        else if (a == "--min-score") { if (!next(v) || !parseDouble(v, 0, 100, s.minShowScore)) return usageError("--min-score needs a number from 0 to 100"); }
        else if (a == "--excerpts") { if (!next(v) || !parseInt(v, 1, 50, s.maxExcerpts)) return usageError("--excerpts needs a whole number from 1 to 50"); }
        else if (a == "--details") { if (!next(v) || !parseInt(v, 0, 1000, details)) return usageError("--details needs a whole number from 0 to 1000"); }
        else if (a == "--dir") { if (!next(dir)) return usageError("--dir needs a folder name"); }
        else if (a == "--report") { if (!next(reportPath)) return usageError("--report needs a file name"); }
        else if (!a.empty() && a[0] == '-') return usageError("unknown option: " + a);
        else files.push_back(a);
    }

    std::vector<Submission> subs;
    std::vector<std::string> warnings;
    if (!dir.empty()) {
        if (!files.empty()) return usageError("give either --dir FOLDER or two files, not both");
        if (!loadFolder(dir, s, subs, warnings)) {
            for (const std::string& w : warnings) std::cerr << "Error: " << w << "\n";
            return 2;
        }
        if (details < 0) details = 3;
    } else {
        if (files.size() != 2) return usageError("expected exactly two files (or --dir FOLDER)");
        subs = loadTwoFiles(files[0], files[1], s);
        for (const Submission& sub : subs) if (!sub.usable) warnings.push_back(sub.name + ": " + sub.note);
        if (details < 0) details = 1;
    }
    for (const std::string& w : warnings) std::cerr << "Warning: " << w << "\n";
    if (countUsable(subs) < 2) {
        std::cerr << "Error: need at least two usable files to compare.\n";
        return 2;
    }

    const std::vector<PairResult> results = compareAll(subs, s);
    printSettings(std::cout, s);
    printSummary(std::cout, subs, results, s);
    int shown = 0;
    for (std::size_t i = 0; i < results.size() && shown < details; ++i) {
        if (!results[i].compared || results[i].score < s.minShowScore) continue;
        printPairDetail(std::cout, subs, results[i], s, i + 1);
        ++shown;
    }

    if (!reportPath.empty()) {
        std::ofstream out(reportPath);
        if (!out) {
            std::cerr << "Error: cannot write report to '" << reportPath << "'\n";
            return 2;
        }
        writeFullReport(out, subs, results, s, warnings);
        std::cout << "\nReport saved to: " << reportPath << "\n";
    }
    return 0;
}

}  // namespace codelens
