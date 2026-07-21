#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <errno.h>
#include <fcntl.h>
#include <filesystem>
#include <iostream>
#include <string>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>
#include <unordered_map>
#include <vector>

namespace Shell {
std::vector<std::string> argv;
};
enum class CMDS { CmdEXIT, CmdECHO, CmdTYPE, CmdPWD, CmdCD, CmdOTHER };
std::unordered_map<std::string, CMDS> builtin{{"exit", CMDS::CmdEXIT},
                                              {"echo", CMDS::CmdECHO},
                                              {"type", CMDS::CmdTYPE},
                                              {"pwd", CMDS::CmdPWD},
                                              {"cd", CMDS::CmdCD}};

std::string trimStr(std::string &input);
std::vector<std::string> splitStr(std::string &str, char delimiter);
bool getPath(std::string &command, std::string &cmd_path);

struct termios orig_termios;
void die(const char *s) {
  perror(s);
  exit(1);
}

void disableRawMode() {
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios) == -1)
    die("tcsetattr");
}
void enableRawMode() {
  if (tcgetattr(STDIN_FILENO, &orig_termios) == -1)
    die("tcgetattr");
  atexit(disableRawMode);

  struct termios raw = orig_termios;
  raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
  raw.c_oflag &= ~(OPOST);
  raw.c_cflag |= (CS8);
  raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
  raw.c_cc[VMIN] = 0;
  raw.c_cc[VTIME] = 1;

  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
    die("tcsetattr");
}

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
    enableRawMode();

    std::string input;

    std::vector<std::string> matching;
    while (1) {
      char c = '\0';
      if (read(STDIN_FILENO, &c, 1) == -1 && errno != EAGAIN)
        die("read");

      if (iscntrl(c)) {
        if (c == '\x08' || c == '\x7f') {
          if (!input.empty()) {
            input.pop_back();
            write(STDOUT_FILENO, "\b \b", 3);
          }
        } else if (c == '\r' || c == '\n') {
          write(STDOUT_FILENO, "\r\n", 2);
          break;
        } else if (c == '\x09') {
          write(STDOUT_FILENO, "\x07", 1);
          std::vector<std::string> autocompletion{"echo", "exit"};
          std::string PATH{getenv("PATH")};
          std::vector<std::string> PATHS = splitStr(PATH, ':');
          for (const std::string path : PATHS) {
            for (const auto &entry :
                 std::filesystem::directory_iterator(path)) {
              if (!entry.is_directory()) {
                std::string filepath = entry.path().string();
                std::filesystem::perms p =
                    std::filesystem::status(filepath).permissions();
                if (std::filesystem::perms::none !=
                    ((std::filesystem::perms::owner_exec |
                      std::filesystem ::perms ::group_exec |
                      std::filesystem::perms::others_exec) &
                     p)) {
                  std::string filename = filepath.substr(path.size() + 1);
                  autocompletion.push_back(filename);
                }
              }
            }
          }

          std::string trimed = trimStr(input);
          if (matching.empty()) {
            for (std::string str : autocompletion) {
              if (str.starts_with(trimed)) {
                matching.push_back(str);
              }
            }
            sort(matching.begin(), matching.end());
          } else if (matching.size() > 1) {
            write(STDOUT_FILENO, "\r\n", 2);
            for (std::string match : matching) {
              write(STDOUT_FILENO, match.c_str(), match.size());
              write(STDOUT_FILENO, "  ", 2);
            }
            write(STDOUT_FILENO, "\r\n$ ", 4);
            write(STDOUT_FILENO, input.c_str(), input.size());
          } else if (matching.size() == 1) {
            int pos = trimed.size();
            for (int i = pos; i < matching[0].size(); i++) {
              input += matching[0][i];
              write(STDOUT_FILENO, &matching[0][i], 1);
            }
            input += ' ';
            write(STDOUT_FILENO, " ", 1);
          }

          if (matching.empty())
            write(STDOUT_FILENO, "\x07", 1);
        }
      } else {
        input += c;
        write(STDOUT_FILENO, &c, 1);
      }
    }
    disableRawMode();

    // Eval: Parse and execute the command
    std::string trimed_input = trimStr(input);
    Shell::argv = splitStr(trimed_input, ' ');

    CMDS condition = builtin.find(Shell::argv[0]) != builtin.end()
                         ? builtin[Shell::argv[0]]
                         : CMDS::CmdOTHER;
    switch (condition) {
    case CMDS::CmdEXIT:
      exitBuiltin();
      break;

    case CMDS::CmdECHO:
      echoBuiltin();
      break;

    case CMDS::CmdTYPE:
      typeBuiltin();
      break;

    case CMDS::CmdPWD:
      pwdBuiltin();
      break;

    case CMDS::CmdCD:
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
            if (Shell::argv[i] == ">" || Shell::argv[i] == "1>") {
              sub_argv[i] = nullptr;
              int oldfd = open(Shell::argv[i + 1].c_str(),
                               O_WRONLY | O_CREAT | O_TRUNC, 0644);
              dup2(oldfd, 1);
              break;
            } else if (Shell::argv[i] == "2>") {
              sub_argv[i] = nullptr;
              int oldfd = open(Shell::argv[i + 1].c_str(),
                               O_WRONLY | O_CREAT | O_TRUNC, 0644);
              dup2(oldfd, STDERR_FILENO);
              break;
            } else if (Shell::argv[i] == ">>" || Shell::argv[i] == "1>>") {
              sub_argv[i] = nullptr;
              int oldfd = open(Shell::argv[i + 1].c_str(),
                               O_WRONLY | O_APPEND | O_CREAT, 0644);
              dup2(oldfd, STDOUT_FILENO);
              break;
            } else if (Shell::argv[i] == "2>>") {
              sub_argv[i] = nullptr;
              int oldfd = open(Shell::argv[i + 1].c_str(),
                               O_WRONLY | O_APPEND | O_CREAT, 0644);
              dup2(oldfd, STDERR_FILENO);
              break;
            } else {
              sub_argv[i] = const_cast<char *>(Shell::argv[i].c_str());
            }
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

// Finite state machine
enum class State { BLANK, BASIC, SQUOTE, DQUOTE, BACKSLASH, DQBACKSLASH };

std::vector<std::string> splitStr(std::string &str, char delimiter) {
  std::vector<std::string> res;
  std::string buffer;
  State current_state = State::BLANK;

  for (const char &c : str) {
    switch (current_state) {
    case State::BLANK:
      if (c == delimiter) {
        if (!buffer.empty()) {
          res.push_back(buffer);
          buffer.clear();
        }
        current_state = State::BLANK;
      } else if (c == '\'') {
        current_state = State::SQUOTE;
      } else if (c == '\"') {
        current_state = State::DQUOTE;
      } else if (c == '\\') {
        current_state = State::BACKSLASH;
      } else {
        current_state = State::BASIC;
        buffer += c;
      }
      break;

    case State::BASIC:
      if (c == delimiter) {
        res.push_back(buffer);
        buffer.clear();
        current_state = State::BLANK;
      } else if (c == '\'') {
        current_state = State::SQUOTE;
      } else if (c == '\"') {
        current_state = State::DQUOTE;
      } else if (c == '\\') {
        current_state = State::BACKSLASH;
      } else {
        buffer += c;
      }
      break;

    case State::SQUOTE:
      if (c == '\'') {
        current_state = State::BLANK;
      } else {
        buffer += c;
      }
      break;

    case State::DQUOTE:
      if (c == '\"') {
        current_state = State::BLANK;
      } else if (c == '\\') {
        current_state = State::DQBACKSLASH;
      } else {
        buffer += c;
      }
      break;

    case State::BACKSLASH:
      buffer += c;
      current_state = State::BLANK;
      break;

    case State::DQBACKSLASH:
      buffer += c;
      current_state = State::DQUOTE;
      break;

    default:
      break;
    }
  }

  if (!buffer.empty()) {
    res.push_back(buffer);
  }

  return res;
}

void exitBuiltin() { exit(0); }

void echoBuiltin() {
  // Redirect stdout
  std::string text;
  for (int i = 1; i < Shell::argv.size(); ++i) {
    if (Shell::argv[i] == ">" || Shell::argv[i] == "1>") {
      int stdout_backup = dup(1);
      int oldfd =
          open(Shell::argv[i + 1].c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
      dup2(oldfd, 1);
      std::cout << text << '\n';
      dup2(stdout_backup, 1);
      close(oldfd);
      return;
    } else if (Shell::argv[i] == "2>") {
      int stdout_backup = dup(STDERR_FILENO);
      int oldfd =
          open(Shell::argv[i + 1].c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
      dup2(oldfd, STDERR_FILENO);
      std::cout << text << '\n';
      dup2(stdout_backup, STDERR_FILENO);
      close(oldfd);
      return;
    } else if (Shell::argv[i] == ">>" || Shell::argv[i] == "1>>") {
      int stdout_backup = dup(STDOUT_FILENO);
      int oldfd =
          open(Shell::argv[i + 1].c_str(), O_WRONLY | O_APPEND | O_CREAT, 0644);
      dup2(oldfd, STDOUT_FILENO);
      std::cout << text << '\n';
      dup2(stdout_backup, STDOUT_FILENO);
      close(oldfd);
      return;
    } else if (Shell::argv[i] == "2>>") {
      int stdout_backup = dup(STDERR_FILENO);
      int oldfd =
          open(Shell::argv[i + 1].c_str(), O_WRONLY | O_APPEND | O_CREAT, 0644);
      dup2(oldfd, STDERR_FILENO);
      std::cout << text << '\n';
      dup2(stdout_backup, STDERR_FILENO);
      close(oldfd);
      return;

    } else {
      text += Shell::argv[i];
      text += " ";
    }
  }
  std::cout << text << '\n';
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
  std::filesystem::path target_dir(
      Shell::argv[1] == "~" ? std::string(getenv("HOME")) : Shell::argv[1]);
  if (std::filesystem::is_directory(target_dir)) {
    std::filesystem::current_path(target_dir);
  } else {
    std::cout << "cd: " << Shell::argv[1] << ": No such file or directory"
              << '\n';
  }
}
