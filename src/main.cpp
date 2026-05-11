#include <iostream>
#include <ranges>
#include <sstream>
#include <string>
#include <unistd.h>

#ifdef _WIN32
constexpr char PATH_LIST_SEPARATOR = ';';
#else
constexpr char PATH_LIST_SEPARATOR = ':';
#endif

int main() {
  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;
  std::string line;
  std::string command;

  while (true) {
    // TODO: Uncomment the code below to pass the first stage
    std::cout << "$ ";
    // get line
    std::getline(std::cin, line);
    // parse command
    std::stringstream ss(line);
    ss >> command;
    // check for exit
    if(command == "exit") {
      break;
    } else if(command == "echo") { // 如果指令为 echo
      std::string word;
      while(ss >> word) 
        std::cout << word << " ";
      std::cout << std::endl;
    } else if (command == "type") { // 如果指令为 type
      bool found = false; // 是否找到
      std::string builtin[3] = {"echo", "type", "exit"}; // 当前的内部命令
      std::string command_to_know; // type 后面接着的命令
      ss >> command_to_know;
      // 将当前的内部命令遍历一遍，查找一下是不是内部命令
      for (int i = 0; i < sizeof(builtin)/sizeof(builtin[0]); i++) {
	      if (builtin[i] == command_to_know) {
	        std::cout << command_to_know << " is a shell builtin\n";
	        found = true;
	        break;
	      }
      }
      // 没有找到内部命令，看是不是可执行文件
      if (!found) {
	      // 获取环境变量
        std::string path_env = std::getenv("PATH");
	      std::stringstream ss_path(path_env);
        std::string path;
	      // 拆分环境变量
	      while(std::getline(ss_path,path,PATH_LIST_SEPARATOR)) {
          // 拼接路径名 + 文件名
          std::string full_path = path + '/' + command_to_know;
          // 查看是不是可执行文件
          if(access(full_path.c_str(),X_OK) == 0) {
            std::cout << command_to_know << " is " << full_path << std::endl;
            found = true;
            break;
          }
	      }
      }
      if (!found) {
	      std::cout << command_to_know << ": not found" << std::endl;
      }
    } else {
      std::cout << command << ": command not found" << std::endl;
    }    
  }
}
