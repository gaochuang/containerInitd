#include "forkProcesAndExec.hpp"
#include "originalouterr.hpp"
#include "childsocket.hpp"
#include <sys/socket.h>
#include "childsocket.hpp"

#include <unistd.h>
#include <cerrno>
#include <csignal>
#include <cstdarg>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <sys/prctl.h>
#include <sys/stat.h>

#include <sys/types.h>
#include <syslog.h>

using namespace containerInitd;

[[noreturn]]
void logAndExit(const char* format, ...) noexcept
{
    va_list ap;
    va_start(ap, format);
    vfprintf(stderr, format, ap);
    va_end(ap);
    _exit(EXIT_FAILURE);
}

void setupKillSwitch(pid_t parentPid) noexcept
{
    //设置如果当前进程的父进程退出，就给当前进程发送 SIGKILL
    if(prctl(PR_SET_PDEATHSIG, SIGKILL))
    {
        logAndExit("prctl(PR_SET_PDEATHSIG, SIGKILL) failed %s \n", strerror(errno));
    }
 
    // PR_SET_PDEATHSIG 并不能保证父进程仍然活着，在设置完后检查父进程是否还存活
    //防止竞态（race condition），确保设置 PR_SET_PDEATHSIG 后，父进程还没有退出
    if(parentPid != getppid())
    {
        logAndExit("parent process already dead \n");
    }
}

[[noreturn]]
void child(pid_t parentPid, int stdout, int stderr, const std::vector<std::string>& argv, 
           const std::vector<std::string>& env, const std::optional<UidAndGid>& UidAndGid, int controlSocket) noexcept
{
    setupKillSwitch(parentPid);

    //改变当前进程的进程组，当前进程脱离父进程所在的进程组，不受父进程信号影响
    if(setpgrp() < 0)
    {
        logAndExit("setpgrp failed: %s \n", strerror(errno));
    }

    dup2(STDOUT_FILENO, ORIGINAL_STDOUT_FILENO);
    dup2(STDERR_FILENO, ORIGINAL_STDERR_FILENO);

    int devnull = open("/dev/null", O_RDONLY);
    if(-1 == devnull)
    {
        logAndExit("open(\"/dev/null\"): %s\n", strerror(errno));
    }

    dup2(devnull, STDIN_FILENO);
    close(devnull);

    if(stdout != STDOUT_FILENO)
    {
        dup2(stdout, STDOUT_FILENO);
        close(stdout);
    }

    if(stderr != STDERR_FILENO)
    {
        dup2(stderr, STDERR_FILENO);
        close(stderr);
    }

    if(-1 != controlSocket)
    {
        dup2(controlSocket, CHILDSOCKET_FILENO);
        close(controlSocket);
    }

    std::vector<const char*> c_argv;
    for(const auto& arg : argv)
    {
        c_argv.push_back(arg.c_str());
    }

    c_argv.push_back(nullptr);

    std::vector<const char*> c_env;
    for(const auto& env : env)
    {
        c_env.push_back(env.c_str());
    }
    c_env.push_back(nullptr);

    execve(c_argv[0], (char* const*)c_argv.data(), (char* const*)c_env.data());

    logAndExit("execve(\"%s\"): %s\n", c_argv[0], strerror(errno));

}

pid_t containerInitd::forkProcessAndExec(int stdout, int stderr, const std::vector<std::string>& argv, const std::vector<std::string>& env,
                        const std::optional<UidAndGid>& uidAndGid)
{
    int pid = getpid();
    int ret = fork();
    //子进程
    if(ret == 0)
    {
        child(pid, stdout, stderr, argv, env, uidAndGid, -1);
    }

    return ret;

}

std::pair<pid_t, int> containerInitd::forkProcessAndExecWithSocket(int stdout, int stderr, const std::vector<std::string>& argv,
                                                    const std::vector<std::string>& env, const std::optional<UidAndGid>& uidAndGid)
{
    int sv[2];
    int ret = socketpair(PF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, sv);
    if( ret < 0)
    {
        logAndExit("socketpair failed: %s \n", strerror(errno));
    }

    int myPid = getpid();
    int pid = fork();
    if(0 == pid)
    {
        child(myPid, stdout, stderr, argv, env, uidAndGid,sv[1]);
    }

    close(sv[1]);

    return std::make_pair(pid, sv[0]);
}