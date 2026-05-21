#include "history.hpp"

#include <iostream>
#include <format>


namespace cmd {
std::vector<std::string> HISTORY{};
void history() {
  for (int i = 0; i < HISTORY.size(); i++) {
    std::cout << std::format("    {:<2d} {}", i + 1, HISTORY[i]) << std::endl;
  }
}

void add_to_history(const std::string& command) {
  HISTORY.push_back(command);
}
} // namespace cmd