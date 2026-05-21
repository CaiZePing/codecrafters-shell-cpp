#pragma once

#include <vector>
#include <string>

namespace cmd {
extern std::vector<std::string> HISTORY;
void history();
void add_to_history(const std::string& command);
}