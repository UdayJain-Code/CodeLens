// report.h - formatting of results for the console and the saved text report
#pragma once
#include <ostream>
#include <string>
#include <vector>

#include "similarity.h"
#include "types.h"

namespace codelens {

std::string percent(double v, int decimals = 1);
std::string overlapLabel(double score);

void printSettings(std::ostream& os, const Settings& s);
void printFiles(std::ostream& os, const std::vector<Submission>& subs);
void printSummary(std::ostream& os, const std::vector<Submission>& subs, const std::vector<PairResult>& pairs,
                  const Settings& s);
void printPairDetail(std::ostream& os, const std::vector<Submission>& subs, const PairResult& p,
                     const Settings& s, std::size_t rank);
void writeFullReport(std::ostream& os, const std::vector<Submission>& subs, const std::vector<PairResult>& pairs,
                     const Settings& s, const std::vector<std::string>& warnings);

}  // namespace codelens
