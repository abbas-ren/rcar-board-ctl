#include "process.hpp"
#include "util.hpp"

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

namespace boardctl {

int Process::run(const std::string& path, const std::vector<std::string>& args) {
    if (path.empty()) {
        throw Error("Process path is empty");
    }

    std::vector<char*> argv;
    argv.reserve(args.size() + 2);

    argv.push_back(const_cast<char*>(path.c_str()));

    for (const auto& arg : args) {
        argv.push_back(const_cast<char*>(arg.c_str()));
    }

    argv.push_back(nullptr);

    pid_t pid = fork();

    if (pid < 0) {
        throw Error("fork failed");
    }

    if (pid == 0) {
        execvp(path.c_str(), argv.data());
        _exit(127);
    }

    int status = 0;

    if (waitpid(pid, &status, 0) < 0) {
        throw Error("waitpid failed");
    }

    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }

    if (WIFSIGNALED(status)) {
        return 128 + WTERMSIG(status);
    }

    return 1;
}

}