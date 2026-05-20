#pragma once

#include <vector>
#include <string>

namespace cmd {
    void complete(const std::vector<std::string>& parsed);

    char* builtinCompletionGenerator(const char *text, int state);  // 命令补全生成器
    char* externalCompletionGenerator(const char *text, int state); // 外部命令补全生成器
    char* fileCompletionGenerator(const char *text, int state);     // 文件补全生成器
    char** shellCompletion(const char *text, int start, int end);   // shell补全函数
}