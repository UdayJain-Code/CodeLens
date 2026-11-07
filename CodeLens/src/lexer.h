// lexer.h - small hand-written lexer (state machine) for C/C++ source text
#pragma once
#include <string>
#include <vector>

#include "types.h"

namespace codelens {

bool isKeyword(const std::string& word);

// Splits source into tokens. Comments and whitespace are skipped; string,
// char and raw-string literals are kept intact (so "//" inside a string is
// not mistaken for a comment). Each token remembers its original line numbers.
std::vector<Token> tokenize(const std::string& source, bool ignoreIncludes);

}  // namespace codelens
