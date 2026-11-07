# CodeLens - C++ source-code similarity detector

Console application (C++17, no external libraries). Reads local C++ files **as text only**
(never compiles or runs them), finds similar code patterns and shows the matching sections
so a teacher can review them. Similarity is a review signal, **not proof of copying**.

## Build and run

    make                 # builds ./codelens          (g++ 8+ / clang 7+ / MSVC 2017+)
    make test            # builds and runs the unit tests
    make demo            # compares the 4 files in sample/

No `make` (e.g. Windows + MinGW)? Compile directly:

    g++ -std=c++17 -O2 src/*.cpp -o codelens          (MinGW older than GCC 9: add -lstdc++fs)

or use CMake: `cmake -S . -B build && cmake --build build`.

    ./codelens                         interactive numbered menu
    ./codelens a.cpp b.cpp             compare two files
    ./codelens --dir submissions/      compare every .cpp/.cc/.c/.h/.hpp in a folder (recursive)
    ./codelens --dir sub/ --report r.txt --min-score 40 --lcs

Options: `-k N`, `-w N`, `--lcs`, `--keep-includes`, `--keep-literals`, `--min-score X`,
`--excerpts N`, `--details N`, `--report FILE`, `--help`.

## Pipeline (matches the project overview)

| Step | File | Technique |
|------|------|-----------|
| 1. Read files | `file_loader.*`, `pipeline.*` | `std::ifstream`, `std::filesystem`; size limit, binary/empty checks |
| 2. Tokenise | `lexer.*` | hand-written state machine: comments, strings, raw strings, char literals, numbers, operators; keeps original line numbers |
| 3. Standardise names | `normalizer.*` | hash map: identifier -> `ID1`, `ID2`...; keywords/operators unchanged; library names (`cout`, `vector`...) unchanged |
| 4. Fingerprints | `fingerprint.*` | k-grams, rolling hash (O(n)), winnowing with a monotonic **deque** |
| 5. Compare | `similarity.*` | hash sets; every shared hash is **verified against the real token groups** |
| 6. Report | `report.*`, `menu.*`, `cli.*` | Jaccard score, `sort` ranking, matching regions with line numbers, text report |

**Similarity** = verified shared distinct fingerprints / all distinct fingerprints x 100 (Jaccard).
**Coverage** (extra) = % of each file's tokens inside matched regions - easier to read than Jaccard
when files are edited copies.
**Guarantee:** any shared run of `k + window - 1` tokens (10 by default) is always detected.
**Optional LCS** (`--lcs` or Settings): token-order score `2*LCS/(nA+nB)`, skipped above 25M table cells.

## Edge cases handled

Empty / comment-only files -> "insufficient code"; short files -> labelled fallback (k reduced, every
k-gram kept); missing/unreadable/binary/oversized files -> clear message; `//` inside strings kept intact;
folder limit (300 files); repeated-pattern cap per hash; blank input cancels any prompt; Ctrl-D/EOF exits cleanly.

## Known limitations (same as the proposal)

No full C++ parsing or macro expansion. Identifier mapping is not scope-aware, so a newly inserted
declaration near the top shifts later labels (ID2 -> ID3 ...) and lowers the score. Different
implementations of the same logic are not detected. Common templates raise similarity.

## Suggested four-member split

1. `file_loader`, `lexer`, `pipeline`  2. `normalizer`, `fingerprint`  3. `similarity`  4. `menu`, `cli`, `report`, `tests/`
