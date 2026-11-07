// file_loader.h - reading local source files (never executes them)
#pragma once
#include <cstddef>
#include <string>
#include <vector>

namespace codelens {

struct ReadResult {
    bool ok = false;
    std::string error;
    std::string content;
};

struct FolderScan {
    std::vector<std::string> files;
    std::string error;
    bool truncated = false;
};

std::string trim(const std::string& s);
std::string trimPath(const std::string& s);   // trims spaces and surrounding quotes

ReadResult readTextFile(const std::string& path, std::size_t maxBytes);
std::vector<std::string> splitLines(const std::string& text);
bool hasSourceExtension(const std::string& path);
FolderScan scanFolder(const std::string& dir, std::size_t maxFiles);
std::string displayName(const std::string& path, const std::string& baseDir);
std::string fileNameOnly(const std::string& path);

}  // namespace codelens
