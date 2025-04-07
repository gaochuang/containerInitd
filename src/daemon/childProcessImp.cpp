#include "childProcessImp.hpp"
#include "forkProcesAndExec.hpp"

#include <iostream>

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
        }

    }

    setProcessState(state);
    
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

ProcessChildImp::~ProcessChildImp()
{

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

}

void ProcessChildImp::startTerminating()
{

}

void ProcessChildImp::startTerminating(int sig)
{

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
