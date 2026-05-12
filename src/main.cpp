#include <cstdlib>
#include <filesystem>
#include <fcntl.h>
#include <iostream>
#include <sstream>
#include <string>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <unordered_set>
#include <vector>

#ifdef _WIN32
constexpr char PATH_LIST_SEPARATOR = ';';
#else
constexpr char PATH_LIST_SEPARATOR = ':';
#endif

using namespace std;
namespace fs = filesystem;

void populatePATH(const string& pathstring);
string findCmdInPath(const string& cmd);

vector<string> parseInput(const string& command);
void handleInput(const string& parsed);
void handleEcho(const vector<string>& parsed);
void handleType(const vector<string>& parsed);
void handlepwd(const vector<string>& parsed);
void handlecd(const vector<string>& parsed);

void myexecv(const vector<string>& parsed);

const unordered_set<string> BUILTIN_COMMANDS{"echo", "type", "exit", "pwd", "cd"};
vector<string> PATH{};

int main() {
  string input;
  string pathstring = getenv("PATH");
  populatePATH(pathstring);

  // Flush after every cout / cerr
  cout << unitbuf;
  cerr << unitbuf;

  while (true) {
    cout << "$ ";
    getline(cin, input);
    handleInput(input);
  }
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
      if ((perms & (fs::perms::group_exec | fs::perms::owner_exec |
                    fs::perms::others_exec)) == fs::perms::none)
        continue;
      return fullPath.string();
    }
  }

  return "";
}
// 分解输入命令
vector<string> parseInput(const string& command) {
  vector<string> parsed;
  string current;
  bool isSingleQuote = false;
  bool isDoubleQuote = false;
  bool isBackslash = false;
  bool isSpace = false;

  for (char c : command) {
    if (isBackslash) {
      current += c;
      isBackslash = false;
    } else if (c == '\'' && !isDoubleQuote ) {
     isSingleQuote ^= 1;
     isSpace = false;
    } else if (c == '\"' && !isSingleQuote) {
      isDoubleQuote ^= 1;
      isSpace = false;
    } else if (c == '\\' && !isSingleQuote) {
      isBackslash = true;
    } else if (c == ' ' && !isSingleQuote && !isDoubleQuote){
      if (isSpace) continue;
      parsed.push_back(current);
      current.clear();
      isSpace = true;
    } else {
      current += c;
      isSpace = false;
    }
  }
  if (!current.empty())
    parsed.push_back(current);
  return parsed;
}
// 处理输入
void handleInput(const string& input) {
  auto parsed = parseInput(input);
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
      /**
       * open 函数有关参数
       * O_RDONLY      // 只读（给 < 输入重定向用）
       * O_WRONLY      // 只写（给 > >> 输出重定向用）
       * O_RDWR        // 读写
       * O_CREAT       // 文件不存在就创建
       * O_TRUNC       // 清空文件（给 > 用）
       * O_APPEND      // 追加（给 >> 用）
       */
      int fd;
      if (redirection == ">" || redirection == "1>") {
        fd = open(file.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
        backup_stdout = dup(1);
        backup_who = 1;
        dup2(fd, 1);
      } else {
        if (redirection[0] == '2'){
          fd = open(file.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
          backup_stdout = dup(2);
          backup_who = 2;
          dup2(fd, 2);
        } else if (redirection == ">>") {
          fd = open(file.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);
          backup_stdout = dup(1);
          backup_who = 1;
          dup2(fd, 1);
        }
      }
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
    fs::current_path(newPath);
  } else if (newPath[0] == '~'){
    string homestring = getenv("HOME");
    newPath.replace(0, 1, homestring);
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
