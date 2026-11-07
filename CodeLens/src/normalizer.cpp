#include "normalizer.h"

#include <unordered_map>
#include <unordered_set>

namespace codelens {

std::uint64_t fnv1a64(const std::string& s) {
    std::uint64_t h = 1469598103934665603ULL;
    for (unsigned char c : s) {
        h ^= c;
        h *= 1099511628211ULL;
    }
    return h;
}

bool isKnownLibraryName(const std::string& name) {
    static const std::unordered_set<std::string> known = {
        "std", "cout", "cin", "cerr", "clog", "endl", "string", "vector", "map", "unordered_map",
        "set", "unordered_set", "multiset", "multimap", "pair", "make_pair", "array", "deque",
        "queue", "priority_queue", "stack", "bitset", "size_t", "printf", "scanf", "puts",
        "getchar", "putchar", "getline", "sort", "swap", "abs", "sqrt", "pow", "push_back",
        "pop_back", "main", "ios", "ifstream", "ofstream", "fstream", "stringstream", "memset",
        "strlen", "strcmp", "malloc", "free", "to_string", "stoi", "stod", "INT_MAX", "INT_MIN",
        "NULL", "fixed", "setprecision", "setw"};
    return known.count(name) > 0;
}

void normalizeTokens(std::vector<Token>& tokens, bool normalizeLiterals) {
    std::unordered_map<std::string, int> labels;   // identifier -> label number
    for (Token& t : tokens) {
        switch (t.type) {
            case TokenType::Identifier:
                if (isKnownLibraryName(t.text)) {
                    t.norm = t.text;
                } else {
                    auto it = labels.find(t.text);
                    if (it == labels.end()) {
                        const int next = static_cast<int>(labels.size()) + 1;
                        it = labels.emplace(t.text, next).first;
                    }
                    t.norm = "ID" + std::to_string(it->second);
                }
                break;
            case TokenType::Number: t.norm = normalizeLiterals ? "NUM" : t.text; break;
            case TokenType::String: t.norm = normalizeLiterals ? "STR" : t.text; break;
            case TokenType::Char:   t.norm = normalizeLiterals ? "CHR" : t.text; break;
            default:                t.norm = t.text; break;
        }
        t.hash = fnv1a64(t.norm);
    }
}

}  // namespace codelens
