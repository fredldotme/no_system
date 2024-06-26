#include "nosystem.h"

#include <atomic>
#include <iostream>
#include <map>
#include <stack>
#include <string>
#include <thread>
#include <vector>

#include <unistd.h>

#ifndef __wasi__
#include <dlfcn.h>
#include <mach-o/dyld.h>
#include <yaml-cpp/yaml.h>
#endif

extern "C" {

struct RunThreadState {
    std::thread execution_thread;
    pid_t pid;
    FILE* my_stdin;
    FILE* my_stdout;
    FILE* my_stderr;
    int exit_code;
};

class nosystem_exit_exception : public std::exception {
public:
    nosystem_exit_exception(int code) : exit_code(code) {}
    int exit_code;
};

struct DynCommand {
    std::string library;
    std::string entrypoint;
};

struct ResolvedDynCommand {
    NoSystemCommand* entrypoint;
    void* handle;
};

static std::map<std::string, NoSystemCommand*> commands;
static std::map<std::string, DynCommand> dycommands;

static std::map<int, RunThreadState&> command_threads;
static std::atomic<pid_t> recent_pid{0};

// Thread-local so that every "process" can have distinct stdio
__thread FILE* nosystem_stdin;
__thread FILE* nosystem_stdout;
__thread FILE* nosystem_stderr;

int nosystem_init()
{
#ifndef __wasi__
    std::string currExe;
    char buf[PATH_MAX];
    uint32_t bufsize = PATH_MAX;
    if(!_NSGetExecutablePath(buf, &bufsize))
        currExe = std::string(buf);

    if (currExe.empty())
        return 0;

    std::string currPath = "/";
    std::string delimiter = "/";
    std::string s = currExe;

    size_t pos = 0;
    std::string token;
    while ((pos = s.find(delimiter)) != std::string::npos) {
        token = s.substr(0, pos);
        currPath += (token + "/");
        s.erase(0, pos + delimiter.length());
    }

    const auto yamlPath = currPath + "commands.yaml";
    const auto fwPath = currPath + "Frameworks/";

    if (getenv("NOSYSTEM_DEBUG")) {
        std::cout << "Loading commands from: " << yamlPath;
    }

    YAML::Node rootNode = YAML::LoadFile(yamlPath);
    YAML::Node commandsNode = rootNode["commands"];
    for (int i = 0; i < commandsNode.size(); i++) {
        DynCommand d;
        const auto currentNode = commandsNode[i];
        const auto commandName = currentNode["command"].as<std::string>();
        const auto fw = currentNode["framework"].as<std::string>();
        d.entrypoint = currentNode["entrypoint"].as<std::string>();
        d.library = fwPath + fw + ".framework/" + fw;

        std::cout << "Adding command " << commandName << " from library " << d.library << " (entrypoint '" << d.entrypoint << "')" << std::endl;
        dycommands.insert({ commandName, d });
    }
#endif

    return 1;
}

static ResolvedDynCommand nosystem_resolvemain(const DynCommand& lib) {
    ResolvedDynCommand ret { nullptr, nullptr };

#ifndef __wasi__
    auto handle = dlopen(lib.library.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!handle)
        return ret;

    *(void**)(&ret.entrypoint) = dlsym(handle, lib.entrypoint.c_str());
    ret.handle = handle;
#endif
    return ret;
}

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

    return 0;
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

    for (const auto& dycommand : dycommands) {
        if (cmd_parts[0].find(dycommand.first) != std::string::npos) {
            return 1;
        }
    }
    return 0;
}

int nosystem_isatty(int fd) {
    return 0;
}

void nosystem_exit(int n) {
#ifndef __wasi__
    throw nosystem_exit_exception(n);
#endif
}


int nosystem_execv(const char *pathname, char *const argv[]) {
    return nosystem_execve(pathname, argv, nullptr);
}

int nosystem_execvp(const char *pathname, char *const argv[]) {
    return nosystem_execve(pathname, argv, nullptr);
}

int nosystem_execve(const char *pathname, char *const argv[], char *const envp[]) {
    std::string pathname_as_std(pathname);
    ResolvedDynCommand resolved { nullptr, nullptr };
    NoSystemCommand* command = nullptr;

    // Find built-ins first
    auto fit = commands.find(pathname_as_std);
    if (fit != commands.end()) {
        command = fit->second;
    }

    // Check external commands afterwards
    if (!command) {
        auto dfit = dycommands.find(pathname_as_std);
        if (dfit != dycommands.end()) {
            resolved = nosystem_resolvemain(dfit->second);
            command = resolved.entrypoint;
        }
    }

    if (!command) {
        return -1;
    }

    int argc = 0;
    while (argv[argc++] != nullptr);

    int ret = 0;
#ifndef __wasi__
    try {
#endif
        ret = command(argc, (char**)argv);
#ifndef __wasi__
    } catch (const nosystem_exit_exception& e) {
        ret = e.exit_code;
    }
#endif
    return ret;
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

    ResolvedDynCommand resolved { nullptr, nullptr };
    NoSystemCommand* command = nullptr;

    // Find built-ins first
    auto fit = commands.find(cmd_parts[0]);
    if (fit != commands.end()) {
        command = fit->second;
    }

    // Check external commands afterwards
    if (!command) {
        auto dfit = dycommands.find(cmd_parts[0]);
        if (dfit != dycommands.end()) {
            resolved = nosystem_resolvemain(dfit->second);
            command = resolved.entrypoint;
        }
    }

    if (!command)
        return -1;

    std::vector<const char*> args;
    for (const auto& arg : cmd_parts) {
        args.push_back(arg.data());
    }

    RunThreadState state;
    state.pid = nosystem_fork();
    state.my_stdin = nosystem_stdin;
    state.my_stdout = nosystem_stdout;
    state.my_stderr = nosystem_stderr;
    state.exit_code = 0;

    state.execution_thread = std::thread ([&state, &command, &args](){
#ifndef __wasi__
        try {
#endif
            nosystem_stdin = state.my_stdin;
            nosystem_stdout = state.my_stdout;
            nosystem_stderr = state.my_stderr;
            state.exit_code = command(args.size(), (char**)args.data());
#ifndef __wasi__
        } catch (const nosystem_exit_exception& e) {
            state.exit_code = e.exit_code;
        }
#endif
    });

    command_threads.insert({state.pid, state});
    state.execution_thread.join();
    command_threads.erase(command_threads.find(state.pid));

#ifndef __wasi__
    if (resolved.handle) {
        dlclose(resolved.handle);
    }
#endif

    return state.exit_code;
}

}
