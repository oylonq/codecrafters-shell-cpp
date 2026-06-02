#include <iostream>
#include <string>
#include <vector>

struct Command {
  std::string cmd;
  std::vector<std::string> args;
};

Command parseInput(std::string &input);

std::string trimString(std::string &input);

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
    const auto &[cmd, args] = parseInput(input);

    if (cmd == "exit") {

      return 0;

    } else if (cmd == "echo") {

      std::cout << args[0] << '\n';

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
  while (trimed_input[pos] != ' ' && pos < n) {
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

  while (left <= right) {
    while (left <= right && input[left] == ' ') {
      left++;
    }
    while (left <= right && input[right] == ' ') {
      right--;
    }
  }

  return input.substr(left, right - left + 1);
}
