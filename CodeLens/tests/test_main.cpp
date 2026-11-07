// Self-contained tests (no framework needed). Build and run with:  make test
#include <cmath>
#include <iostream>
#include <random>

#include "file_loader.h"
#include "fingerprint.h"
#include "lexer.h"
#include "normalizer.h"
#include "pipeline.h"
#include "similarity.h"

using namespace codelens;

static int g_total = 0, g_fail = 0;
#define CHECK(cond)                                                                   \
    do {                                                                              \
        ++g_total;                                                                    \
        if (!(cond)) {                                                                \
            ++g_fail;                                                                 \
            std::cerr << "FAIL  line " << __LINE__ << ":  " #cond << "\n";            \
        }                                                                             \
    } while (0)

static Submission make(const std::string& name, const std::string& code, const Settings& s) {
    Submission sub;
    sub.name = name;
    sub.lines = splitLines(code);
    sub.loaded = true;
    retokenize(sub, s);
    return sub;
}

static std::string joinNorm(const std::vector<Token>& t) {
    std::string out;
    for (const Token& x : t) out += (out.empty() ? "" : " ") + x.norm;
    return out;
}

static void testLexer() {
    const std::string code =
        "int x = 5; // note\n"
        "/* a\n"
        "b */ char* s = \"// not /* comment */\";\n";
    auto toks = tokenize(code, true);
    bool sawString = false, sawComment = false;
    int charLine = 0;
    for (const Token& t : toks) {
        if (t.text == "\"// not /* comment */\"") sawString = true;
        if (t.text.rfind("//", 0) == 0 || t.text == "note") sawComment = true;
        if (t.text == "char") charLine = t.line;
    }
    CHECK(sawString);
    CHECK(!sawComment);
    CHECK(charLine == 3);

    auto raw = tokenize("auto s = R\"x(a \"q\" // not comment)x\"; int y;", true);
    CHECK(raw.size() == 8);
    CHECK(raw[3].type == TokenType::String);

    auto inc = tokenize("#include <vector>\nint a;\n", true);
    CHECK(inc.size() == 3);
    auto inc2 = tokenize("#include <vector>\nint a;\n", false);
    CHECK(inc2.size() > 3);

    auto nums = tokenize("x = 0x1F + 1'000 + 3.14e-2f;", true);
    int numberCount = 0;
    for (const Token& t : nums) if (t.type == TokenType::Number) ++numberCount;
    CHECK(numberCount == 3);
}

static void testNormalizer() {
    auto a = tokenize("int total = a + b;", true);
    auto b = tokenize("int answer = x + y;", true);
    normalizeTokens(a, true);
    normalizeTokens(b, true);
    CHECK(joinNorm(a) == "int ID1 = ID2 + ID3 ;");
    CHECK(joinNorm(a) == joinNorm(b));

    auto c = tokenize("cout << x << x;", true);
    normalizeTokens(c, true);
    CHECK(joinNorm(c) == "cout << ID1 << ID1 ;");
}

static void testHashing() {
    Settings s;
    auto toks = tokenize("a b c d e f g h i j", true);
    normalizeTokens(toks, true);
    const int k = 3;
    auto rolled = rollingKgramHashes(toks, k);
    CHECK(rolled.size() == toks.size() - k + 1);
    bool allEqual = true;
    for (std::size_t i = 0; i < rolled.size(); ++i) {
        std::uint64_t h = 0;
        for (int j = 0; j < k; ++j) h = h * kHashBase + toks[i + j].hash;
        if (mix64(h) != rolled[i]) allEqual = false;
    }
    CHECK(allEqual);
    CHECK(rollingKgramHashes(toks, 50).empty());

    // Winnowing guarantee: every window of w hashes contains a selected position.
    std::mt19937 rng(12345);
    bool guarantee = true;
    for (int round = 0; round < 50; ++round) {
        std::vector<std::uint64_t> h(60);
        for (auto& x : h) x = rng() % 20;   // many ties on purpose
        const int w = 1 + round % 6;
        auto fps = winnow(h, w);
        for (std::size_t st = 0; st + w <= h.size(); ++st) {
            bool found = false;
            for (const Fingerprint& f : fps) if (f.pos >= st && f.pos < st + w) found = true;
            if (!found) guarantee = false;
        }
        for (const Fingerprint& f : fps) {   // each pick equals the window minimum
            if (h[f.pos] != f.hash) guarantee = false;
        }
    }
    CHECK(guarantee);
    CHECK(winnow({}, 4).empty());
    CHECK(winnow({5, 3, 9}, 10).size() == 1);   // window larger than input
}

static void testJaccard() {
    CHECK(std::fabs(jaccardPercent(3, 4, 4) - 60.0) < 1e-9);   // the worked example in the overview
    CHECK(jaccardPercent(0, 0, 0) == 0.0);
    CHECK(std::fabs(jaccardPercent(5, 5, 5) - 100.0) < 1e-9);
}

static void testSimilarity() {
    Settings s;
    const std::string original =
        "#include <iostream>\nusing namespace std;\n"
        "int main() {\n  int n; cin >> n; int sum = 0;\n"
        "  for (int i = 0; i < n; i++) { int v; cin >> v; sum += v; }\n"
        "  cout << \"Total: \" << sum << endl;\n  return 0;\n}\n";
    const std::string renamed =
        "// totally different comments\nusing namespace std;\n"
        "int main()\n{\n int count;\n cin >> count;\n int acc = 0;\n"
        " for (int j = 0; j < count; j++)\n {\n  int item;\n  cin >> item;\n  acc += item;\n }\n"
        " cout << \"Sum is \" << acc << endl;\n return 0;\n}\n";
    const std::string different =
        "using namespace std;\n"
        "bool check(int n) { if (n < 2) return false; for (int d = 2; d * d <= n; d++) { if (n % d == 0) return false; } return true; }\n"
        "int main() { int limit; cin >> limit; while (limit > 0) { if (check(limit)) cout << limit << \"\\n\"; limit -= 3; } return 7; }\n";

    Submission a = make("a.cpp", original, s);
    Submission b = make("b.cpp", renamed, s);
    Submission c = make("c.cpp", different, s);
    CHECK(a.usable && b.usable && c.usable);

    PairResult same = comparePair(a, a, s);
    CHECK(same.compared && same.score > 99.99);
    CHECK(!same.regions.empty());
    if (!same.regions.empty()) CHECK(same.regions[0].aLineStart >= 1);

    PairResult ren = comparePair(a, b, s);
    CHECK(ren.score > 99.99);               // renaming/reformatting/comment changes do not matter
    CHECK(ren.coverageA > 99.0 && ren.coverageB > 99.0);

    PairResult diff = comparePair(a, c, s);
    CHECK(diff.score < 50.0);
    CHECK(diff.score < ren.score);

    // ranking
    std::vector<Submission> all = {c, a, b};
    auto ranked = compareAll(all, s);
    CHECK(ranked.size() == 3);
    CHECK(ranked[0].score >= ranked[1].score && ranked[1].score >= ranked[2].score);
    CHECK((ranked[0].a == 1 && ranked[0].b == 2));   // a.cpp <-> b.cpp is most similar

    // verified matching region lines
    Settings s2;
    Submission p = make("p.cpp", "int f(int a) {\n return a + 1;\n}\nint g(int b) {\n return b * 2;\n}\n", s2);
    Submission q = make("q.cpp", "using namespace std;\nint f(int a) {\n return a + 1;\n}\nint g(int b) {\n return b * 2;\n}\n", s2);
    PairResult pq = comparePair(p, q, s2);
    CHECK(!pq.regions.empty());
    if (!pq.regions.empty()) {
        CHECK(pq.regions[0].aLineStart == 1);
        CHECK(pq.regions[0].bLineStart == 2);   // q has one extra line at the top
    }

    // optional LCS
    Settings s3;
    s3.computeLcs = true;
    PairResult lcs = comparePair(a, b, s3);
    CHECK(lcs.lcsComputed && lcs.lcsScore > 99.0);
    s3.lcsMaxCells = 10;
    CHECK(comparePair(a, b, s3).lcsSkipped);
}

static void testEdgeCases() {
    Settings s;
    Submission empty = make("empty.cpp", "", s);
    Submission comments = make("comments.cpp", "// nothing\n/* here */\n", s);
    CHECK(!empty.usable && !empty.note.empty());
    CHECK(!comments.usable);

    Submission shortA = make("s1.cpp", "int main(){return 0;}", s);
    Submission shortB = make("s2.cpp", "int main(){return 1;}", s);
    CHECK(shortA.usable && shortB.usable);
    PairResult r = comparePair(shortA, shortB, s);
    CHECK(r.compared && r.shortFallback);
    CHECK(r.score > 99.0);   // literals are normalised, so these two are the same shape

    PairResult bad = comparePair(empty, shortA, s);
    CHECK(!bad.compared);

    ReadResult missing = readTextFile("/definitely/not/here.cpp", 1000);
    CHECK(!missing.ok && !missing.error.empty());

    FolderScan nofolder = scanFolder("/definitely/not/here", 10);
    CHECK(!nofolder.error.empty());

    Submission loaded = loadSubmission("/definitely/not/here.cpp", "x", s);
    CHECK(!loaded.usable && !loaded.loaded);

    CHECK(hasSourceExtension("a.CPP") && hasSourceExtension("b.hpp") && !hasSourceExtension("c.txt"));
    CHECK(trimPath("  \"C:\\a b\\x.cpp\"  ") == "C:\\a b\\x.cpp");
    CHECK(splitLines("a\r\nb\n\nc").size() == 4);
}

int main() {
    testLexer();
    testNormalizer();
    testHashing();
    testJaccard();
    testSimilarity();
    testEdgeCases();
    std::cout << (g_total - g_fail) << " / " << g_total << " checks passed\n";
    return g_fail == 0 ? 0 : 1;
}
