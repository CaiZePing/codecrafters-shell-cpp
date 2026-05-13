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

#ifdef _WIN32
constexpr char PATH_LIST_SEPARATOR = ';';
#else
constexpr char PATH_LIST_SEPARATOR = ':';
#endif

using namespace std;
namespace fs = filesystem;

/**
 * 全局变量
 */

const unordered_set<string> BUILTIN_COMMANDS{"echo", "type", "exit", "pwd", "cd", "complete", "jobs"};
vector<string> PATH{};              // 存储PATH环境变量中的所有路径

// 自定义的两个环境变量
string COMP_LINE{};
string COMP_POINT{};

vector<string> executables{};       // 存储所有可执行文件
unordered_map<string, string> completes{}; // 存储注册命令

/**
 * function declaration
 */

void clear();                                 // 清屏
void populatePATH(const string& pathstring);  // 将PATH环境变量分解成一个个路径
string findCmdInPath(const string& cmd);      // 在PATH中查找命令
void cacheAllExecutables();                   // 缓存所有可执行文件

char* builtinCompletionGenerator(const char *text, int state);  // 命令补全生成器
char* externalCompletionGenerator(const char *text, int state); // 外部命令补全生成器
char* fileCompletionGenerator(const char *text, int state);     // 文件补全生成器
char** shellCompletion(const char *text, int start, int end);   // shell补全函数

bool readInput(string &input);                                  // 读取用户输入
vector<string> parseInput(const string& command);               // 解析输入命令
void handleInput(const string& parsed);                         // 处理输入
void handleEcho(const vector<string>& parsed);                  // 处理 echo
void handleType(const vector<string>& parsed);                  // 处理 type
void handlepwd(const vector<string>& parsed);                   // 处理 pwd
void handlecd(const vector<string>& parsed);                    // 处理 cd
void handlecomplete(const vector<string> parsed);               // 处理 complete
void handlejobs(const vector<string> parsed);                   // 处理 jobs

void myexecv(const vector<string>& parsed);                     // 执行命令
string pipeexecv(const vector<string>& parsed);                 // 执行管道命令并捕获输出

// function declaration end

int main() {
  string input;
  string pathstring = getenv("PATH");
  populatePATH(pathstring);
  cacheAllExecutables();
  
  // Flush after every cout / cerr
  clear();

  // 启用历史记录
  using_history();
  rl_attempted_completion_function = shellCompletion;
  
  while (true) {
    if (!readInput(input)) {
      // 用户按下 Ctrl+D 退出
      cout << endl;
      exit(0);
    }
    if (!input.empty()) {
      add_history(input.c_str()); // 添加到历史记录
    }
    handleInput(input);
  }
}

/**
 * function definition
 */

void clear() {
  cout << unitbuf;
  cerr << unitbuf;
}

// 将PATH环境变量分解成一个个路径，存储在全局变量PATH中
void populatePATH(const string& pathstring) {
  stringstream ss(pathstring);
  string directory;
  while (getline(ss, directory, PATH_LIST_SEPARATOR)) {
    PATH.push_back(directory);
  }
}

// 查找是不是可执行文件，返回绝对路径，否则返回空字符串
string findCmdInPath(const string& cmd) {
  fs::path cwd = fs::current_path();
  fs::path cwd_fullPath = cwd / cmd;
  
  if (fs::exists(cwd_fullPath) && fs::is_regular_file(cwd_fullPath)) {
    auto perms = fs::status(cwd_fullPath).permissions();
    if ((perms & (fs::perms::group_exec | fs::perms::owner_exec | fs::perms::others_exec)) != fs::perms::none) {
      return cwd_fullPath.string();
    }
  }

  for (const string& directory : PATH) {
    fs::path fullPath = fs::path(directory) / cmd;

    if (fs::exists(fullPath) && fs::is_regular_file(fullPath)) {
      auto perms = fs::status(fullPath).permissions();
      if ((perms & (fs::perms::group_exec | fs::perms::owner_exec | fs::perms::others_exec)) != fs::perms::none) {
        return fullPath.string();
      }
    }
  }

  return "";
}

// 缓存当前 PATH 中的可执行文件
void cacheAllExecutables() {
  if (!executables.empty()) return;

  unordered_set<string> seen;

  for (const auto& dir : PATH) {
    try {
      // 是否存在 或 是不是文件夹
      if (!fs::exists(dir) || !fs::is_directory(dir))
        continue;
      // 遍历文件夹中的所有文件
      for (const auto& entry : fs::directory_iterator(dir)) {
        if (!entry.is_regular_file())
          continue;
        // 查看文件的 权限
        auto perms = entry.status().permissions();
        if ((perms & (fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec)) == fs::perms::none)
          continue;
  
        string name = entry.path().filename().string();
        if (seen.count(name)) continue;
  
        seen.insert(name);
        executables.push_back(name);
      }
    } catch (const fs::filesystem_error& ex) {
      // 静默忽略无法访问的目录
      continue;
    }
  }
}

// 内部指令补全生成器
char* builtinCompletionGenerator(const char *text, int state) {
  static vector<string> candidates;
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
    const string& cmd = candidates[index++];
    if (strncmp(cmd.c_str(), text, len) == 0) {
        return strdup(cmd.c_str());
    }
  }
  return nullptr;
}

// 外部命令补全生成器
char* externalCompletionGenerator(const char *text, int state) {
  static vector<string> candidates;
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
    string cmd_line(rl_line_buffer);
    vector<string> parsed = parseInput(cmd_line);
    if (parsed.empty()) {
      return nullptr;
    }
    // 查看是不是 使用 complete 注册过的
    auto it = completes.find(parsed[0]);
    if (it == completes.end()) {
      return nullptr;
    }
    // 获取光标前的输入
    string before_cursor = cmd_line.substr(0, rl_point);
    vector<string> parsed_before_cursor = parseInput(before_cursor);
    
    string cmd_name = parsed[0];
    string current_word = text;
    string previous_word = "";
    
    int word_index = parsed_before_cursor.size() - 1;
    if (word_index > 0) {
      previous_word = parsed_before_cursor[word_index - 1];
    }
    
    vector<string> cmd_args = parseInput(it->second);
    cmd_args.push_back(cmd_name);
    cmd_args.push_back(current_word);
    cmd_args.push_back(previous_word);
    string output = pipeexecv(cmd_args);
    
    stringstream ss(output);
    string line;
    while (getline(ss, line)) {
      if (!line.empty()) {
        candidates.push_back(line);
      }
    }
  }

  while (index < candidates.size()) {
    const string& candidate = candidates[index++];
    if (strncmp(candidate.c_str(), text, len) == 0) {
      return strdup(candidate.c_str());
    }
  }
  
  return nullptr;
}

// 文件补全生成器
char* fileCompletionGenerator(const char *text, int state) {
  static vector<string> matches;
  static size_t index = 0;
  
  if (state == 0) {
    matches.clear();
    index = 0;
    
    string full_path(text);
    size_t lastSlash = full_path.find_last_of("/\\");
    string path;
    string prefix;
    
    if (lastSlash != string::npos) {
      path = full_path.substr(0, lastSlash + 1);
      prefix = full_path.substr(lastSlash + 1);
    } else {
      path = "";
      prefix = full_path;
    }
    
    string search_path = path.empty() ? "./" : path;
    try {
      for (const auto& entry : fs::directory_iterator(search_path)) {
        string name = entry.path().filename().string();
        if (name.compare(0, prefix.size(), prefix) == 0) {
          if (entry.is_directory()) {
            matches.push_back(path + name + "/");
          } else {
            matches.push_back(path + name + " ");
          }
        }
      }
    } catch (const fs::filesystem_error& ex) {
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
    setenv("COMP_POINT", to_string(rl_point).c_str(), 1);
    // 第一个单词：补全内置命令和可执行文件
    rl_completion_append_character = ' ';
    return rl_completion_matches(text, builtinCompletionGenerator);
  } else {
    setenv("COMP_LINE", rl_line_buffer, 1);
    setenv("COMP_POINT", to_string(rl_point).c_str(), 1);
    // 检查是否有注册的外部补全命令
    string cmd_line(rl_line_buffer);
    vector<string> parsed = parseInput(cmd_line);
    if (!parsed.empty() && completes.count(parsed[0])) {
      rl_completion_append_character = ' ';
      return rl_completion_matches(text, externalCompletionGenerator);
    }
    
    // 默认：文件补全
    rl_completion_append_character = '\0';
    return rl_completion_matches(text, fileCompletionGenerator);
  }
}

bool readInput(string &input) {
  char *line = readline("$ ");

  if (line == nullptr)
    return false;

  input = line;
  free(line);
  return true;
}

// 分解输入命令
vector<string> parseInput(const string& command) {
  vector<string> parsed;
  string current;
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

// 处理输入
void handleInput(const string& input) {
  auto parsed = parseInput(input);
  if (parsed.empty()) {
    return;
  }

  auto command = parsed[0];
  int backup_fd = -1;
  int target_fd = -1;
  bool write_into_file = false;
  string file;

  if (parsed.size() > 2) {
    string redirection = parsed[parsed.size() - 2];
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
        cerr << "cannot open " << file << ": Permission denied" << endl;
        return;
      }
      
      backup_fd = dup(target_fd);
      dup2(fd, target_fd);
      close(fd);
    }
  }
  
  if (command == "exit") {
    exit(0);
  } else if (command == "echo") {
    handleEcho(parsed);
  } else if (command == "type") {
    handleType(parsed);
  } else if (command == "pwd") {
    handlepwd(parsed);
  } else if (command == "cd") {
    handlecd(parsed);
  } else if (command == "clear") {
    // 真正的清屏命令
    cout << "\033[H\033[2J\033[3J" << flush;
  } else if (command == "complete") {
    handlecomplete(parsed);
  } else if (command == "jobs") {
    handlejobs(parsed);
  } else {
    string cmd_path = findCmdInPath(command);
    if (!cmd_path.empty()) {
      myexecv(parsed);
    } else {
      cout << command << ": command not found" << endl;
    }
  }

  // 恢复文件描述符
  if (write_into_file && backup_fd != -1) {
    dup2(backup_fd, target_fd);
    close(backup_fd);
  }
}

// 处理echo命令，输出参数
void handleEcho(const vector<string>& parsed) {
  for (size_t i = 1; i < parsed.size(); i++) {
    if (i > 1) cout << " ";
    cout << parsed[i];
  }
  cout << endl;
}

// 处理type命令，判断参数是内置命令还是可执行文件
void handleType(const vector<string>& parsed) {
  if (parsed.size() < 2) {
    return;
  }
  
  string arg1 = parsed[1];
  if (BUILTIN_COMMANDS.count(arg1) > 0) {
    cout << arg1 << " is a shell builtin" << endl;
  } else {
    string path = findCmdInPath(arg1);
    if (!path.empty()) {
      cout << arg1 << " is " << path << endl;
    } else {
      cout << arg1 << ": not found" << endl;
    }
  }
}

// 处理pwd命令，输出当前路径
void handlepwd(const vector<string>& parsed) {
  cout << fs::current_path().string() << endl;
}

// 处理cd命令，切换当前路径
void handlecd(const vector<string>& parsed) {
  string newPath;
  
  if (parsed.size() < 2 || parsed[1] == "~") {
    // 没有参数或 ~ 直接切换到用户目录
    const char* home = getenv("HOME");
    if (home == nullptr) {
      // cout << "cd: HOME not set" << endl;
      return;
    }
    newPath = home;
  } else {
    newPath = parsed[1];
    if (newPath[0] == '~') {
      // 处理 ~/path 形式
      const char* home = getenv("HOME");
      if (home == nullptr) {
        // cout << "cd: HOME not set" << endl;
        return;
      }
      newPath.replace(0, 1, home);
    }
  }
  
  try {
    fs::current_path(newPath);
  } catch (const fs::filesystem_error& ex) {
    // cout << "cd: " << newPath << ": " << ex.what() << endl;
    cout << "cd: " << newPath << ": No such file or directory" << endl;
  }
}

// 处理complete命令
void handlecomplete(const vector<string> parsed) {
  if (parsed.size() < 3) {
    return;
  }
  
  if (parsed[1] == "-p") {
    auto it = completes.find(parsed[2]);
    if (it == completes.end()) {
      cout << "complete: " << parsed[2] << ": no completion specification" << endl;
    } else {
      cout << "complete -C '" << it->second << "' " << it->first << endl;
    }
  } else if (parsed[1] == "-C") {
    if (parsed.size() != 4) return;
    completes[parsed[3]] = parsed[2];
  } else if (parsed[1] == "-r") {
    completes.erase(parsed[2]);
  }
}

void handlejobs(const vector<string> parsed) {
  return;
}

// 执行外部命令，创建子进程，父进程等待子进程结束
void myexecv(const vector<string>& parsed) {
  vector<const char*> argv;
  for (const auto& arg : parsed) {
    argv.push_back(arg.c_str());
  }
  argv.push_back(nullptr);

  pid_t pid = fork();
  if (pid == 0) {
    string cmd_path = findCmdInPath(argv[0]);
    if (!cmd_path.empty()) {
      execvp(cmd_path.c_str(), (char**)argv.data());
    } else {
      execvp(argv[0], (char**)argv.data());
    }
    // 如果execvp失败，子进程退出
    cerr << parsed[0] << ": command not found" << endl;
    exit(1);
  } else if (pid > 0) {
    int status;
    waitpid(pid, &status, 0);
  } else {
    cerr << "fork failed" << endl;
  }
}

// 执行管道命令并捕获标准输出
string pipeexecv(const vector<string>& parsed) {
  int pipefd[2];
  if (pipe(pipefd) == -1) {
    cerr << "pipe failed" << endl;
    return "";
  }

  pid_t pid = fork();
  if (pid == 0) {
    // 子进程：执行命令并将输出写入管道
    close(pipefd[0]); // 关闭读端
    dup2(pipefd[1], STDOUT_FILENO);
    dup2(pipefd[1], STDERR_FILENO); // 也捕获标准错误
    close(pipefd[1]);
    
    vector<const char*> argv;
    for (const auto& arg : parsed) {
      argv.push_back(arg.c_str());
    }
    argv.push_back(nullptr);
    
    string cmd_path = findCmdInPath(argv[0]);
    if (!cmd_path.empty()) {
      execvp(cmd_path.c_str(), (char**)argv.data());
    } else {
      execvp(argv[0], (char**)argv.data());
    }
    exit(1);
  } else if (pid > 0) {
    // 父进程：读取管道输出
    close(pipefd[1]); // 关闭写端
    
    string output;
    char buffer[4096];
    ssize_t n;
    
    while ((n = read(pipefd[0], buffer, sizeof(buffer)-1)) > 0) {
      buffer[n] = '\0';
      output += buffer;
    }
    
    close(pipefd[0]);
    
    int status;
    waitpid(pid, &status, 0);
    
    return output;
  } else {
    cerr << "fork failed" << endl;
    close(pipefd[0]);
    close(pipefd[1]);
    return "";
  }
}

// function definition end