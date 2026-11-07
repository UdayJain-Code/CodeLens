// normalizer.h - rename-proof token labels (ID1, ID2, ...) and token hashing
#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "types.h"

namespace codelens {

std::uint64_t fnv1a64(const std::string& s);
bool isKnownLibraryName(const std::string& name);

// Fills Token::norm and Token::hash.
//  * keywords, operators and symbols stay unchanged
//  * user-defined identifiers become ID1, ID2, ... (consistent within the file)
//  * well-known library names (cout, vector, ...) stay unchanged
//  * optionally: numbers -> NUM, strings -> STR, chars -> CHR
void normalizeTokens(std::vector<Token>& tokens, bool normalizeLiterals);

}  // namespace codelens
