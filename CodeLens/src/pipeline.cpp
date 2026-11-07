#include "pipeline.h"

#include "file_loader.h"
#include "lexer.h"
#include "normalizer.h"

namespace codelens {

namespace {
std::string joinLines(const std::vector<std::string>& lines) {
    std::string s;
    for (const std::string& l : lines) { s += l; s += '\n'; }
    return s;
}
}  // namespace

void retokenize(Submission& sub, const Settings& s) {
    sub.tokens.clear();
    sub.usable = false;
    if (!sub.loaded) return;
    sub.tokens = tokenize(joinLines(sub.lines), s.ignoreIncludes);
    normalizeTokens(sub.tokens, s.normalizeLiterals);
    if (sub.tokens.empty()) {
        sub.note = "empty or comment-only file: insufficient code";
        return;
    }
    if (sub.tokens.size() < s.minTokens) {
        sub.note = "only " + std::to_string(sub.tokens.size()) + " token(s): insufficient code";
        return;
    }
    sub.note.clear();
    sub.usable = true;
}

Submission loadSubmission(const std::string& path, const std::string& displayName, const Settings& s) {
    Submission sub;
    sub.path = path;
    sub.name = displayName;
    ReadResult r = readTextFile(path, s.maxFileBytes);
    if (!r.ok) {
        sub.note = r.error;
        return sub;
    }
    sub.lines = splitLines(r.content);
    sub.loaded = true;
    retokenize(sub, s);
    return sub;
}

std::vector<Submission> loadTwoFiles(const std::string& pathA, const std::string& pathB, const Settings& s) {
    std::string nameA = fileNameOnly(pathA), nameB = fileNameOnly(pathB);
    if (nameA == nameB) { nameA = pathA; nameB = pathB; }
    std::vector<Submission> subs;
    subs.push_back(loadSubmission(pathA, nameA, s));
    subs.push_back(loadSubmission(pathB, nameB, s));
    return subs;
}

bool loadFolder(const std::string& dir, const Settings& s, std::vector<Submission>& out,
                std::vector<std::string>& warnings) {
    FolderScan scan = scanFolder(dir, s.maxFiles);
    if (!scan.error.empty()) {
        warnings.push_back(scan.error);
        return false;
    }
    if (scan.files.empty()) {
        warnings.push_back("no C/C++ source files (.cpp .cc .cxx .c .h .hpp) found in: " + dir);
        return false;
    }
    if (scan.truncated) {
        warnings.push_back("folder has more than " + std::to_string(s.maxFiles) + " files; only the first " +
                           std::to_string(s.maxFiles) + " were loaded");
    }
    out.clear();
    for (const std::string& path : scan.files) {
        out.push_back(loadSubmission(path, displayName(path, dir), s));
        if (!out.back().usable) warnings.push_back(out.back().name + ": " + out.back().note);
    }
    return true;
}

std::size_t countUsable(const std::vector<Submission>& subs) {
    std::size_t n = 0;
    for (const Submission& s : subs) if (s.usable) ++n;
    return n;
}

}  // namespace codelens
