#pragma once

#include <vector>
#include <string>

namespace cmd {
extern std::vector<std::string> HISTORY;
void history(const std::vector<std::string>& parsed);
void add_to_history(const std::string& command);
void writeFileToHistory(const std::string& filename);
void writeFileFromHistory(const std::string& filename);
void writeFileAppFromHistory(const std::string& filename);
}