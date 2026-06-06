#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <unordered_set>
#include <vector>

struct Command {
  std::string cmd;
  std::vector<std::string> args;
};

std::unordered_set<std::string> builtin{"echo", "exit", "type"};

Command parseInput(std::string &input);
std::string trimString(std::string &input);
std::vector<std::string> splitStr(std::string s, char symbol);

int main() {
  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  while (true) {
    // Read: Display a prompt and wait for user input
    std::cout << "$ ";

    std::string input;
    std::getline(std::cin, input);

    // Eval: Parse and execute the command
    auto [cmd, args] = parseInput(input);

    if (cmd == "exit") {

      return 0;

    } else if (cmd == "echo") {

      if (!args.empty()) {
        std::cout << args[0] << '\n';
      } else {
        std::cout << '\n';
      }

    } else if (cmd == "type") {

      if (!args.empty()) {
        if (builtin.find(args[0]) != builtin.end()) {
          std::cout << args[0] << " is a shell builtin" << '\n';

        } else { // cmd is not a builtin
          // Get PATH
          std::string PATH{getenv("PATH")};
          std::vector<std::string> PATHs = splitStr(PATH, ':');
          bool is_exec = false;

          for (std::string path : PATHs) {
            if (std::filesystem::exists(path + "/" + args[0])) {
              std::filesystem::perms p =
                  std::filesystem::status(path + "/" + args[0]).permissions();
              is_exec = std::filesystem::perms::none !=

                        ((std::filesystem::perms::owner_exec |
                          std::filesystem ::perms ::group_exec |
                          std::filesystem::perms::others_exec) &
                         p);
              if (is_exec) {
                std::cout << args[0] << " is " + path + "/" + args[0] << '\n';
                break;
              }
            }
          }
          if (!is_exec)
            std::cout << args[0] << ": not found" << '\n';
        }
      }

    } else {
      // Print: Display the output or error message
      std::cout << cmd << ": command not found" << '\n';
    }

  } // Loop: Return to step 1 and wait for the next command

  return 0;
}

Command parseInput(std::string &input) {
  Command res;

  // Trim input
  std::string trimed_input = trimString(input);

  // Find first delimit
  int n = trimed_input.size();
  if (n == 0) {
    return res;
  }

  int pos = 0;
  while (pos < n && trimed_input[pos] != ' ') {
    pos++;
  }

  res.cmd = trimed_input.substr(0, pos - 0);
  if (pos == n) {
    return res;
  }

  res.args.push_back(trimed_input.substr(pos + 1, n - pos - 1));
  return res;
}

std::string trimString(std::string &input) {
  int left = 0;
  int right = input.size() - 1;

  while (left <= right && input[left] == ' ') {
    left++;
  }
  while (left <= right && input[right] == ' ') {
    right--;
  }

  return input.substr(left, right - left + 1);
}
std::vector<std::string> splitStr(std::string s, char symbol) {
  std::vector<std::string> res;

  int start = 0;
  for (int end = 0; end < s.size(); ++end) {
    if (s[end] == symbol) {
      res.push_back(s.substr(start, end - start));
      start = end + 1;
    }
  }

  return res;
}
