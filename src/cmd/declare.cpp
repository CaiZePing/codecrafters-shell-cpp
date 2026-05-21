#include "declare.hpp"

#include <iostream>

namespace cmd {
std::unordered_map<std::string, std::string> variables{};
void declare(const std::vector<std::string>& parsed) {
    if (parsed.size() > 2) {
        if (parsed[1] == "-p") {
            if ( variables.find(parsed[2]) != variables.end() ) {
                std::cout << "declare -- " << parsed[2] << "=\"" << variables[parsed[2]] << "\"" << std::endl;
            } else {
                std::cerr << "declare: " << parsed[2] << ": not found" << std::endl;
            }
        }
    } else if (parsed.size() == 2) {
        // 它以字母或下划线开头，其余部分可以使用字母、数字和下划线
        if (!std::isalpha(parsed[1][0]) && parsed[1][0] != '_') {
            std::cerr << "declare: `" << parsed[1] << "\': not a valid identifier" << std::endl;
            return;
        }
        size_t pos = parsed[1].find('=');
        if (pos != std::string::npos) {
            variables[parsed[1].substr(0, pos)] = parsed[1].substr(pos + 1);
        }
    }
}
}