#include "lexer.h"

#include <unordered_set>

namespace codelens {

namespace {

bool isAlpha(unsigned char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
bool isDigit(unsigned char c) { return c >= '0' && c <= '9'; }
bool isIdentStart(unsigned char c) { return isAlpha(c) || c == '_' || c >= 0x80; }
bool isIdentChar(unsigned char c) { return isAlpha(c) || isDigit(c) || c == '_' || c >= 0x80; }

const std::unordered_set<std::string>& keywordSet() {
    static const std::unordered_set<std::string> kw = {
        "alignas", "alignof", "and", "and_eq", "asm", "auto", "bitand", "bitor", "bool", "break",
        "case", "catch", "char", "char16_t", "char32_t", "class", "compl", "const", "constexpr",
        "const_cast", "continue", "decltype", "default", "delete", "do", "double", "dynamic_cast",
        "else", "enum", "explicit", "export", "extern", "false", "final", "float", "for", "friend",
        "goto", "if", "inline", "int", "long", "mutable", "namespace", "new", "noexcept", "not",
        "not_eq", "nullptr", "operator", "or", "or_eq", "override", "private", "protected", "public",
        "register", "reinterpret_cast", "return", "short", "signed", "sizeof", "static",
        "static_assert", "static_cast", "struct", "switch", "template", "this", "thread_local",
        "throw", "true", "try", "typedef", "typeid", "typename", "union", "unsigned", "using",
        "virtual", "void", "volatile", "wchar_t", "while", "xor", "xor_eq"};
    return kw;
}

}  // namespace

bool isKeyword(const std::string& word) { return keywordSet().count(word) > 0; }

std::vector<Token> tokenize(const std::string& s, bool ignoreIncludes) {
    std::vector<Token> out;
    const std::size_t n = s.size();
    std::size_t i = 0;
    int line = 1;
    bool lineStart = true;   // only whitespace/comments seen so far on this line
    bool afterHash = false;  // previous token was a line-leading '#'

    auto emit = [&](TokenType type, std::size_t from, std::size_t to, int startLine, int endLine) {
        Token t;
        t.type = type;
        t.text = s.substr(from, to - from);
        t.line = startLine;
        t.endLine = endLine;
        out.push_back(std::move(t));
        lineStart = false;
        afterHash = false;
    };

    // Skip to end of line, honouring backslash-newline continuations.
    auto skipLine = [&]() {
        while (i < n && s[i] != '\n') {
            if (s[i] == '\\') {
                if (i + 1 < n && s[i + 1] == '\n') { i += 2; ++line; continue; }
                if (i + 2 < n && s[i + 1] == '\r' && s[i + 2] == '\n') { i += 3; ++line; continue; }
            }
            ++i;
        }
    };

    // "..." or '...' literal starting at quotePos (optional prefix begins at `from`).
    auto lexQuoted = [&](std::size_t from, std::size_t quotePos, TokenType type) {
        const char q = s[quotePos];
        std::size_t j = quotePos + 1;
        const int startLine = line;
        while (j < n && s[j] != '\n') {
            if (s[j] == '\\' && j + 1 < n) {
                if (s[j + 1] == '\n') ++line;
                j += 2;
                continue;
            }
            if (s[j] == q) { ++j; break; }
            ++j;
        }
        emit(type, from, j, startLine, line);
        i = j;
    };

    // R"delim( ... )delim"
    auto lexRaw = [&](std::size_t from, std::size_t quotePos) {
        std::size_t j = quotePos + 1;
        std::string delim;
        while (j < n && s[j] != '(' && s[j] != '"' && s[j] != ' ' && s[j] != '\n' && s[j] != '\\' &&
               s[j] != ')' && delim.size() <= 16) {
            delim.push_back(s[j]);
            ++j;
        }
        if (j >= n || s[j] != '(') {          // malformed: treat as an ordinary string
            lexQuoted(from, quotePos, TokenType::String);
            return;
        }
        const std::string closing = ")" + delim + "\"";
        const std::size_t endPos = s.find(closing, j + 1);
        const std::size_t stop = (endPos == std::string::npos) ? n : endPos + closing.size();
        const int startLine = line;
        for (std::size_t x = j; x < stop; ++x) if (s[x] == '\n') ++line;
        emit(TokenType::String, from, stop, startLine, line);
        i = stop;
    };

    static const char* ops3[] = {"<<=", ">>=", "<=>", "...", "->*"};
    static const char* ops2[] = {"::", "->", "++", "--", "<<", ">>", "<=", ">=", "==", "!=", "&&", "||",
                                 "+=", "-=", "*=", "/=", "%=", "&=", "|=", "^=", ".*", "##"};
    static const std::string singleOps = "+-*/%=<>!&|^~?:.";

    while (i < n) {
        const unsigned char c = static_cast<unsigned char>(s[i]);

        if (c == '\n') { ++line; ++i; lineStart = true; afterHash = false; continue; }
        if (c == ' ' || c == '\t' || c == '\r' || c == '\f' || c == '\v') { ++i; continue; }

        // Comments: skipped, but line numbers are still counted.
        if (c == '/' && i + 1 < n && s[i + 1] == '/') { skipLine(); continue; }
        if (c == '/' && i + 1 < n && s[i + 1] == '*') {
            i += 2;
            while (i < n && !(s[i] == '*' && i + 1 < n && s[i + 1] == '/')) {
                if (s[i] == '\n') ++line;
                ++i;
            }
            i = (i < n) ? i + 2 : n;   // unterminated comment: consume to end
            continue;
        }

        // Preprocessor directive.
        if (c == '#' && lineStart) {
            std::size_t j = i + 1;
            while (j < n && (s[j] == ' ' || s[j] == '\t')) ++j;
            std::size_t k = j;
            while (k < n && isIdentChar(static_cast<unsigned char>(s[k]))) ++k;
            const std::string directive = s.substr(j, k - j);
            if (ignoreIncludes && (directive == "include" || directive == "include_next")) {
                i = k;
                skipLine();
                continue;
            }
            emit(TokenType::Symbol, i, i + 1, line, line);
            afterHash = true;
            ++i;
            continue;
        }

        // Identifiers, keywords, and prefixed literals (L"..", u8"..", R"(..)").
        if (isIdentStart(c)) {
            std::size_t j = i;
            while (j < n && isIdentChar(static_cast<unsigned char>(s[j]))) ++j;
            const std::string word = s.substr(i, j - i);
            const bool strPrefix = (word == "L" || word == "u" || word == "U" || word == "u8");
            const bool rawPrefix = (word == "R" || word == "LR" || word == "uR" || word == "UR" || word == "u8R");
            if (j < n && s[j] == '"' && rawPrefix) { lexRaw(i, j); continue; }
            if (j < n && s[j] == '"' && strPrefix) { lexQuoted(i, j, TokenType::String); continue; }
            if (j < n && s[j] == '\'' && strPrefix) { lexQuoted(i, j, TokenType::Char); continue; }
            const bool kw = isKeyword(word) || afterHash;
            emit(kw ? TokenType::Keyword : TokenType::Identifier, i, j, line, line);
            i = j;
            continue;
        }

        // Numbers (decimal, hex, floats, digit separators, suffixes).
        if (isDigit(c) || (c == '.' && i + 1 < n && isDigit(static_cast<unsigned char>(s[i + 1])))) {
            const bool hex = (c == '0' && i + 1 < n && (s[i + 1] == 'x' || s[i + 1] == 'X'));
            std::size_t j = i;
            while (j < n) {
                const unsigned char d = static_cast<unsigned char>(s[j]);
                if (isAlpha(d) || isDigit(d) || d == '_' || d == '.') {
                    const bool sign = j + 1 < n && (s[j + 1] == '+' || s[j + 1] == '-');
                    if (sign && ((!hex && (d == 'e' || d == 'E')) || (hex && (d == 'p' || d == 'P')))) {
                        j += 2;
                        continue;
                    }
                    ++j;
                } else if (d == '\'' && j + 1 < n && (isAlpha(static_cast<unsigned char>(s[j + 1])) ||
                                                       isDigit(static_cast<unsigned char>(s[j + 1])))) {
                    ++j;   // digit separator: 1'000'000
                } else {
                    break;
                }
            }
            emit(TokenType::Number, i, j, line, line);
            i = j;
            continue;
        }

        if (c == '"') { lexQuoted(i, i, TokenType::String); continue; }
        if (c == '\'') { lexQuoted(i, i, TokenType::Char); continue; }

        // Operators: longest match first.
        bool matched = false;
        for (const char* op : ops3) {
            if (s.compare(i, 3, op) == 0) { emit(TokenType::Operator, i, i + 3, line, line); i += 3; matched = true; break; }
        }
        if (matched) continue;
        for (const char* op : ops2) {
            if (s.compare(i, 2, op) == 0) { emit(TokenType::Operator, i, i + 2, line, line); i += 2; matched = true; break; }
        }
        if (matched) continue;
        if (singleOps.find(static_cast<char>(c)) != std::string::npos) {
            emit(TokenType::Operator, i, i + 1, line, line);
        } else {
            emit(TokenType::Symbol, i, i + 1, line, line);   // ( ) { } [ ] ; , # and anything unknown
        }
        ++i;
    }
    return out;
}

}  // namespace codelens
