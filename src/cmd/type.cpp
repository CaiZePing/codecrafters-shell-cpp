#include "command.hpp"
#include "type.hpp"

#include <iostream>
#include <unordered_set>

extern const std::unordered_set<std::string> BUILTIN_COMMANDS;

// extern std::string findCmdInPath(const std::string& cmd);

namespace cmd {
void type(const std::vector<std::string>& parsed) {
  if (parsed.size() < 2) {
    return;
  }
  
  std::string arg1 = parsed[1];
  if (BUILTIN_COMMANDS.count(arg1) > 0) {
    std::cout << arg1 << " is a shell builtin" << std::endl;
  } else {
    std::string path = findCmdInPath(arg1);
    if (!path.empty()) {
      std::cout << arg1 << " is " << path << std::endl;
    } else {
      std::cout << arg1 << ": not found" << std::endl;
    }
  }
}
}