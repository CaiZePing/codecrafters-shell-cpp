#include <cstdlib>
#include <filesystem>
#include <fcntl.h>
#include <iostream>
#include <readline/readline.h>
#include <sstream>
#include <string>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <unordered_set>
#include <vector>
#include <algorithm>

#ifdef _WIN32
constexpr char PATH_LIST_SEPARATOR = ';';
#else
constexpr char PATH_LIST_SEPARATOR = ':';
#endif

using namespace std;
namespace fs = filesystem;

const unordered_set<string> BUILTIN_COMMANDS{"echo", "type", "exit", "pwd", "cd"};
vector<string> PATH{};          // 存储PATH环境变量中的所有路径
vector<string> executables{};   // 存储所有可执行文件
bool only_dir = false;

void clear();                                 // 清屏
void populatePATH(const string& pathstring);  // 将PATH环境变量分解成一个个路径
string findCmdInPath(const string& cmd);      // 在PATH中查找命令
void cacheAllExecutables();                   // 缓存所有可执行文件

char* builtinCompletionGenerator(const char *text, int state);  // 命令补全生成器
char* fileCompletionGenerator(const char *text, int state);     // 文件补全生成器
char** shellCompletion(const char *text, int start, int end);   // shell补全函数

bool readInput(string &input);                                  // 读取用户输入
vector<string> parseInput(const string& command);               // 解析输入命令
void handleInput(const string& parsed);                         // 处理输入
void handleEcho(const vector<string>& parsed);                  // 处理 echo
void handleType(const vector<string>& parsed);                  // 处理 type
void handlepwd(const vector<string>& parsed);                   // 处理 pwd
void handlecd(const vector<string>& parsed);                    // 处理 cd

void myexecv(const vector<string>& parsed);                     // 执行命令

int main() {
  string input;
  string pathstring = getenv("PATH");
  populatePATH(pathstring);
  cacheAllExecutables();
  // Flush after every cout / cerr
  clear();

  rl_attempted_completion_function = shellCompletion;
  
  while (true) {
    readInput(input);
    handleInput(input);
  }
}

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
  for (string directory : PATH) {
    fs::path fullPath = fs::path(directory) / cmd;

    if (fs::exists(fullPath)) {
      auto perms = fs::status(fullPath).permissions();
      if ((perms & (fs::perms::group_exec | fs::perms::owner_exec | fs::perms::others_exec)) == fs::perms::none)
        continue;
      return fullPath.string();
    }
  }

  return "";
}

void cacheAllExecutables() {
  if (!executables.empty()) return;

  unordered_set<string> seen;

  for (const auto& dir : PATH) {
    try {
      if (!fs::exists(dir) || !fs::is_directory(dir))
        continue;
  
      for (const auto& entry : fs::directory_iterator(dir)) {
        if (!entry.is_regular_file())
          continue;
  
        auto perms = entry.status().permissions();
        if ((perms & (fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec)) == fs::perms::none)
          continue;
  
        string name = entry.path().filename().string();
        if (seen.count(name)) continue;
  
        seen.insert(name);
        executables.push_back(name);
      }
    } catch (const fs::filesystem_error& ex) {
      cerr << "Error occurred while accessing filesystem: " << ex.what() << endl;
      continue;
    }
  }
}

char* builtinCompletionGenerator(const char *text, int state) {
  static vector<string> candidates;
  static size_t index = 0;
  static int len;

  if (state == 0) {
    index = 0;
    len = strlen(text);

    // 先把内置命令加进去
    candidates.assign(BUILTIN_COMMANDS.begin(), BUILTIN_COMMANDS.end());

    // 再把 PATH 里所有可执行文件加进去
    cacheAllExecutables();
    candidates.insert(candidates.end(), executables.begin(), executables.end());
  }

  while (index < candidates.size()) {
    string& cmd = candidates[index++];
    if (strncmp(cmd.c_str(), text, len) == 0) {
        return strdup(cmd.c_str());
    }
  }
  return nullptr;
}

char* fileCompletionGenerator(const char *text, int state) {
  static vector<string> matches;
  static size_t matchIndex;
  extern bool only_dir; // 是否只补全目录（cd 命令专用）

  // state=0：初始化，生成匹配列表
  if (state == 0) {
    matches.clear();
    matchIndex = 0;
    string prefix(text);
    fs::path base_path;
    string name_prefix;

    // ------------- 1. 解析路径：分离 目录 + 前缀 -------------
    if (prefix.empty() || fs::is_directory(prefix)) {
      // 空输入 / 直接输入目录
      base_path = prefix.empty() ? "." : prefix;
      name_prefix = "";
    } else {
      // 带前缀的路径（如 test/abc → 目录test/，前缀abc）
      base_path = fs::path(prefix).parent_path();
      name_prefix = fs::path(prefix).filename().string();
    }

    // ------------- 2. 遍历目录，筛选匹配项 -------------
    try {
      if (!fs::exists(base_path) || !fs::is_directory(base_path)) {
        return nullptr;
      }

      for (const auto& entry : fs::directory_iterator(base_path)) {
        string name = entry.path().filename().string();
        // 前缀匹配
        if (name.compare(0, name_prefix.size(), name_prefix) != 0) continue;
        // 筛选：cd 只保留目录
        if (only_dir && !entry.is_directory()) continue;

        // 拼接完整路径
        fs::path full_path = base_path / name;
        string match_str = full_path.string();
        // 目录自动加 /（标准 shell 行为）
        if (entry.is_directory()) match_str += "/";

        matches.push_back(match_str);
      }
      // 排序，提升体验
      sort(matches.begin(), matches.end());
    } catch (...) {
      return nullptr;
    }
  }

  // ------------- 3. 返回匹配项 -------------
  if (matchIndex < matches.size()) {
    return strdup(matches[matchIndex++].c_str());
  }

  return nullptr;
}

char** shellCompletion(const char *text, int start, int end) {
  // 消除「未使用函数参数」的编译器警告
  (void)end;
  // 强制禁用 readline 自带的所有默认补全
  rl_attempted_completion_over = 1;

  if(start == 0) {
    // 补全后追加的字符
    rl_completion_append_character = ' ';
    char** matches = rl_completion_matches(text, builtinCompletionGenerator);
    if (!matches)
      cout << "\a" << flush;
    return matches;
  }
  // 补全后追加的字符
  rl_completion_append_character = ' ';

  string cmd_line(rl_line_buffer);
  vector<string> parsed = parseInput(cmd_line);
  bool is_cd = (!parsed.empty() && parsed[0] == "cd");
  only_dir = is_cd;

  return rl_completion_matches(text, fileCompletionGenerator);
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
    if (command[i] == '\'' && !isDoubleQuote ) {
      isSingleQuote ^= 1;
      isSpace = false;
    } else if (command[i] == '\"' && !isSingleQuote) {
      isDoubleQuote ^= 1;
      isSpace = false;
    } else if (command[i] == '\\' && !isSingleQuote && i + 1 < command.size()) {
      current += command[++i];
    } else if (command[i] == ' ' && !isSingleQuote && !isDoubleQuote){
      if (isSpace) continue;
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
  // 备份标准输出
  int backup_stdout = 0;
  int backup_who = -1;
  bool write_into_file = false;
  string file;
  // for (int i = 0; i < parsed.size(); ++i) {
  //   cout << "parsed[" << i << "]: " << parsed[i] << endl;
  // }
  // cout << endl;
  // cout << "handleInput: " << command << endl;

  if (parsed.size() > 2) {
    string redirection = parsed[parsed.size() - 2];
    if (redirection.back() == '>') {
      write_into_file = true;
      file = parsed[parsed.size() - 1];
      parsed.pop_back();
      parsed.pop_back();
      char flag = 0;
      size_t size = redirection.size();
      if (redirection == ">>" || redirection == "1>>" || redirection == "2>>") {
        flag |= 0b01; // 第 0 位设为 1 → 追加
      }
      if (redirection == "2>" || redirection == "2>>") {
        flag |= 0b10; // 第 1 位设为 1 → 标准错误输出重定向
      }
      /**
       * open 函数有关参数
       * O_RDONLY      // 只读（给 < 输入重定向用）
       * O_WRONLY      // 只写（给 > >> 输出重定向用）
       * O_RDWR        // 读写
       * O_CREAT       // 文件不存在就创建
       * O_TRUNC       // 清空文件（给 > 用）
       * O_APPEND      // 追加（给 >> 用）
       */
      int fd = open(file.c_str(), O_WRONLY | O_CREAT | (flag & 0b01 ? O_APPEND : O_TRUNC), 0644);
      backup_who = flag & 0b10 ? 2 : 1;
      backup_stdout = dup(backup_who);
      dup2(fd, backup_who);
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
    clear();
  } else if (findCmdInPath(command) != "") {
    myexecv(parsed);
  } else {
    cout << command << ": command not found" << endl;
  }

  // 将输出重定向回标准输出
  if (write_into_file) {
    dup2(backup_stdout,backup_who);
    close(backup_stdout);
  }
}

// 处理echo命令，输出参数
void handleEcho(const vector<string>& parsed) {
  for (size_t i = 1; i < parsed.size(); i++) {
    cout << parsed[i] << " ";
  }

  cout << endl;
}

// 处理type命令，判断参数是内置命令还是可执行文件
void handleType(const vector<string>& parsed) {
  string command = parsed[0];
  string arg1 = parsed[1];

  if (BUILTIN_COMMANDS.count(arg1) > 0) {
    cout << arg1 << " is a shell builtin" << endl;
  } else if (findCmdInPath(arg1) != "") {
    cout << arg1 << " is " << findCmdInPath(arg1) << endl;
  } else {
    cout << arg1 << ": not found" << endl;
  }
}

// 处理pwd命令，输出当前路径
void handlepwd(const vector<string>& parsed) {
  cout << fs::current_path().string() << endl;
}

// 处理cd命令，切换当前路径
void handlecd(const vector<string>& parsed) {
  if (parsed.size() < 2) {
    cout << "cd: missing operand" << endl;
    return;
  }
  // 获取从命令行输入的路径
  string newPath = parsed[1];
  if (newPath[0] == '/') {
    if (!fs::exists(newPath) || !fs::is_directory(newPath)) {
      cout << "cd: " << parsed[1] << ": No such file or directory" << endl;
      return;
    }
    // 设置当前工作目录
    fs::current_path(newPath);
  } else if (newPath[0] == '~'){ // 用户目录
    string homestring = getenv("HOME"); // 获取用户目录
    newPath.replace(0, 1, homestring); // 将输入地址的 ~ 替换为用户目录
    if (!fs::exists(newPath) || !fs::is_directory(newPath)){
      cout << "cd: " << newPath << " No such file or directory" << endl;
      return;
    }
    fs::current_path(newPath);
  } else {
    fs::path fullPath = fs::current_path() / fs::path(newPath);
    if (!fs::exists(fullPath) || ! fs::is_directory(fullPath)){
      cout << "cd: " << newPath << ": No such file or directory" << endl;
      return; 
    }
    fs::current_path(fullPath);
  }
}

// 执行外部命令，创建子进程，父进程等待子进程结束
void myexecv(const vector<string>& parsed) {
  const char **argv = new const char *[parsed.size() + 1];
  for (int j = 0; j < parsed.size(); ++j)
    argv[j] = parsed[j].c_str();
  argv[parsed.size()] = NULL;

  pid_t pid = fork();
  if (pid == 0) {
    // execvp 会将当前进程替换成要执行的命令
    // 所以子进程会被替换成要执行的命令，父进程继续等待子进程结束
    execvp(argv[0], (char **)argv);
  } else {
    int status;
    wait(&status);
  }
}
