#include "declare.hpp"

#include <iostream>

namespace cmd {
std::unordered_map<std::string, std::string> variables{};
void declare(const std::vector<std::string>& parsed) {
    if (parsed.size() > 2) {
        if (parsed[1] == "-p") {
            if ( variables.find(parsed[2]) != variables.end() ) {
                std::cout << parsed[2] << "=" << variables[parsed[2]] << std::endl;
            } else {
                std::cerr << "declare: " << parsed[2] << ": not found" << std::endl;
            }
        }
    }
}
}