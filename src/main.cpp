#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <unordered_map>
#include <vector>

namespace Shell {
std::vector<std::string> argv;
};
enum class CMDS { EXIT, ECHO, TYPE, PWD, CD, OTHER };
std::unordered_map<std::string, CMDS> builtin{{"exit", CMDS::EXIT},
                                              {"echo", CMDS::ECHO},
                                              {"type", CMDS::TYPE},
                                              {"pwd", CMDS::PWD},
                                              {"cd", CMDS::CD}};

std::string trimStr(std::string &input);
std::vector<std::string> splitStr(std::string &s, char symbol);
bool getPath(std::string &command, std::string &cmd_path);

void exitBuiltin();
void echoBuiltin();
void typeBuiltin();
void pwdBuiltin();
void cdBuiltin();

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
    std::string trimed_input = trimStr(input);
    Shell::argv = splitStr(trimed_input, ' ');

    CMDS condition = builtin.find(Shell::argv[0]) != builtin.end()
                         ? builtin[Shell::argv[0]]
                         : CMDS::OTHER;
    switch (condition) {
    case CMDS::EXIT:
      exitBuiltin();
      break;

    case CMDS::ECHO:
      echoBuiltin();
      break;

    case CMDS::TYPE:
      typeBuiltin();
      break;

    case CMDS::PWD:
      pwdBuiltin();
      break;

    case CMDS::CD:
      cdBuiltin();
      break;

    default: {
      pid_t pid;
      // Determine if the give command is an executable
      std::string cmd_path;
      if (getPath(Shell::argv[0], cmd_path)) {
        // if it is, execute the program
        pid = fork();
        if (pid == 1) {
          std::perror("Fork failed");
          return 1;
        } else if (pid == 0) {
          // Inside child process
          char *sub_argv[Shell::argv.size() + 1];
          for (int i = 0; i < Shell::argv.size(); ++i) {
            sub_argv[i] = const_cast<char *>(Shell::argv[i].c_str());
          }
          sub_argv[Shell::argv.size()] = nullptr;

          // pass any arguments from the command line to the program
          execv(cmd_path.c_str(), sub_argv);

          std::perror("execv failed");
          exit(1);
        }
      } else {
        // Print: Display the output or error message

        std::cout << Shell::argv[0] << ": command not found" << '\n';
      }

      int status;
      waitpid(pid, &status, 0);
    } break;
    } // Switch End

  } // Loop: Return to step 1 and wait for the next command

  return 0;
}

bool getPath(std::string &command, std::string &cmd_path) {
  std::string PATH{getenv("PATH")};
  std::vector<std::string> PATHS = splitStr(PATH, ':');

  for (std::string path : PATHS) {
    std::string full_path = path + "/" + command;

    if (std::filesystem::exists(full_path)) {

      std::filesystem::perms p =
          std::filesystem::status(full_path).permissions();

      if (std::filesystem::perms::none !=
          ((std::filesystem::perms::owner_exec |
            std::filesystem ::perms ::group_exec |
            std::filesystem::perms::others_exec) &
           p)) {
        cmd_path = full_path;
        return true;
      }
    }
  }

  cmd_path = "";
  return false;
}

std::string trimStr(std::string &input) {
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

std::vector<std::string> splitStr(std::string &s, char symbol) {
  std::vector<std::string> res;
  int n = s.size();

  int start = 0;
  for (int end = 0; end < n; ++end) {
    if (s[end] == symbol) {
      res.push_back(s.substr(start, end - start));
      start = end + 1;
    }
  }

  res.push_back(s.substr(start));

  return res;
}

void exitBuiltin() { exit(0); }

void echoBuiltin() {
  for (int i = 1; i < Shell::argv.size(); ++i) {
    std::cout << Shell::argv[i] << ' ';
  }
  std::cout << '\n';
}

void typeBuiltin() {
  if (Shell::argv.size() > 0) {
    if (builtin.find(Shell::argv[1]) != builtin.end()) {
      std::cout << Shell::argv[1] << " is a shell builtin" << '\n';

    } else { // cmd is not a builtin
      // Get PATH
      std::string PATH{getenv("PATH")};
      std::vector<std::string> PATHs = splitStr(PATH, ':');
      std::string full_path;
      bool is_exec = false;

      for (std::string &path : PATHs) {
        full_path = path + "/" + Shell::argv[1];
        if (std::filesystem::exists(full_path)) {
          std::filesystem::perms p =
              std::filesystem::status(full_path).permissions();

          is_exec = std::filesystem::perms::none !=
                    ((std::filesystem::perms::owner_exec |
                      std::filesystem ::perms ::group_exec |
                      std::filesystem::perms::others_exec) &
                     p);

          if (is_exec) {
            break;
          }
        }
      }

      if (is_exec) {
        std::cout << Shell::argv[1] << " is " << full_path << '\n';
      } else {
        std::cout << Shell::argv[1] << ": not found" << '\n';
      }
    }
  }
}

void pwdBuiltin() {
  std::filesystem::path cwd = std::filesystem::current_path();
  std::cout << std::string(cwd) << '\n';
}

void cdBuiltin() {
  std::filesystem::path target_dir(Shell::argv[1] == "~" ? getenv("HOME")
                                                         : Shell::argv[1]);
  if (std::filesystem::is_directory(target_dir)) {
    std::filesystem::current_path(target_dir);
  } else {
    std::cout << "cd: " << Shell::argv[1] << ": No such file or directory"
              << '\n';
  }
}
