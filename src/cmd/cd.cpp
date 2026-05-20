#include "cd.hpp"

#include <vector>
#include <string>
#include <filesystem>
#include <iostream>

namespace cmd {
// 处理cd命令，切换当前路径
void cd(const std::vector<std::string>& parsed) {
  std::string newPath;
  
  if (parsed.size() < 2 || parsed[1] == "~") {
    // 没有参数或 ~ 直接切换到用户目录
    const char* home = getenv("HOME");
    if (home == nullptr) {
      // std::cout << "cd: HOME not set" << std::endl;
      return;
    }
    newPath = home;
  } else {
    newPath = parsed[1];
    if (newPath[0] == '~') {
      // 处理 ~/path 形式
      const char* home = getenv("HOME");
      if (home == nullptr) {
        // std::cout << "cd: HOME not set" << std::endl;
        return;
      }
      newPath.replace(0, 1, home);
    }
  }
  
  try {
    std::filesystem::current_path(newPath);
  } catch (const std::filesystem::filesystem_error& ex) {
    // std::cout << "cd: " << newPath << ": " << ex.what() << std::endl;
    std::cout << "cd: " << newPath << ": No such file or directory" << std::endl;
  }
}
}