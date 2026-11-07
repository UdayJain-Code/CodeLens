// cli.h - non-interactive command-line mode (handy for scripts and tests)
#pragma once
#include <string>
#include <vector>

namespace codelens {
int runCli(const std::vector<std::string>& args);
}
