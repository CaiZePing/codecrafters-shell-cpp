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
    std:getline(std::cin, command);
    if(command == "exit") {
      break;
    }
    if(command.substr(0, 5) == "echo ") {
      std::cout << command.substr(5) << std::endl;
    }
    std::cout << command << ": command not found" <<std::endl;
  }
}
