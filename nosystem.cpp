#include "nosystem.h"

#include <map>
#include <stack>
#include <string>
#include <thread>
#include <vector>

static std::map<std::string, NoSystemCommand*> commands;
static std::stack<std::thread> cmd_threads;

__thread FILE* nosystem_stdin;
__thread FILE* nosystem_stdout;
__thread FILE* nosystem_stderr;

class nosystem_exit_exception : public std::exception {};

void nosystem_exit(int n) {
    throw nosystem_exit_exception{};
}

int nosystem_system(const char* cmd) {
    if (!cmd)
        return -1;

    std::vector<std::string> cmd_parts;
    std::string tmp_part;

    std::string cmd_as_std(cmd);
    for (auto it = cmd_as_std.begin(); it != cmd_as_std.end(); it++) {
        if (*it == '\"') {
            ++it;
            while (*it != '\"') {
                tmp_part += *it;
            }
        } else if (*it == ' ') {
            cmd_parts.push_back(tmp_part);
            tmp_part = "";
        } else {
            tmp_part += *it;
        }
    }
    if (tmp_part.size() != 0)
        cmd_parts.push_back(tmp_part);

    if (cmd_parts.size() == 0)
        return -1;

    auto fit = commands.find(cmd_parts[0]);
    if (fit == commands.end())
        return -1;

    std::vector<char*> args;
    for (auto& arg : cmd_parts) {
        args.push_back(arg.data());
    }

    struct RunThreadState {
        RunThreadState () {}
        int exit_code;
    };

    RunThreadState state;
    std::thread t = std::thread ([&state, &fit, &args](){
        try {
            state.exit_code = fit->second(args.size(), args.data());
        } catch (const nosystem_exit_exception&) {
            state.exit_code = 255;
        }
    });
    t.join();

    return state.exit_code;
}
