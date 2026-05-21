#pragma once

#include <iostream>
#include <sstream>
#include <filesystem>
#include <unordered_set>
#include <unordered_map>
#include <readline/readline.h>
#include <readline/history.h>
#include <algorithm>
#include <unistd.h>
#include <fcntl.h>

#include "echo.hpp"
#include "type.hpp"
#include "exit.hpp"
#include "pwd.hpp"
#include "cd.hpp"
#include "complete.hpp"
#include "jobs.hpp"
#include "clear.hpp"

namespace cmd {
#ifdef _WIN32
constexpr char PATH_LIST_SEPARATOR = ';';
#else
constexpr char PATH_LIST_SEPARATOR = ':';
#endif

extern const std::unordered_set<std::string> BUILTIN_COMMANDS;
extern std::vector<std::string> executables;              // 存储所有可执行文件
extern std::unordered_map<std::string, std::string> completes; // 存储注册命令


extern std::vector<std::string> PATH;              // 存储PATH环境变量中的所有路径

// 自定义的两个环境变量
extern std::string COMP_LINE;
extern std::string COMP_POINT;

void populatePATH(const std::string& pathstring);       // 将PATH环境变量分解成一个个路径
std::string findCmdInPath(const std::string& cmd);      // 在PATH中查找命令
void cacheAllExecutables();                             // 缓存所有可执行文件

bool isBuiltinCommand(const std::string& command); // 是内部命令

bool readInput(std::string &input);                                           // 读取用户输入
std::vector<std::string> parseInput(const std::string& command);              // 解析输入命令
std::vector<std::string> splitByPipe(const std::vector<std::string>& parsed); // 解析管道命令
void handleInput(const std::string& parsed);                                  // 处理输入
}