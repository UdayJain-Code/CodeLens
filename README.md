# CodeLens

**A dependency-free C++17 console application that detects source-code similarity by finding and displaying matching code patterns in local C++ files.**

![C++17](https://img.shields.io/badge/C%2B%2B-17-blue)
![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20Linux%20%7C%20macOS-lightgrey)
![Dependencies](https://img.shields.io/badge/dependencies-none-brightgreen)

CodeLens helps a teacher find pairs of programming submissions that deserve a closer look. It ignores comments, spacing and variable names, so lightly disguised copies still show up. Everything runs on one computer: no internet, no accounts, and submitted programs are **read as text and never executed**.

> **Important:** a similarity score is a *review signal*, not proof of copying. Two students can independently write similar standard solutions. Always check the matched code and the assignment context before judging.

---

## Features

- Compare **two files** or a **whole folder** of submissions (subfolders included)
- **Rename-proof:** `int total = a + b;` and `int answer = x + y;` look identical to the tool
- Ignores comments, whitespace and `#include` lines (shared templates)
- **Ranked list** of file pairs, most similar first
- **Matching code excerpts** with original line numbers from both files
- **Saves a full text report**
- Interactive **numbered menu** and a **command-line mode** for scripts
- Safe handling of empty, comment-only, very short, missing, binary and oversized files
- Unit tests (48 checks) included

## Quick start (Windows)

### 1. Install a C++ compiler (one time only)

1. Install **MSYS2** from <https://www.msys2.org>
2. Open the **MSYS2 UCRT64** app and run:
   ```
   pacman -S mingw-w64-ucrt-x86_64-gcc
   ```
3. Add `C:\msys64\ucrt64\bin` to your Windows **PATH**, then restart your terminal.
4. Check it works:
   ```
   g++ --version
   ```

### 2. Download the project

**With Git:**
```
git clone https://github.com/UdayJain-Code/CodeLens.git
```

**Without Git:** on the GitHub page click **Code → Download ZIP**, then right-click the ZIP and choose **Extract All**.

### 3. Go to the folder that contains `src`

The project sits inside a `CodeLens` folder inside the repository folder, so you need to step in twice:

```
cd CodeLens
cd CodeLens
dir
```

`dir` should list `sample`, `src` and `tests`. If it does not, you are one level too high or too low.

### 4. Compile

In PowerShell:
```
g++ -std=c++17 -O2 (Get-ChildItem src\*.cpp).FullName -o codelens.exe
```
No message means success, and `codelens.exe` appears in the folder.

### 5. Run

```
.\codelens.exe
```

Try it with the included sample files:

| Type | What happens |
|------|--------------|
| `2` then `sample` | loads the 4 sample programs |
| `4` | runs the comparison and shows the ranked table |
| `6` then `1` | shows the matching code of the top pair |
| `8` | saves a text report |
| `0` | exits |

## Quick start (macOS / Linux)

```
git clone https://github.com/UdayJain-Code/CodeLens.git
cd CodeLens/CodeLens
make
./codelens
```

On macOS run `xcode-select --install` first if you have no compiler.

## Usage

### Interactive menu

```
=============== CodeLens ===============
 1) Select two C++ files
 2) Select a folder of submissions
 3) Show loaded files
 4) Run comparison
 5) View ranked results
 6) View matching code of a pair
 7) Settings
 8) Save text report
 9) Help
 0) Exit
```

Paths can be full paths (`C:\Users\you\Desktop\a.cpp`) or relative to the current folder. A blank answer cancels any prompt.

### Command line

```
codelens FILE_A FILE_B [options]       compare two files
codelens --dir FOLDER [options]        compare every C/C++ file in a folder
```

| Option | Meaning |
|--------|---------|
| `-k N` | k-gram size in tokens (2-30, default 7) |
| `-w N` | winnowing window (1-30, default 4) |
| `--lcs` | also compute the token-order (LCS) score |
| `--keep-includes` | do not ignore `#include` lines |
| `--keep-literals` | do not normalise numbers, strings and chars |
| `--min-score X` | hide pairs below X percent |
| `--excerpts N` | matching sections shown per pair (default 5) |
| `--details N` | number of top pairs shown in detail |
| `--report FILE` | also save a full text report |
| `-h`, `--help` | show help |

Examples:
```
.\codelens.exe sample\student_a.cpp sample\student_b.cpp
.\codelens.exe --dir submissions --min-score 40 --report report.txt
```

### Sample output

```
RANKED FILE PAIRS (most similar first)
------------------------------------------------------------------------------
 Rank  Similarity  Overlap     Coverage A/B   Pair
------------------------------------------------------------------------------
    1       51.7%  high          84% /85%     student_a.cpp <-> student_b.cpp
    2        4.0%  low           27% /23%     student_b.cpp <-> student_c.cpp
    3        3.2%  low           24% /20%     student_a.cpp <-> student_c.cpp
    4        0.0%  low            0% /0%      student_a.cpp <-> student_d.cpp
```

`student_b.cpp` is `student_a.cpp` with renamed variables, new comments and a different brace style, so it ranks first. The other files solve different problems.

## How it works

```
 C++ files ─► Tokens ─► Normalised ─► k-gram ─► Winnowed ─► Verified ─► Score, ranking
 (read only)  (lexer)   (ID1, ID2…)   hashes    fingerprints  matches    and excerpts
```

1. **Read files** as text, keeping original line numbers.
2. **Tokenise** with a small state machine; comments and spacing are skipped, strings stay intact.
3. **Standardise names:** user-defined identifiers become `ID1`, `ID2`, … consistently within each file. Keywords, operators and common library names (`cout`, `vector`) are kept.
4. **Fingerprint:** a rolling hash covers every group of `k` tokens, and winnowing keeps one representative hash per window.
5. **Compare:** shared fingerprints are checked against the real tokens, so a hash collision can never count as a match.
6. **Report:** Jaccard similarity, ranked pairs, and matching sections with line numbers.

### Reading the numbers

- **Similarity** = shared distinct fingerprints ÷ all distinct fingerprints × 100 (Jaccard). It is *not* a probability of copying.
- **Coverage** = % of each file's tokens that lie inside matched regions. It is easier to read than Jaccard when a file is an edited copy.
- **Detection guarantee:** any shared run of `k + window − 1` tokens (10 by default) is always detected.
- **Optional LCS score** (`--lcs`): token-order similarity, `2·LCS / (tokens A + tokens B)`, skipped for very large files.

## Data structures and algorithms

| Component | Technique | Purpose |
|-----------|-----------|---------|
| Tokenisation | Finite state machine | Recognise tokens; handle comments and strings, O(n) |
| Name normalisation | Hash map | Remember the label given to each identifier |
| Token hashing | FNV-1a | Turn each token into a 64-bit number |
| Fingerprint creation | Rolling hash (Rabin–Karp) | Hash every k-token group in O(n) total |
| Fingerprint selection | Winnowing with a monotonic **deque** | Sliding-window minimum in O(n) |
| Similarity | Hash sets, Jaccard index | Count shared and total distinct fingerprints |
| Matching regions | Sorting and diagonal merging | Turn matches into line ranges |
| Ranking | `std::stable_sort` | Order file pairs by similarity |
| Token-order score (optional) | Dynamic programming (LCS) | Two-row table, O(n·m) time, O(m) memory |

## Project structure

```
CodeLens/                      repository
└── CodeLens/                  project folder
    ├── src/
    │   ├── main.cpp           entry point
    │   ├── menu.*             interactive menu
    │   ├── cli.*              command-line mode
    │   ├── file_loader.*      safe file and folder reading
    │   ├── lexer.*            tokeniser (state machine)
    │   ├── normalizer.*       identifier labelling and token hashing
    │   ├── fingerprint.*      rolling hash and winnowing
    │   ├── similarity.*       verification, Jaccard, regions, ranking, LCS
    │   ├── pipeline.*         load, tokenise and normalise files
    │   ├── report.*           console and text-report formatting
    │   └── types.h            shared data types and settings
    ├── tests/test_main.cpp    unit tests
    ├── sample/                four example submissions
    ├── Makefile
    └── CMakeLists.txt
```

## Running the tests

```
g++ -std=c++17 -O2 -Isrc (Get-ChildItem src\*.cpp -Exclude menu.cpp,cli.cpp,main.cpp).FullName tests\test_main.cpp -o codelens_tests.exe
.\codelens_tests.exe
```

Expected: `48 / 48 checks passed`. On macOS and Linux use `make test`.

## Limitations

- No full C++ parsing or macro expansion.
- Identifier mapping is not scope-aware, so a newly inserted declaration near the top of a file shifts later labels and lowers the score.
- A different implementation of the same logic is not detected.
- Common templates and textbook solutions raise similarity between unrelated students.
- Large files and very large comparisons are limited (1 MB per file, 300 files per folder).

## Troubleshooting

| Problem | Fix |
|---------|-----|
| `g++ is not recognized` | The compiler is not installed or not on PATH (see step 1) |
| `No such file or directory` for `src/...` | You are in the wrong folder; `cd` into the one that contains `src` |
| `filesystem` or `-lstdc++fs` errors | Your GCC is older than 8; update it or add `-lstdc++fs` at the end of the compile command |
| Strange characters in the output | Run `chcp 65001` once in that terminal |

## Author

Built by **[UdayJain-Code](https://github.com/UdayJain-Code)** as a college project.
