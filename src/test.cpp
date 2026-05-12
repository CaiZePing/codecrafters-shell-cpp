#include <complex>
#include <cstdlib>
#include <fcntl.h>
#include <filesystem>
#include <iostream>
#include <ranges>
#include <sstream>
#include <string>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

std::vector<std::string> parse_input(std::string line) {
  std::vector<std::string> tokens;
  std::string current_token;
  char quote_char = 0;
  bool token_started = false;

  for (size_t i = 0; i < line.length(); i++) {
    char c = line[i];

    if (quote_char == 0) {
      if (c == '\\') {
        if (i + 1 < line.length()) {
          current_token += line[i + 1];
          i++;
          token_started = true;
        }
      } else if (c == '\'' || c == '"') {
        quote_char = c;
        token_started = true;
      } else if (c == ' ') {
        if (token_started) {
          tokens.push_back(current_token);
          current_token.clear();
          token_started = false;
        }
      } else {
        current_token += c;
        token_started = true;
      }
    } else if (quote_char == '\'') {
      if (c == '\'') {
        quote_char = 0;
      } else {
        current_token += c;
      }
    } else if (quote_char == '"') {
      if (c == '"') {
        quote_char = 0;
      } else if (c == '\\') {
        if (i + 1 < line.length()) {
          char next = line[i + 1];
          if (next == '\\' || next == '"' || next == '$' || next == '\n') {
            current_token += next;
            i++;
          } else {
            current_token += c;
          }
        } else {
          current_token += c;
        }
      } else {
        current_token += c;
      }
    }
  }
  if (token_started) {
    tokens.push_back(current_token);
  }
  return tokens;
}

int main() {
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;
  std::string line;
  std::string command;

  while (true) {
    std::cout << "$ " << std::flush;

    if (!std::getline(std::cin, line))
      break;
    if (line.empty())
      continue;

    std::vector<std::string> args = parse_input(line);
    if (args.empty())
      continue;

    std::string command = args[0];

    bool write_into_file = false;
    std::string file;
    int saved = dup(1);
    if (args.size() > 2)
      if (args[args.size() - 2] == ">" || args[args.size() - 2] == "1>") {
        write_into_file = true;
        file = args[args.size() - 1];
        args.pop_back();
        args.pop_back();
        int file_fd = open(file.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
        dup2(file_fd, 1);
        close(file_fd);
      }

    if (command == "echo") {
      for (size_t i = 1; i < args.size(); i++) {
        if (i > 1)
          std::cout << " ";
        std::cout << args[i];
      }
      std::cout << "\n";
    }

    else if (command == "exit") {
      return 0;
    }

    else if (command == "type") {
      if (args.size() < 2)
        continue;
      const std::string &command_to_know = args[1];
      bool found = false;
      std::vector<std::string> builtins = {"echo", "exit", "type", "pwd", "cd"};

      for (const auto &b : builtins)
        if (b == command_to_know) {
          std::cout << command_to_know << " is a shell builtin\n";
          found = true;
        }

      if (!found) {
        std::string path_env = std::getenv("PATH");
        std::stringstream ss_path(path_env);
        std::string path;
        while (std::getline(ss_path, path, ':')) {
          std::string full_path = path + '/' + command_to_know;
          if (access(full_path.c_str(), X_OK) == 0) {
            std::cout << command_to_know << " is " << full_path << "\n";
            found = true;
            break;
          }
        }
      }
      if (!found)
        std::cout << command_to_know << ": not found\n";
    }

    else if (command == "pwd") {
      std::cout << std::filesystem::current_path().string() << "\n";
    }

    else if (command == "cd") {
      const char *target = nullptr;
      const char *home = std::getenv("HOME");

      if (args.size() < 2) {
        target = home;
        if (!target)
          target = "/";
      } else if (args[1] == "~")
        target = home;
      else
        target = args[1].c_str();
      if (chdir(target) != 0)
        std::cout << "cd: " << target << ": No such file or directory\n";
    } else {
      std::vector<char *> c_args;
      for (auto &a : args)
        c_args.push_back(&a[0]);
      c_args.push_back(nullptr);

      pid_t pid = fork();
      if (pid == 0) {
        execvp(command.c_str(), c_args.data());
        std::cout << command << ": command not found\n";
        exit(1);
      } else {
        wait(nullptr);
      }
    }

    if (write_into_file) {
      dup2(saved, 1);
      close(saved);
    }
  }
}