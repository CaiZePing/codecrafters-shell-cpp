#include "echo.hpp"
#include <iostream>

namespace cmd {

    void echo(const std::vector<std::string>& parsed) {
        for (size_t i = 1; i < parsed.size(); i++) {
            if (i > 1) std::cout << " ";
            std::cout << parsed[i];
        }
        std::cout << std::endl;
    }
}