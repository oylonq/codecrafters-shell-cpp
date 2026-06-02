#include <iostream>
#include <string>

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

    // Print: Display the output or error message
    std::cout << input << ": command not found" << '\n';

  } // Loop: Return to step 1 and wait for the next command

  return 0;
}
