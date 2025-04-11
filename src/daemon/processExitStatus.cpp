#include "processExitStatus.hpp"

#include <sys/types.h>
#include <sys/wait.h>
#include <csignal>
#include <cstdlib>
#include <sstream>
#include <syslog.h>

using namespace containerInitd;

namespace
{
    inline bool isExitedSuccessfully(int status) 
    {
        return WIFEXITED(status) && (WEXITSTATUS(status) == EXIT_SUCCESS);
    }

    inline bool isTerminatedBySignal(int status, int sig)
    {
        return WIFSIGNALED(status) && (WTERMSIG(status) == sig);
    }
}

std::string containerInitd::getProcessExitDescription(int status)
{
    std::ostringstream os;

    if(WIFEXITED(status)) //判断进程是否是通过 exit() 正常退出的
    {
        os << "exit code " << WEXITSTATUS(status); //	获取子进程的返回码（exit(x) 里的 x）
    }else if(WIFSIGNALED(status)) //判断进程是否是被信号终止
    {
        os << "signal " << WTERMSIG(status); //获取终止进程的信号编号
        if(WCOREDUMP(status))
        {
            os << " core dumped";
        }
    }else
    {
        syslog(LOG_ERR, "containerInitd abort, file %s, line %d: %s\n", __FILE__, __LINE__, "invalid exit status");
        abort();
    }

    return os.str();
}

bool containerInitd::isSuccessExit(int status, int sig)
{
    return isExitedSuccessfully(status) || isTerminatedBySignal(status, sig);
}
