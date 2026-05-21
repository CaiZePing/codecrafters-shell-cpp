#include "complete.hpp"

#include <vector>
#include <string>
#include <iostream>
#include <sstream>
#include <filesystem>
#include <readline/readline.h>
#include <readline/history.h>
#include <unordered_set>
#include <unordered_map>

namespace cmd {
extern std::vector<std::string> executables;              // 存储所有可执行文件
extern const std::unordered_set<std::string> BUILTIN_COMMANDS;
extern std::unordered_map<std::string, std::string> completes;

extern std::vector<std::string> parseInput(const std::string& command);
extern std::string stdoutexecv(const std::vector<std::string>& parsed, std::string command);

void complete(const std::vector<std::string>& parsed) {
  if (parsed.size() < 3) {
    return;
  }
  
  if (parsed[1] == "-p") {
    auto it = completes.find(parsed[2]);
    if (it == completes.end()) {
      std::cout << "complete: " << parsed[2] << ": no completion specification" << std::endl;
    } else {
      std::cout << "complete -C '" << it->second << "' " << it->first << std::endl;
    }
  } else if (parsed[1] == "-C") {
    if (parsed.size() != 4) return;
    completes[parsed[3]] = parsed[2];
  } else if (parsed[1] == "-r") {
    completes.erase(parsed[2]);
  }
}

// 内部指令补全生成器
char* builtinCompletionGenerator(const char *text, int state) {
  static std::vector<std::string> candidates;
  static size_t index = 0;
  static int len;

  if (state == 0) {
    index = 0;
    len = strlen(text);

    // 先把内置命令加进去
    // assign 函数会重置 candidates
    candidates.assign(BUILTIN_COMMANDS.begin(), BUILTIN_COMMANDS.end());

    // 再把 PATH 里所有可执行文件加进去
    // 实际上可以不使用， 因为在 main 函数中 已经初始化过，这个函数 全局只需要使用一次即可，再次执行会直接返回
    // cacheAllExecutables();
    candidates.insert(candidates.end(), executables.begin(), executables.end());
  }

  while (index < candidates.size()) {
    const std::string& cmd = candidates[index++];
    if (strncmp(cmd.c_str(), text, len) == 0) {
        return strdup(cmd.c_str());
    }
  }
  return nullptr;
}

// 外部命令补全生成器
char* externalCompletionGenerator(const char *text, int state) {
  static std::vector<std::string> candidates;
  static size_t index = 0;
  static int len;
  // 这个里面的 state 在每次重新按 <tab> 键的时候都会重新变为 0
  // 只是在一次 <tab> 中，每次查找下一个的时候，这个 state 才会增加
  // 也就是说，在一次 补全 的情况下 candidates 里面的值应该是固定的
  if (state == 0) {
    index = 0;
    candidates.clear();
    len = strlen(text);
    // 获取当前的输入行的缓存
    std::string cmd_line(rl_line_buffer);
    std::vector<std::string> parsed = parseInput(cmd_line);
    if (parsed.empty()) {
      return nullptr;
    }
    // 查看是不是 使用 complete 注册过的
    auto it = completes.find(parsed[0]);
    if (it == completes.end()) {
      return nullptr;
    }
    // 获取光标前的输入
    std::string before_cursor = cmd_line.substr(0, rl_point);
    std::vector<std::string> parsed_before_cursor = parseInput(before_cursor);
    
    std::string cmd_name = parsed[0];
    std::string current_word = text;
    std::string previous_word = "";
    
    int word_index = parsed_before_cursor.size() - 1;
    if (word_index > 0) {
      previous_word = parsed_before_cursor[word_index - 1];
    }
    
    std::vector<std::string> cmd_args = parseInput(it->second);
    cmd_args.push_back(cmd_name);
    cmd_args.push_back(current_word);
    cmd_args.push_back(previous_word);
    std::string output = stdoutexecv(cmd_args, it->second);
    
    std::stringstream ss(output);
    std::string line;
    while (getline(ss, line)) {
      if (!line.empty()) {
        candidates.push_back(line);
      }
    }
  }

  while (index < candidates.size()) {
    const std::string& candidate = candidates[index++];
    if (strncmp(candidate.c_str(), text, len) == 0) {
      return strdup(candidate.c_str());
    }
  }
  
  return nullptr;
}

// 文件补全生成器
char* fileCompletionGenerator(const char *text, int state) {
  static std::vector<std::string> matches;
  static size_t index = 0;
  
  if (state == 0) {
    matches.clear();
    index = 0;
    
    std::string full_path(text);
    size_t lastSlash = full_path.find_last_of("/\\");
    std::string path;
    std::string prefix;
    
    if (lastSlash != std::string::npos) {
      path = full_path.substr(0, lastSlash + 1);
      prefix = full_path.substr(lastSlash + 1);
    } else {
      path = "";
      prefix = full_path;
    }
    
    std::string search_path = path.empty() ? "./" : path;
    try {
      for (const auto& entry : std::filesystem::directory_iterator(search_path)) {
        std::string name = entry.path().filename().string();
        if (name.compare(0, prefix.size(), prefix) == 0) {
          if (entry.is_directory()) {
            matches.push_back(path + name + "/");
          } else {
            matches.push_back(path + name + " ");
          }
        }
      }
    } catch (const std::filesystem::filesystem_error& ex) {
      // 静默忽略无法访问的目录
    }
  }
  
  if (index < matches.size()) {
    return strdup(matches[index++].c_str());
  }
  return nullptr;
}

char** shellCompletion(const char *text, int start, int end) {
  (void)end;
  rl_attempted_completion_over = 1;

  if (start == 0) {
    setenv("COMP_LINE", rl_line_buffer, 1);
    setenv("COMP_POINT", std::to_string(rl_point).c_str(), 1);
    // 第一个单词：补全内置命令和可执行文件
    rl_completion_append_character = ' ';
    return rl_completion_matches(text, builtinCompletionGenerator);
  } else {
    setenv("COMP_LINE", rl_line_buffer, 1);
    setenv("COMP_POINT", std::to_string(rl_point).c_str(), 1);
    // 检查是否有注册的外部补全命令
    std::string cmd_line(rl_line_buffer);
    std::vector<std::string> parsed = parseInput(cmd_line);
    if (!parsed.empty() && completes.count(parsed[0])) {
      rl_completion_append_character = ' ';
      return rl_completion_matches(text, externalCompletionGenerator);
    }
    
    // 默认：文件补全
    rl_completion_append_character = '\0';
    return rl_completion_matches(text, fileCompletionGenerator);
  }
}
}