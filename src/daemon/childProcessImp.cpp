#include "childProcessImp.hpp"
#include "forkProcesAndExec.hpp"
#include "loggerPrefixBuilder.hpp"
#include "processExitStatus.hpp"

#include <iostream>
#include <sys/wait.h>

using namespace containerInitd;

ProcessChildImp::ProcessChildImp(boost::asio::io_context& ioc, const ProcessService& service, const StateChangeCallCB& stateChangeCb, 
                                const LoggerOutputForwarderCreator& loggetOutputCreator)
                :ioc(ioc),
                name(service.name),
                heartbeatTimeout(0),
                stopTimeout(0),
                watchdogTimeout(0),
                stateChangeCb(stateChangeCb),
                unackedHeartbeatCount(0),
                heartbeatThreshold(0),
                savedTerminationSignal(-1)

{
    ProcessState state = ProcessState::RUNNING;

    int childOutputFd = STDOUT_FILENO;
    int childOutErrFd = STDERR_FILENO;

    if(service.standardOut == ProcessService::LoggerOut::INIT_PROCESS)
    {
        outputForwarder = loggetOutputCreator(STDOUT_FILENO);
        childOutputFd = outputForwarder->getChildFd();
    }

    if(service.standardErr == ProcessService::LoggerOut::INIT_PROCESS)
    {
        errputForwarder = loggetOutputCreator(STDERR_FILENO);
        childOutErrFd = errputForwarder->getChildFd();
    }

    if((service.type == ProcessService::Type::NOTIFY) || (service.watchdogTimeout ) || (service.heartbeat))
    {

        auto pidAndFd = forkProcessAndExecWithSocket(childOutputFd, childOutErrFd, service.argv, service.env, service.uidAndGid);
        streamDescriptor = std::make_shared<StreamDescriptor>(ioc);
        streamDescriptor->assign(pidAndFd.second);
        asyncRead();
        
        //启动超时
        if(ProcessService::Type::NOTIFY == service.type)
        {
            startTimer = std::make_shared<SteadyTimer>(ioc);
            startTimer->expires_after(service.startTimeout);
            startTimer->async_wait([this](const boost::system::error_code& ec){
                this->startTimeoutHandler(ec);
            });

            state = ProcessState::STARTING;
        }

        //设置心跳检测
        if(service.heartbeat)
        {
            unackedHeartbeatCount = 0;
            //尝试次数
            heartbeatThreshold = service.heartbeat->failureThreshold;
            //超时时间
            heartbeatTimeout = service.heartbeat->interval;
            heartbeatTimer = std::make_shared<SteadyTimer>(ioc);
            startHeartBeat();
        }else if(service.watchdogTimeout)
        {
            watchdogTimeout = *service.watchdogTimeout;
            watchdogTimer = std::make_shared<SteadyTimer>(ioc);

            if(ProcessService::Type::NOTIFY != service.type)
            {
                startWatchdog();
            }
        }else
        {
            pid = forkProcessAndExec(childOutputFd, childOutErrFd, service.argv, service.env, service.uidAndGid);
        }

    }

    auto prefixStr = loggerPrefixBulder(service.outputPrefix, pid);

    if(outputForwarder)
    {
        outputForwarder->closeChildFd();
        outputForwarder->setLogPrefix(prefixStr);
    }

    if(errputForwarder)
    {
        errputForwarder->closeChildFd();
        errputForwarder->setLogPrefix(prefixStr);
    }

    setProcessState(state);

    std::cout << "name: " << name << "started with pid " << pid << std::endl;

}

ProcessChildImp::~ProcessChildImp()
{
    heartbeatTimer.reset();
    watchdogTimer.reset();
    startTimer.reset();
    stopTimer.reset();
    if(0 != pid)
    {
        //杀死整个进程组中的进程,ESRCH 进程或进程组不存在（可能已经提前退出
        if((-1 == kill(-pid, SIGKILL)) && (errno != ESRCH))
        {
            std::cerr << "kill " << -pid << " process failed " << strerror(errno) << std::endl;
        }else
        {
            std::cout << "kill " << -pid << " process successful" << std::endl;
        }

        waitpid(pid, nullptr, 0);
    }
}

void ProcessChildImp::startTimeoutHandler(const boost::system::error_code& ec)
{
    if(ec)
    {
        return;
    }
    std::cout << "process service:" << name << " pid: " << pid << " start up timeout, will kill " << std::endl;
    killProcess();

}


void ProcessChildImp::handleRead(const boost::system::error_code& ec)
{

}

void ProcessChildImp::asyncRead()
{
    streamDescriptor->async_read_some(boost::asio::null_buffers(), [this](const boost::system::error_code& ec, std::size_t){
        handleRead(ec);
    });
}

ChildProcess::ProcessState ProcessChildImp::getProcessState() const
{
    return state;
}

pid_t ProcessChildImp::getProcessPid() const
{
    return pid;
}

void ProcessChildImp::processTerminated(int exitStatus)
{
    heartbeatTimer.reset();
    startTimer.reset();
    stopTimer.reset();
    watchdogTimer.reset();

    auto pidCopy = pid;
    pid = 0;
    if((-1 == kill(-pidCopy, SIGKILL)) && (errno != ESRCH))
    {
        throw std::system_error(errno, std::system_category(), "kill");
    }

    if(outputForwarder)
    {
        outputForwarder->readRemaining();
    }

    if(errputForwarder)
    {
        errputForwarder->readRemaining();
    }

    auto sig = savedTerminationSignal;
    if(sig < 0)
    {
        sig = SIGTERM;
    }

    if((state == ProcessState::TERMINATING) && isSuccessExit(exitStatus, sig))
    {
        std::cout << name << " pid: " << pidCopy << "terminated successfully " << std::endl;
        setProcessState(ProcessState::EXPECTDEAD);
    }else
    {
        std::cerr << "detected unexpected  " << name << " terminalter , pid " << pidCopy
             << " terminated with " << getProcessExitDescription(exitStatus) << std::endl;
    }
}

void ProcessChildImp::startTerminating()
{
    startTerminating(SIGTERM);
}

void ProcessChildImp::startTerminating(int sig)
{
    startTimer.reset();

    std::cout << "terminating " << name << " pid " << pid << std::endl;
    kill(pid, sig);

    stopTimer  = std::make_shared<SteadyTimer>(ioc);
    stopTimer->expires_from_now(stopTimeout);
    setProcessState(ProcessState::TERMINATING);

    stopTimer->async_wait([this](const boost::system::error_code& ec){
        this->stopTimeoutHandler(ec);
    });

}

void ProcessChildImp::stopTimeoutHandler(const boost::system::error_code& ec)
{
    if(ec)
    {
        return;
    }

    std::cerr << "name: "  << name << " pid: " << pid << " terminating timeout, will kill by SIGKILL " << std::endl;

    kill(pid, SIGKILL);
    setProcessState(ProcessState::KILLING);
}

void ProcessChildImp::setProcessState(ProcessState state)
{
    this->state = state;
    ioc.post([this](){
        stateChangeCb(this->state);
    });
}

void ProcessChildImp::killProcess()
{
   kill(pid, SIGKILL);
   setProcessState(ProcessState::KILLING);
}

void ProcessChildImp::heartBeatTimeoutHandle(const boost::system::error_code& ec)
{
    if(ec)
    {
        return;
    }

    if(++unackedHeartbeatCount < heartbeatThreshold)
    {
        //sendHeartbeatRequest
        sendHeartBeatRequestMessage();
        heartbeatTimer->expires_from_now(heartbeatTimeout);
        heartbeatTimer->async_wait([this](const boost::system::error_code& ec){
            this->heartBeatTimeoutHandle(ec);
        });
    }else
    {
        std::cout << name << "  pid: " << pid << "heartbear timeout " << std::endl;
        killProcess();
    }
}

void ProcessChildImp::watchdogTimeoutHandle(const boost::system::error_code& ec)
{
    if(ec)
    {
        return;
    }

    std::cerr << name << " pid: " << pid << " watch dog timeout, killing with SIGABRT " << std::endl;
    
    killProcess();

}

void ProcessChildImp::startHeartBeat()
{
    heartbeatTimer->expires_from_now(heartbeatTimeout);
    heartbeatTimer->async_wait(
        [this](const boost::system::error_code& ec){
            this->heartBeatTimeoutHandle(ec);
        }
    );
}

void ProcessChildImp::startWatchdog()
{
   watchdogTimer->expires_from_now(watchdogTimeout);
   watchdogTimer->async_wait([this](const boost::system::error_code& ec){
       this->watchdogTimeoutHandle(ec);
   });
}

void ProcessChildImp::sendHeartBeatRequestMessage()
{

}
