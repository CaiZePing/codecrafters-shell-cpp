#pragma once

#include <vector>
#include <string>
#include <unordered_map>

namespace cmd {
extern std::unordered_map<std::string, std::string> variables;
void declare(const std::vector<std::string>& parsed);
}