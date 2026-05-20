#include "clear.hpp"

#include <cstdlib>

namespace cmd {
void clear() {
#ifdef _WIN32
    system("cls");   // Windows
#else
    system("clear"); // Linux / macOS
#endif
}
}