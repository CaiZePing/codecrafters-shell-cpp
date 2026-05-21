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
        size_t pos = parsed[1].find('=');
        if (pos != std::string::npos) {
            std::string key = parsed[1].substr(0, pos);
            // key 它以字母或下划线开头，其余部分可以使用字母、数字和下划线
            if (!std::isalpha(key[0]) && key[0] != '_') {
                std::cerr << "declare: `" << parsed[1] << "\': not a valid identifier" << std::endl;
                return;
            }
            for (const char c : key) {
                if (!std::isalnum(c) && c != '_') {
                    std::cerr << "declare: `" << parsed[1] << "\': not a valid identifier" << std::endl;
                    return;
                }
            }
            variables[key] = parsed[1].substr(pos + 1);
        }
    }
}

void handleVariable(std::vector<std::string>& parsed) {
    for (auto& it : parsed) {
        int pos = it.find('$');
        while (pos != std::string::npos) {
            std::string temp = it.substr(0, pos);
            std::string key = it.substr(pos + 1);
            if (key[0] == '{') {
                it = temp;
                key = key.substr(1);
                pos = key.find('}');
                if (pos != std::string::npos) {
                    temp = key.substr(pos +1);
                    key = key.substr(0, pos);
                    if (variables.find(key) != variables.end()) {
                        it += variables[key];
                    }
                }
                it += temp;
            } else if (variables.find(key) != variables.end()) {
                it = temp + variables[key];
            } else {
                it = temp;
            }
            pos = it.find('$');
        }
    }
    // 将所有空字符串移除
    parsed.erase(std::remove(parsed.begin(), parsed.end(), ""), parsed.end());
}
}