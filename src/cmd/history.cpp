#include "history.hpp"

#include <iostream>
#include <format>


namespace cmd {
std::vector<std::string> HISTORY{};
void history(const std::vector<std::string>& parsed) {
  // 将 parsed 中的命令解析为整数
  int index = 0;
  if (!parsed.empty()) {
    try {
      index = std::stoi(parsed[parsed.size() - 1]);
    } catch (const std::exception&) {
      std::cerr << "Invalid history index" << std::endl;
      return;
    }
  }

  for (int i = HISTORY.size() - index; i < HISTORY.size(); i++) {
    std::cout << std::format("    {:<2d} {}", i + 1, HISTORY[i]) << std::endl;
  }
}

void add_to_history(const std::string& command) {
  HISTORY.push_back(command);
}
} // namespace cmd