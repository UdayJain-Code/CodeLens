// CodeLens - console-based C++17 source-code similarity detector
#include <string>
#include <vector>

#include "cli.h"
#include "menu.h"

int main(int argc, char** argv) {
    std::vector<std::string> args(argv + 1, argv + argc);
    if (args.empty()) return codelens::runMenu();
    return codelens::runCli(args);
}
