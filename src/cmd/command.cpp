#include "command.hpp"

namespace cmd {
const std::unordered_set<std::string> BUILTIN_COMMANDS{"echo", "type", "exit", "pwd", "cd", "complete", "jobs" , "history", "declare"};

std::vector<std::string> executables{};              // 存储所有可执行文件
std::unordered_map<std::string, std::string> completes{}; // 存储注册命令

std::vector<std::string> PATH{};              // 存储PATH环境变量中的所有路径
std::string HISTORYPATH{};

// 自定义的两个环境变量
std::string COMP_LINE{};
std::string COMP_POINT{};

// 将PATH环境变量分解成一个个路径，存储在全局变量PATH中
void populatePATH(const std::string& pathstring) {
  std::stringstream ss(pathstring);
  std::string directory;
  while (std::getline(ss, directory, PATH_LIST_SEPARATOR)) {
    PATH.push_back(directory);
  }
}

// 查找是不是可执行文件，返回绝对路径，否则返回空字符串
std::string findCmdInPath(const std::string& cmd) {
  std::filesystem::path cwd = std::filesystem::current_path();
  std::filesystem::path cwd_fullPath = cwd / cmd;
  
  if (std::filesystem::exists(cwd_fullPath) && std::filesystem::is_regular_file(cwd_fullPath)) {
    auto perms = std::filesystem::status(cwd_fullPath).permissions();
    if ((perms & (std::filesystem::perms::group_exec | std::filesystem::perms::owner_exec | std::filesystem::perms::others_exec)) != std::filesystem::perms::none) {
      return cwd_fullPath.string();
    }
  }

  for (const std::string& directory : PATH) {
    std::filesystem::path fullPath = std::filesystem::path(directory) / cmd;

    if (std::filesystem::exists(fullPath) && std::filesystem::is_regular_file(fullPath)) {
      auto perms = std::filesystem::status(fullPath).permissions();
      if ((perms & (std::filesystem::perms::group_exec | std::filesystem::perms::owner_exec | std::filesystem::perms::others_exec)) != std::filesystem::perms::none) {
        return fullPath.string();
      }
    }
  }

  return "";
}

// 缓存当前 PATH 中的可执行文件
void cacheAllExecutables() {
  if (!executables.empty()) return;

  std::unordered_set<std::string> seen;

  for (const auto& dir : PATH) {
    try {
      // 是否存在 或 是不是文件夹
      if (!std::filesystem::exists(dir) || !std::filesystem::is_directory(dir))
        continue;
      // 遍历文件夹中的所有文件
      for (const auto& entry : std::filesystem::directory_iterator(dir)) {
        if (!entry.is_regular_file())
          continue;
        // 查看文件的 权限
        auto perms = entry.status().permissions();
        if ((perms & (std::filesystem::perms::owner_exec | std::filesystem::perms::group_exec | std::filesystem::perms::others_exec)) == std::filesystem::perms::none)
          continue;
  
        std::string name = entry.path().filename().string();
        if (seen.count(name)) continue;
  
        seen.insert(name);
        executables.push_back(name);
      }
    } catch (const std::filesystem::filesystem_error& ex) {
      // 静默忽略无法访问的目录
      continue;
    }
  }
}

// 是内部命令
bool isBuiltinCommand(const std::string& command) {
  std::string cmd{};
  int c;
  for (int i = 0; i < command.length(); i++) {
    c = command[i];
    cmd += c;
  }
  return BUILTIN_COMMANDS.find(cmd) != BUILTIN_COMMANDS.end();
}

bool readInput(std::string &input) {
  Jobs::instance().checkAndRemoveJob();
  rl_already_prompted = 0;
  char *line = readline("$ ");

  if (line == nullptr)
    return false;

  input = line;
  free(line);
  return true;
}

// 分解输入命令
std::vector<std::string> parseInput(const std::string& command) {
  std::vector<std::string> parsed;
  std::string current;
  bool isSingleQuote = false;
  bool isDoubleQuote = false;
  bool isSpace = false;
  size_t i = 0;

  while (i < command.size()) {
    if (command[i] == '\'' && !isDoubleQuote) {
      isSingleQuote ^= 1;
      isSpace = false;
    } else if (command[i] == '\"' && !isSingleQuote) {
      isDoubleQuote ^= 1;
      isSpace = false;
    } else if (command[i] == '\\' && !isSingleQuote && i + 1 < command.size()) {
      current += command[++i];
    } else if (command[i] == ' ' && !isSingleQuote && !isDoubleQuote) {
      if (isSpace) {
        i++;
        continue;
      }
      parsed.push_back(current);
      current.clear();
      isSpace = true;
    } else {
      current += command[i];
      isSpace = false;
    }
    i++;
  }
  if (!current.empty())
    parsed.push_back(current);
  return parsed;
}

std::vector<std::string> splitByPipe(const std::vector<std::string>& parsed) {
  std::vector<std::string> commands;
  std::string current_command;
  for (const auto& arg : parsed) {
    if (arg == "|") {
      commands.push_back(current_command);
      current_command.clear();
    } else {
      if (current_command.empty()) {
        current_command = arg;
      } else {
        current_command = current_command + " " + arg;
      }
    }
  }
  if (!current_command.empty()) {
    commands.push_back(current_command);
  }
  return commands;
}

// 处理输入
void handleInput(const std::string& input) {
  auto parsed = parseInput(input);
  if (parsed.empty()) {
    return;
  }
  add_to_history(input);
  auto command = parsed[0];
  int backup_fd = -1;
  int target_fd = -1;
  bool write_into_file = false;
  bool isBack = false;
  std::string file;

  // 将所有前面是 $ 的如果是内部变量，都替换 value
  handleVariable(parsed);

  if (parsed.size() > 1) {
    std::string last_arg = parsed[parsed.size() - 1];
    if (last_arg == "&") {
      parsed.pop_back();
      isBack = true;
    }
  }

  if (parsed.size() > 2) {
    std::string redirection = parsed[parsed.size() - 2];
    if (redirection.back() == '>') {
      write_into_file = true;
      file = parsed[parsed.size() - 1];
      parsed.pop_back();
      parsed.pop_back();
      
      int flags = O_WRONLY | O_CREAT;
      target_fd = STDOUT_FILENO;
      
      if (redirection == ">>" || redirection == "1>>") {
        flags |= O_APPEND;
      } else if (redirection == "2>" || redirection == "2>>") {
        target_fd = STDERR_FILENO;
        if (redirection == "2>>") flags |= O_APPEND;
        else flags |= O_TRUNC;
      } else {
        flags |= O_TRUNC; // 默认 > 覆盖
      }
      
      int fd = open(file.c_str(), flags, 0644);
      if (fd == -1) {
        std::cerr << "cannot open " << file << ": Permission denied" << std::endl;
        return;
      }
      
      backup_fd = dup(target_fd);
      dup2(fd, target_fd);
      close(fd);
    }
  }

  for (int i = 0; i < parsed.size() ; i++) {
    if (parsed[i] == "|") {
      parsed = splitByPipe(parseInput(input));
      if (isBack) {
        cmd::pipebgexecv(parsed, input.c_str());
      } else {
        cmd::pipeexecv(parsed, input.c_str());
      }
      return;
    }
  }

  if (command == "exit") {
    writeFileFromHistory(HISTORYPATH);
    exit(0);
  } else if (command == "echo") {
    cmd::echo(parsed);
  } else if (command == "type") {
    cmd::type(parsed);
  } else if (command == "pwd") {
    cmd::pwd();
  } else if (command == "cd") {
    cmd::cd(parsed);
  } else if (command == "clear") {
    cmd::clear();
  } else if (command == "complete") {
    cmd::complete(parsed);
  } else if (command == "jobs") {
    cmd::jobs(parsed);
  } else if (command == "history") {
    cmd::history(parsed);
  } else if (command == "declare") {
    cmd::declare(parsed);
  } else {
    std::string cmd_path = cmd::findCmdInPath(command);
    if (!cmd_path.empty()) {
      if (isBack) {
        cmd::bgexecv(parsed, input.c_str());
      } else {
        cmd::myexecv(parsed, input.c_str());
      }
    } else {
      std::cout << command << ": command not found" << std::endl;
    }
  }

  // 恢复文件描述符
  if (write_into_file && backup_fd != -1) {
    // 先刷新缓冲区，确保所有数据都被写入
    fflush(stdout);
    fflush(stderr);
    // 恢复文件描述符
    int result = dup2(backup_fd, target_fd);
    if (result == -1) {
      // dup2 失败，打印错误（此时文件描述符已经是原始的了吗？）
      // 不过我们继续执行
    }
    close(backup_fd);
    // 再次刷新，确保恢复后的状态
    fflush(stdout);
    fflush(stderr);
  }
}

}