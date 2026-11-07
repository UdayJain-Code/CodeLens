#include "file_loader.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <system_error>

namespace fs = std::filesystem;

namespace codelens {

std::string trim(const std::string& s) {
    std::size_t b = 0, e = s.size();
    while (b < e && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
    return s.substr(b, e - b);
}

std::string trimPath(const std::string& s) {
    std::string t = trim(s);
    if (t.size() >= 2 && ((t.front() == '"' && t.back() == '"') || (t.front() == '\'' && t.back() == '\''))) {
        t = t.substr(1, t.size() - 2);
    }
    return t;
}

ReadResult readTextFile(const std::string& path, std::size_t maxBytes) {
    ReadResult r;
    std::error_code ec;
    fs::path p(path);
    if (!fs::exists(p, ec) || ec) { r.error = "file not found"; return r; }
    if (!fs::is_regular_file(p, ec) || ec) { r.error = "not a regular file"; return r; }
    std::uintmax_t size = fs::file_size(p, ec);
    if (ec) { r.error = "cannot read file size"; return r; }
    if (size > maxBytes) {
        r.error = "file too large (" + std::to_string(size) + " bytes, limit " + std::to_string(maxBytes) + ")";
        return r;
    }
    std::ifstream in(p, std::ios::binary);
    if (!in) { r.error = "cannot open file (permission denied?)"; return r; }
    std::ostringstream ss;
    ss << in.rdbuf();
    r.content = ss.str();
    if (r.content.find('\0') != std::string::npos) {
        r.content.clear();
        r.error = "looks like a binary file, not source text";
        return r;
    }
    if (r.content.size() >= 3 && static_cast<unsigned char>(r.content[0]) == 0xEF &&
        static_cast<unsigned char>(r.content[1]) == 0xBB && static_cast<unsigned char>(r.content[2]) == 0xBF) {
        r.content.erase(0, 3);   // UTF-8 byte order mark
    }
    r.ok = true;
    return r;
}

std::vector<std::string> splitLines(const std::string& text) {
    std::vector<std::string> lines;
    std::string cur;
    for (char c : text) {
        if (c == '\n') {
            if (!cur.empty() && cur.back() == '\r') cur.pop_back();
            lines.push_back(cur);
            cur.clear();
        } else {
            cur.push_back(c);
        }
    }
    if (!cur.empty()) {
        if (cur.back() == '\r') cur.pop_back();
        lines.push_back(cur);
    }
    return lines;
}

bool hasSourceExtension(const std::string& path) {
    std::string ext = fs::path(path).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    static const char* exts[] = {".cpp", ".cc", ".cxx", ".c++", ".c", ".h", ".hpp", ".hh", ".hxx"};
    for (const char* e : exts) if (ext == e) return true;
    return false;
}

FolderScan scanFolder(const std::string& dir, std::size_t maxFiles) {
    FolderScan scan;
    std::error_code ec;
    fs::path p(dir);
    if (!fs::exists(p, ec) || ec) { scan.error = "folder not found: " + dir; return scan; }
    if (!fs::is_directory(p, ec) || ec) { scan.error = "not a folder: " + dir; return scan; }

    fs::recursive_directory_iterator it(p, fs::directory_options::skip_permission_denied, ec), end;
    if (ec) { scan.error = "cannot read folder: " + dir; return scan; }
    for (; it != end; it.increment(ec)) {
        if (ec) break;
        std::error_code e2;
        if (it->is_regular_file(e2) && !e2 && hasSourceExtension(it->path().string())) {
            scan.files.push_back(it->path().string());
        }
    }
    std::sort(scan.files.begin(), scan.files.end());
    if (scan.files.size() > maxFiles) {
        scan.files.resize(maxFiles);
        scan.truncated = true;
    }
    return scan;
}

std::string displayName(const std::string& path, const std::string& baseDir) {
    std::error_code ec;
    fs::path rel = fs::relative(fs::path(path), fs::path(baseDir), ec);
    if (ec || rel.empty()) return fs::path(path).filename().generic_string();
    return rel.generic_string();
}

std::string fileNameOnly(const std::string& path) {
    return fs::path(path).filename().generic_string();
}

}  // namespace codelens
