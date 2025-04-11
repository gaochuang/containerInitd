#ifndef CHILD_PROCESS_HPP
#define CHILD_PROCESS_HPP

#include <functional>
#include <sys/types.h>

namespace containerInitd
{

class ChildProcess
{
public:

    enum class ProcessState
    {
        STARTING,
        RUNNING,
        TERMINATING,
        KILLING,
        DEAD,
        EXPECTDEAD,
    };

    using StateChangeCallCB = std::function<void(ProcessState)>;

    ChildProcess() = default;
    virtual ~ChildProcess() = default;

    ChildProcess(const ChildProcess&) = delete;
    ChildProcess(ChildProcess&&) = delete;
    ChildProcess operator =(const ChildProcess&) = delete;
    ChildProcess operator =(ChildProcess&&) = delete;

    virtual ProcessState getProcessState() const = 0;
    virtual pid_t getProcessPid() const = 0;
    virtual void processTerminated(int exitStatus) = 0;
    virtual void startTerminating() = 0;
    virtual void startTerminating(int sig) = 0;

};

}

#endif
