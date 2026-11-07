// pipeline.h - load files, tokenise and normalise them
#pragma once
#include <string>
#include <vector>

#include "types.h"

namespace codelens {

// Re-run lexer + normaliser (needed after settings such as ignoreIncludes change).
void retokenize(Submission& sub, const Settings& s);

Submission loadSubmission(const std::string& path, const std::string& displayName, const Settings& s);

// Two individual files. Names collide -> full paths are used as names.
std::vector<Submission> loadTwoFiles(const std::string& pathA, const std::string& pathB, const Settings& s);

// All C/C++ files below `dir`. Returns false (with a message in warnings) if nothing could be loaded.
bool loadFolder(const std::string& dir, const Settings& s, std::vector<Submission>& out,
                std::vector<std::string>& warnings);

std::size_t countUsable(const std::vector<Submission>& subs);

}  // namespace codelens
