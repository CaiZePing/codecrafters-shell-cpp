#include <iostream>
#include <string>

int main() {
  std::string command;
  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  while (true) {
    // TODO: Uncomment the code below to pass the first stage
    std::cout << "$ ";
    // get line
    std:getline(std::cin, command);
    // check for exit
    if(command == "exit") {
      break;
    }
    // check for echo
    if(command.substr(0, 5) == "echo ") {
      std::cout << command.substr(5) << std::endl;
    } else if(command.substr(0,5) == "type ") {
      std::string cmd = command.substr(5);
      if(cmd == "echo" || cmd == "type" || cmd == "exit") {
        std::cout << command.substr(5) << " is a shell builtin" << std::endl;
      } else {
        std::cout << command.substr(5) << " not found" << std::endl;
      }
    } else {
      std::cout << command << ": command not found" <<std::endl;
    }
  }
}
