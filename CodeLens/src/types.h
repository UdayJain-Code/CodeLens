// types.h - shared data types for CodeLens
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace codelens {

enum class TokenType { Keyword, Identifier, Number, String, Char, Operator, Symbol };

struct Token {
    TokenType type = TokenType::Symbol;
    std::string text;          // original lexeme, exactly as written
    std::string norm;          // normalised lexeme (ID1, NUM, ...), set by normaliser
    std::uint64_t hash = 0;    // 64-bit hash of `norm`
    int line = 0;              // 1-based line where the token starts
    int endLine = 0;           // 1-based line where the token ends
};

// All tunable options. Defaults are chosen so that a shared run of
// k + window - 1 = 10 tokens is always detected (winnowing guarantee).
struct Settings {
    int k = 7;                          // k-gram size, in tokens
    int window = 4;                     // winnowing window, in k-grams
    bool ignoreIncludes = true;         // drop '#include' lines (common templates)
    bool normalizeLiterals = true;      // numbers -> NUM, strings -> STR, chars -> CHR
    bool computeLcs = false;            // optional token-order score
    double minShowScore = 0.0;          // hide pairs below this similarity (%)
    int maxExcerpts = 5;                // matching sections shown per pair
    int maxExcerptLines = 12;           // lines shown per excerpt

    std::size_t minTokens = 4;                  // fewer tokens => "insufficient code"
    std::size_t maxFileBytes = 1024 * 1024;     // refuse files larger than 1 MB
    std::size_t maxFiles = 300;                 // folder load limit
    std::size_t lcsMaxCells = 25000000;         // LCS table limit (n * m)
    std::size_t maxPositionsPerHash = 32;       // cap on repeated-pattern blow-up
    std::size_t maxStoredRegions = 30;          // regions kept per pair
};

// One student file. Source is kept as lines (for excerpts); tokens are derived.
struct Submission {
    std::string path;
    std::string name;                   // display name
    std::vector<std::string> lines;     // original lines; lines[i] is line i+1
    std::vector<Token> tokens;
    bool loaded = false;                // file was read successfully
    bool usable = false;                // loaded and has enough code to compare
    std::string note;                   // reason when not loaded/usable
};

}  // namespace codelens
