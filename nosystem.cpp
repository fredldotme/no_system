#include "nosystem.h"

#include <atomic>
#include <iostream>
#include <map>
#include <stack>
#include <string>
#include <thread>
#include <vector>

#include <unistd.h>

extern "C" {

struct RunThreadState {
    std::thread execution_thread;
    pid_t pid;
    FILE* stdin;
    FILE* stdout;
    FILE* stderr;
    int exit_code;
};

class nosystem_exit_exception : public std::exception {
public:
    nosystem_exit_exception(int code) : exit_code(code) {}
    int exit_code;
};

static std::map<std::string, NoSystemCommand*> commands;
static std::map<int, RunThreadState&> command_threads;
static std::atomic<pid_t> recent_pid{0};

// Thread-local so that every "process" can have distinct stdio
__thread FILE* nosystem_stdin;
__thread FILE* nosystem_stdout;
__thread FILE* nosystem_stderr;

void nosystem_addcommand(const char* cmd, NoSystemCommand* func) {
    if (!cmd || !func)
        return;

    std::string cmd_as_std(cmd);
    commands.insert({ cmd_as_std, func });
}

pid_t nosystem_fork() {
    return ++recent_pid;
}

pid_t nosystem_currentPid() {
    for (const auto& t : command_threads) {
        if (t.second.execution_thread.native_handle() == pthread_self()) {
            return t.first;
        }
    }

    return 1;
}

pid_t nosystem_waitpid(pid_t pid, int *status, int options)
{
    for (const auto& t : command_threads) {
        if (t.second.pid == pid) {
            t.second.execution_thread.join();
            return t.first;
        }
    }

    return -1;
}

static inline std::vector<std::string> __nosystem_split_command(const std::string& cmd)
{
    std::vector<std::string> cmd_parts;
    std::string tmp_part;

    std::string cmd_as_std(cmd);
    for (auto it = cmd_as_std.begin(); it != cmd_as_std.end(); it++) {
        if (*it == '\"') {
            while (++it != cmd_as_std.end() && *it != '\"') {
                tmp_part += *it;
            }
        } else if (*it == ' ') {
            if (tmp_part.size() != 0)
                cmd_parts.push_back(tmp_part);
            tmp_part = "";
        } else {
            tmp_part += *it;
        }
    }
    if (tmp_part.size() != 0)
        cmd_parts.push_back(tmp_part);

    return cmd_parts;
}

int nosystem_executable(const char* cmd) {
    if (!cmd)
        return 0;

    std::string cmd_as_std(cmd);

    if (getenv("NOSYSTEM_DEBUG")) {
        std::cout << "Command to be found: " << cmd_as_std << std::endl;
    }

    const std::vector<std::string> cmd_parts = __nosystem_split_command(cmd_as_std);

    if (cmd_parts.size() == 0)
        return 0;

    for (const auto& command : commands) {
        if (cmd_parts[0].find(command.first) != std::string::npos) {
            return 1;
        }
    }
    return 0;
}

int nosystem_isatty(int fd) {
    return 0;
}

void nosystem_exit(int n) {
    throw nosystem_exit_exception(n);
}


int nosystem_execv(const char *pathname, char *const argv[]) {
    return nosystem_execve(pathname, argv, nullptr);
}

int nosystem_execve(const char *pathname, char *const argv[], char *const envp[]) {
    std::string pathname_as_std(pathname);
    auto fit = commands.find(pathname_as_std);
    if (fit == commands.end())
        return -1;

    int argc = 0;
    while (argv[argc++] != nullptr);

    return fit->second(argc, (char**)argv);
}

int nosystem_system(const char* cmd) {
    if (!cmd)
        return -1;

    std::string cmd_as_std(cmd);

    if (getenv("NOSYSTEM_DEBUG")) {
        std::cout << "Command to be found: " << cmd_as_std << std::endl;
    }

    const std::vector<std::string> cmd_parts = __nosystem_split_command(cmd_as_std);

    if (cmd_parts.size() == 0)
        return -1;

    auto fit = commands.find(cmd_parts[0]);
    if (fit == commands.end())
        return -1;

    std::vector<const char*> args;
    for (const auto& arg : cmd_parts) {
        args.push_back(arg.data());
    }

    RunThreadState state;
    state.pid = nosystem_fork();
    state.stdin = nosystem_stdin;
    state.stdout = nosystem_stdout;
    state.stderr = nosystem_stderr;
    state.exit_code = 0;

    state.execution_thread = std::thread ([&state, &fit, &args](){
        try {
            nosystem_stdin = state.stdin;
            nosystem_stdout = state.stdout;
            nosystem_stderr = state.stderr;
            state.exit_code = fit->second(args.size(), (char**)args.data());
        } catch (const nosystem_exit_exception& e) {
            state.exit_code = e.exit_code;
        }
    });

    command_threads.insert({state.pid, state});
    state.execution_thread.join();
    command_threads.erase(command_threads.find(state.pid));

    return state.exit_code;
}

}
