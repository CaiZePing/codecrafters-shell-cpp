#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <unordered_set>
#include <vector>


void populatePATH(const std::string& pathstring);
std::string findCmdInPath(const std::string& cmd);

std::vector<std::string> parseInput(const std::string& command);
void handleInput(const std::string& parsed);
void handleEcho(const std::vector<std::string>& parsed);
void handleType(const std::vector<std::string>& parsed);
void handlepwd(const std::vector<std::string>& parsed);
void handlecd(const std::vector<std::string>& parsed);

void myexecv(const std::vector<std::string>& parsed);

const std::unordered_set<std::string> BUILTIN_COMMANDS{"echo", "type", "exit", "pwd", "cd"};
std::vector<std::string> PATH{};

int main() {
  std::string input;
  std::string pathstring = getenv("PATH");
  populatePATH(pathstring);

  // Flush after every cout / cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  while (true) {
    std::cout << "$ ";
    std::getline(std::cin, input);
    handleInput(input);
  }
}
// 将PATH环境变量分解成一个个路径，存储在全局变量PATH中
void populatePATH(const std::string& pathstring) {
  std::stringstream ss{pathstring};
  std::string patharg;
  while (std::getline(ss, patharg, ':')) {
    PATH.push_back(patharg);
  }
}
// 查找是不是可执行文件，返回绝对路径，否则返回空字符串
std::string findCmdInPath(const std::string& cmd) {
  for (std::string directory : PATH) {
    std::filesystem::path fullPath = std::filesystem::path(directory) / cmd;

    if (std::filesystem::exists(fullPath)) {
      auto perms = std::filesystem::status(fullPath).permissions();
      if ((perms & (std::filesystem::perms::group_exec | std::filesystem::perms::owner_exec |
                    std::filesystem::perms::others_exec)) == std::filesystem::perms::none)
        continue;
      return fullPath.string();
    }
  }

  return "";
}
// 分解输入命令
std::vector<std::string> parseInput(const std::string& command) {
  std::stringstream ss{command};
  std::vector<std::string> parsed{};
  std::string commandArg;

  while (std::getline(ss, commandArg, ' ')) {
    parsed.push_back(commandArg);
  }

  return parsed;
}
// 处理输入
void handleInput(const std::string& input) {
  auto parsed = parseInput(input);
  auto command = parsed[0];

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
    std::cout << command << ": command not found" << std::endl;
  }
}
// 处理echo命令，输出参数
void handleEcho(const std::vector<std::string>& parsed) {
  for (size_t i = 1; i < parsed.size(); i++) {
    std::cout << parsed[i] << " ";
  }

  std::cout << std::endl;
}
// 处理type命令，判断参数是内置命令还是可执行文件
void handleType(const std::vector<std::string>& parsed) {
  std::string command = parsed[0];
  std::string arg1 = parsed[1];

  if (BUILTIN_COMMANDS.count(arg1) > 0) {
    std::cout << arg1 << " is a shell builtin" << std::endl;
  } else if (findCmdInPath(arg1) != "") {
    std::cout << arg1 << " is " << findCmdInPath(arg1) << std::endl;
  } else {
    std::cout << arg1 << ": not found" << std::endl;
  }
}
// 处理pwd命令，输出当前路径
void handlepwd(const std::vector<std::string>& parsed) {
  std::cout << std::filesystem::current_path().string() << std::endl;
}
// 处理cd命令，切换当前路径
void handlecd(const std::vector<std::string>& parsed) {
  // 获取从命令行输入的路径
  std::filesystem::path newPath = parsed[1];
  if (newPath[0] == '/') {
    if (std::filesystem::exists(newPath) || std::filesystem::is_directory(newPath)) {
      std::cout << "cd: " << parsed[1] << ": No such file or directory" << std::endl;
    }
    std::filesystem::current_path(newPath);
  }
}
// 执行外部命令，创建子进程，父进程等待子进程结束
void myexecv(const std::vector<std::string>& parsed) {
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