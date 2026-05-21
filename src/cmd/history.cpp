#include "history.hpp"

#include <iostream>
#include <format>
#include <fstream>

namespace cmd {
std::vector<std::string> HISTORY{};
void history(const std::vector<std::string>& parsed) {
  // 将 parsed 中的命令解析为整数
  if (parsed.size() == 3) { 
    if (parsed[1] == "-r") {
      // 处理 -r 选项
      writeFileToHistory(parsed[2]);
    } else if (parsed[1] == "-w") {
      // 处理 -w 选项
      writeFileFromHistory(parsed[2]);
    }
  } else if (parsed.size() == 2) {
    int index = 0;
    if (!parsed.empty()) {
      try {
        index = std::stoi(parsed[1]);
      } catch (const std::exception&) {
        std::cerr << "Invalid history index" << std::endl;
        return;
      }
    }

    for (int i = HISTORY.size() - index; i < HISTORY.size(); i++) {
      std::cout << std::format("    {:<2d} {}", i + 1, HISTORY[i]) << std::endl;
    }
  } else {
    for (size_t i = 0; i < HISTORY.size(); i++) {
      std::cout << std::format("    {:<2d} {}", i + 1, HISTORY[i]) << std::endl;
    }
  }
}

void add_to_history(const std::string& command) {
  HISTORY.push_back(command);
}

void writeFileToHistory(const std::string& filename) {
  // 将文件中的历史记录按行写入 HISTORY
  std::ifstream file(filename);
  std::string line;
  if (file.is_open()) {
    while (std::getline(file, line) ) {
      if (!line.empty()) HISTORY.push_back(line);
    }
    file.close();
  }
}
void writeFileFromHistory(const std::string& filename) {
  // 将 HISTORY 中的记录写入文件
  std::ofstream file(filename);
  if (file.is_open()) {
    for (const auto& line : HISTORY) {
      file << line << std::endl;
    }
    file.close();
  }
}
} // namespace cmd