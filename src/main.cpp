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


void populatePATH(std::string pathstring);
std::string findCmdInPath(std::string cmd);

std::vector<std::string> parseInput(std::string command);
void handleInput(std::string command);
void handleEcho(std::vector<std::string> command);
void handleType(std::vector<std::string> command);
void handlepwd(std::vector<std::string> parsed);

void myexecv(std::vector<std::string> parsed);

const std::unordered_set<std::string> BUILTIN_COMMANDS{"echo", "type", "exit", "pwd"};
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

void populatePATH(std::string pathstring) {
  std::stringstream ss{pathstring};
  std::string patharg;
  while (std::getline(ss, patharg, ':')) {
    PATH.push_back(patharg);
  }
}

std::string findCmdInPath(std::string cmd) {
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

std::vector<std::string> parseInput(std::string command) {
  std::stringstream ss{command};
  std::vector<std::string> parsed{};
  std::string commandArg;

  while (std::getline(ss, commandArg, ' ')) {
    parsed.push_back(commandArg);
  }

  return parsed;
}

void handleInput(std::string input) {
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
  } else if (findCmdInPath(command) != "") {
    myexecv(parsed);
  } else {
    std::cout << command << ": command not found" << std::endl;
  }
}

void handleEcho(std::vector<std::string> parsed) {
  for (size_t i = 1; i < parsed.size(); i++) {
    std::cout << parsed[i] << " ";
  }

  std::cout << std::endl;
}

void handleType(std::vector<std::string> parsed) {
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

void handlepwd(std::vector<std::string> parsed) {
  std::cout << std::filesystem::current_path().string() << std::endl;
}

void myexecv(std::vector<std::string> parsed) {
  const char **argv = new const char *[parsed.size() + 1];
  for (int j = 0; j < parsed.size(); ++j)
    argv[j] = parsed[j].c_str();
  argv[parsed.size()] = NULL;

  pid_t pid = fork();
  if (pid == 0) {
    execvp(argv[0], (char **)argv);
  } else {
    int status;
    wait(&status);
  }
}