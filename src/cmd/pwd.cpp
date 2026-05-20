#include "pwd.hpp"

#include <iostream>
#include <filesystem>

namespace cmd {
    void pwd() {
        std::cout << std::filesystem::current_path().string() << std::endl;
    }
}