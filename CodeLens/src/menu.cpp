#include "menu.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include "file_loader.h"
#include "pipeline.h"
#include "report.h"
#include "similarity.h"

namespace fs = std::filesystem;

namespace codelens {

namespace {

struct AppState {
    Settings settings;
    std::vector<Submission> subs;
    std::vector<std::string> warnings;
    std::vector<PairResult> results;
    bool haveResults = false;
    bool stale = false;   // settings or files changed after the last run
};

bool readLine(const std::string& prompt, std::string& out) {
    std::cout << prompt << std::flush;
    if (!std::getline(std::cin, out)) return false;
    if (!out.empty() && out.back() == '\r') out.pop_back();
    return true;
}

// Returns false on blank input (cancel) or end of input.
bool askInt(const std::string& prompt, int lo, int hi, int& value) {
    for (;;) {
        std::string line;
        if (!readLine(prompt, line)) return false;
        line = trim(line);
        if (line.empty()) return false;
        char* end = nullptr;
        const long v = std::strtol(line.c_str(), &end, 10);
        if (*end != '\0' || v < lo || v > hi) {
            std::cout << "  Please enter a whole number from " << lo << " to " << hi << " (blank to cancel).\n";
            continue;
        }
        value = static_cast<int>(v);
        return true;
    }
}

bool askDouble(const std::string& prompt, double lo, double hi, double& value) {
    for (;;) {
        std::string line;
        if (!readLine(prompt, line)) return false;
        line = trim(line);
        if (line.empty()) return false;
        char* end = nullptr;
        const double v = std::strtod(line.c_str(), &end);
        if (*end != '\0' || v < lo || v > hi) {
            std::cout << "  Please enter a number from " << lo << " to " << hi << " (blank to cancel).\n";
            continue;
        }
        value = v;
        return true;
    }
}

bool askYesNo(const std::string& prompt) {
    std::string line;
    if (!readLine(prompt + " [y/N]: ", line)) return false;
    line = trim(line);
    return !line.empty() && (line[0] == 'y' || line[0] == 'Y');
}

void resetResults(AppState& st) {
    st.results.clear();
    st.haveResults = false;
    st.stale = false;
}

void showFiles(const AppState& st) {
    if (st.subs.empty()) {
        std::cout << "\nNo files loaded yet. Use option 1 or 2 first.\n";
        return;
    }
    std::cout << "\nLoaded files:\n";
    printFiles(std::cout, st.subs);
}

void afterLoad(AppState& st) {
    resetResults(st);
    showFiles(st);
    const std::size_t usable = countUsable(st.subs);
    if (usable < 2) {
        std::cout << "\nWarning: fewer than two usable files. Fix the problems above before comparing.\n";
    } else {
        std::cout << "\nReady. Choose option 4 to run the comparison.\n";
    }
}

void doSelectTwoFiles(AppState& st) {
    std::string a, b;
    if (!readLine("First C++ file  (blank to cancel): ", a)) return;
    a = trimPath(a);
    if (a.empty()) return;
    if (!readLine("Second C++ file (blank to cancel): ", b)) return;
    b = trimPath(b);
    if (b.empty()) return;
    st.subs = loadTwoFiles(a, b, st.settings);
    st.warnings.clear();
    for (const Submission& s : st.subs) if (!s.usable) st.warnings.push_back(s.name + ": " + s.note);
    afterLoad(st);
}

void doSelectFolder(AppState& st) {
    std::string dir;
    if (!readLine("Folder containing submissions (blank to cancel): ", dir)) return;
    dir = trimPath(dir);
    if (dir.empty()) return;
    std::vector<Submission> loaded;
    std::vector<std::string> warnings;
    if (!loadFolder(dir, st.settings, loaded, warnings)) {
        for (const std::string& w : warnings) std::cout << "  Error: " << w << "\n";
        return;   // keep whatever was loaded before
    }
    st.subs = std::move(loaded);
    st.warnings = warnings;
    afterLoad(st);
}

void doRun(AppState& st) {
    for (Submission& s : st.subs) retokenize(s, st.settings);
    const std::size_t usable = countUsable(st.subs);
    if (usable < 2) {
        std::cout << "\nNeed at least two usable files to compare. Use option 1 or 2 to load files";
        std::cout << (st.subs.empty() ? ".\n" : ", or option 3 to see why files were skipped.\n");
        return;
    }
    std::cout << "\nComparing " << usable << " file(s) (" << usable * (usable - 1) / 2 << " pair(s))...\n";
    st.results = compareAll(st.subs, st.settings, [](std::size_t done, std::size_t total) {
        if (total > 25 && (done % 10 == 0 || done == total)) {
            std::cout << "\r  Progress: " << done << " / " << total << " pairs" << std::flush;
            if (done == total) std::cout << "\n";
        }
    });
    st.haveResults = true;
    st.stale = false;
    printSettings(std::cout, st.settings);
    printSummary(std::cout, st.subs, st.results, st.settings);
    std::cout << "\nUse option 6 to see the matching code of a pair.\n";
}

bool requireResults(const AppState& st) {
    if (!st.haveResults) {
        std::cout << "\nNo results yet. Load files (option 1 or 2) and run the comparison (option 4).\n";
        return false;
    }
    if (st.stale) std::cout << "\n(Note: settings changed since this run - choose option 4 to refresh the results.)\n";
    return true;
}

void doDetail(AppState& st) {
    if (!requireResults(st)) return;
    if (st.results.empty()) { std::cout << "No pairs were compared.\n"; return; }
    int rank = 0;
    if (!askInt("Rank number of the pair to inspect (1-" + std::to_string(st.results.size()) + ", blank to cancel): ",
                1, static_cast<int>(st.results.size()), rank)) return;
    printPairDetail(std::cout, st.subs, st.results[static_cast<std::size_t>(rank) - 1], st.settings,
                    static_cast<std::size_t>(rank));
}

void doSave(AppState& st) {
    if (!requireResults(st)) return;
    std::string path;
    if (!readLine("Report file name (blank = codelens_report.txt): ", path)) return;
    path = trimPath(path);
    if (path.empty()) path = "codelens_report.txt";
    std::error_code ec;
    if (fs::exists(fs::path(path), ec) && !askYesNo("  '" + path + "' already exists. Overwrite?")) {
        std::cout << "  Cancelled.\n";
        return;
    }
    std::ofstream out(path);
    if (!out) {
        std::cout << "  Error: cannot write to '" << path << "' (check the folder and permissions).\n";
        return;
    }
    writeFullReport(out, st.subs, st.results, st.settings, st.warnings);
    out.flush();
    if (!out) std::cout << "  Error: writing the report failed.\n";
    else std::cout << "  Report saved to: " << path << "\n";
}

void showSettingsMenu(AppState& st) {
    Settings& s = st.settings;
    for (;;) {
        std::cout << "\nSETTINGS\n"
                  << " 1) k-gram size (tokens)           : " << s.k << "\n"
                  << " 2) Winnowing window (k-grams)     : " << s.window << "\n"
                  << " 3) Ignore #include lines          : " << (s.ignoreIncludes ? "yes" : "no") << "\n"
                  << " 4) Normalise literals (NUM/STR)   : " << (s.normalizeLiterals ? "yes" : "no") << "\n"
                  << " 5) Token-order (LCS) score        : " << (s.computeLcs ? "on" : "off") << "\n"
                  << " 6) Hide pairs below (% similarity): " << s.minShowScore << "\n"
                  << " 7) Matching sections shown / pair : " << s.maxExcerpts << "\n"
                  << " 8) Max lines shown per excerpt    : " << s.maxExcerptLines << "\n"
                  << " 9) Reset to defaults\n"
                  << " 0) Back\n"
                  << "    (shared runs of " << (s.k + s.window - 1) << "+ tokens are always detected)\n";
        int c = 0;
        if (!askInt("Choose a setting: ", 0, 9, c) || c == 0) return;
        bool changed = true;
        switch (c) {
            case 1: changed = askInt("  New k (2-30): ", 2, 30, s.k); break;
            case 2: changed = askInt("  New window (1-30): ", 1, 30, s.window); break;
            case 3: s.ignoreIncludes = !s.ignoreIncludes; break;
            case 4: s.normalizeLiterals = !s.normalizeLiterals; break;
            case 5: s.computeLcs = !s.computeLcs; break;
            case 6: changed = askDouble("  Minimum similarity to show (0-100): ", 0.0, 100.0, s.minShowScore); break;
            case 7: changed = askInt("  Sections per pair (1-50): ", 1, 50, s.maxExcerpts); break;
            case 8: changed = askInt("  Lines per excerpt (1-100): ", 1, 100, s.maxExcerptLines); break;
            case 9: s = Settings(); break;
            default: changed = false; break;
        }
        if (changed) {
            for (Submission& sub : st.subs) retokenize(sub, s);
            if (st.haveResults) st.stale = true;
        }
    }
}

void showHelp() {
    std::cout << R"(
HOW CODELENS WORKS
  1. Reads local C++ files as text (nothing is ever compiled or executed).
  2. Splits them into tokens; comments and spacing are ignored.
  3. Renames your own identifiers to ID1, ID2, ... so renamed variables look the same.
  4. Hashes overlapping groups of k tokens (rolling hash) and keeps representative
     hashes (winnowing).
  5. Compares fingerprints and verifies them against the real tokens.
  6. Reports a similarity score, a ranked list and the matching code with line numbers.

READING THE RESULT
  Similarity = shared fingerprints / all distinct fingerprints (Jaccard) x 100.
  It is a review signal, not proof of copying - standard solutions and shared templates
  also look alike. The teacher makes the final judgment.

LIMITATIONS
  No full C++ parsing or macro expansion; identifier mapping is not scope-aware;
  different implementations of the same logic are not detected.
)";
}

void printMenu(const AppState& st) {
    std::cout << "\n=============== CodeLens ===============\n"
              << " Files loaded: " << st.subs.size() << " (" << countUsable(st.subs) << " usable)   k=" << st.settings.k
              << "  window=" << st.settings.window << "\n"
              << " 1) Select two C++ files\n"
              << " 2) Select a folder of submissions\n"
              << " 3) Show loaded files\n"
              << " 4) Run comparison\n"
              << " 5) View ranked results\n"
              << " 6) View matching code of a pair\n"
              << " 7) Settings\n"
              << " 8) Save text report\n"
              << " 9) Help\n"
              << " 0) Exit\n";
}

}  // namespace

int runMenu() {
    AppState st;
    std::cout << "CodeLens - C++ source-code similarity detector\n"
              << "Everything runs locally; submitted programs are read as text and never executed.\n";
    for (;;) {
        printMenu(st);
        int choice = 0;
        if (!askInt("Choose an option: ", 0, 9, choice)) {
            if (std::cin.eof()) break;
            continue;
        }
        switch (choice) {
            case 1: doSelectTwoFiles(st); break;
            case 2: doSelectFolder(st); break;
            case 3: showFiles(st); break;
            case 4: doRun(st); break;
            case 5: if (requireResults(st)) printSummary(std::cout, st.subs, st.results, st.settings); break;
            case 6: doDetail(st); break;
            case 7: showSettingsMenu(st); break;
            case 8: doSave(st); break;
            case 9: showHelp(); break;
            case 0: std::cout << "Goodbye.\n"; return 0;
            default: break;
        }
        if (std::cin.eof()) break;
    }
    return 0;
}

}  // namespace codelens
