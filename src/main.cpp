#include <cstdlib>
#include <filesystem>
#include <fcntl.h>
#include <iostream>
#include <readline/readline.h>
#include <readline/history.h> // 新增：支持历史记录
#include <sstream>
#include <string>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <unordered_set>
#include <unordered_map>
#include <vector>
#include <algorithm>

#include "cmd/command.hpp"

using namespace std;

int main() {
  string input;
  string pathstring = getenv("PATH");
  cmd::populatePATH(pathstring);
  cmd::cacheAllExecutables();
  
  // Flush after every cout / cerr
  cout << unitbuf;
  cerr << unitbuf;

  // 启用历史记录
  using_history();
  rl_attempted_completion_function = cmd::shellCompletion;
  
  while (true) {
    if (!cmd::readInput(input)) {
      // 用户按下 Ctrl+D 退出
      cout << endl;
      exit(0);
    }
    if (!input.empty()) {
      add_history(input.c_str()); // 添加到历史记录
    }
    cmd::handleInput(input);
  }
}
