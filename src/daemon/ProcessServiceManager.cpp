#include "ProcessServiceManager.hpp"
#include "processExitStatus.hpp"

#include <sys/wait.h>
#include <boost/optional.hpp>
#include <iostream>
#include <algorithm>

using namespace containerInitd;


struct ProcessServiceManager::ServiceAndProcess
{
    ProcessService service;
    std::shared_ptr<ChildProcess> childProcess;
    std::shared_ptr<SteadyTimer> restartTimer;
    boost::optional<std::chrono::steady_clock::duration> runningTimestamp;
};

namespace 
{
    //移除服务名为 name 的依赖，并返回所有因此变为“可启动”的服务
    std::vector<ProcessService> pruneDependencies(const std::string& name, ServiceMap& serviceMap, const Configure& configure)
    {
        std::vector<ProcessService> retService;

        for(auto it = serviceMap.begin(); it != serviceMap.end();)
        {
            std::string serviceName = it->first;
            std::vector<std::string>& serviceNameVector = it->second;
            for(auto itVector = serviceNameVector.begin(); itVector != serviceNameVector.end(); )
            {
                if(*itVector == name)
                {
                    itVector = serviceNameVector.erase(itVector);
                }else
                {
                    itVector++;
                }
            }

            if(serviceNameVector.empty())
            {
                auto itFind = configure.servicesMap.find(serviceName);
                if(itFind != configure.servicesMap.end())
                {
                    retService.push_back(itFind->second);
                }else
                {
                    std::cout << "can't find service in configuration" << std::endl;
                }

                serviceMap.erase(it++);
            }else
            {
                it++;
            }
        }

        return retService;
    }

    std::string getProcessManagerStateString(ProcessServiceManager::State state)
    {
        switch (state)
        {
        case ProcessServiceManager::State::STARTING:
            return "STARTING";
        case ProcessServiceManager::State::RUNNING:
            return "RUNNING";
        case ProcessServiceManager::State::TERMINARED:
            return "TERMINARED";
        case ProcessServiceManager::State::TERMINATING:
            return "TERMNINATING";
        }
        return "UNKOWN";
    }

    std::string getChildProcessStateString(ChildProcess::ProcessState state)
    {
        switch (state)
        {
            case ChildProcess::ProcessState::STARTING:
                return "STARTING";
            case ChildProcess::ProcessState::RUNNING:
                return "RUNNING";
            case ChildProcess::ProcessState::TERMINATING:
                return "TERMINATING";
            case ChildProcess::ProcessState::KILLING:
                return "KILLING";
            case ChildProcess::ProcessState::DEAD:
                return "DEAD";
            case ChildProcess::ProcessState::EXPECTDEAD:
                return "EXPECTDEAD";
        }
        return "UNKNOWN";
    }

    std::string getTerminateSourceString(ProcessServiceManager::TerminateSource terminateSource)
    {
        switch (terminateSource)
        {
            case ProcessServiceManager::TerminateSource::TERMINAL_CB:
                return "TERMINAL_CB";
            case ProcessServiceManager::TerminateSource::KILL_ALL:
                return "KILL_ALL";
        }
        return "UNKNOWN";
    }
}

ProcessServiceManager::ProcessServiceManager(boost::asio::io_context& io, const NotifyStartupReadyCb& notifyStartupReadyCb,
                 const NotifyTerminationReadyCb& notifyTerminationReadyCb,
                 const ChildProcessCreator& childProcessCreator, const Configure& configure)
                 :ioc(io),
                 signalSet(std::make_shared<boost::asio::signal_set>(ioc)),
                 notifyStartupReadyCb(notifyStartupReadyCb),
                 notifyTerminationReadyCb(notifyTerminationReadyCb),
                 childProcessCreator(childProcessCreator),
                 state(State::TERMINATING),
                 configure(configure),
                 terminateSource(TerminateSource::TERMINAL_CB),
                 hasNotifiedStartupReady(false)
{
    signalSet->add(SIGCHLD); //用SIGCHLD信号完成对子进程的回收可以避免父进程阻塞等待而不能执行其他操作，只有当父进程收到SIGCHLD信号之后才去调用信号捕捉函数完成对子进程的回收
    asyncWaitSignal();
}

ProcessServiceManager::~ProcessServiceManager()
{

}

void ProcessServiceManager::start(const ServiceDependencyGraph& dependencyGraph)
{
    if(state == State::STARTING || state == State::RUNNING)
    {
        return;
    }

    state = State::STARTING;

    this->dependencyGraph = dependencyGraph;

    for(const auto& i : configure.servicesMap)
    {
        //如果进程没有启动依赖，启动该进程
        if(i.second.startAfter.empty())
        {
           map.insert(std::make_pair(i.first, ServiceAndProcess({i.second, newChildProcess(i.second)}))); 
        }
    }

    if(configure.servicesMap.empty())
    {
        this->state = State::RUNNING;
        notifyStartupReady();
    }
}

void ProcessServiceManager::notifyStartupReady()
{
    if(!hasNotifiedStartupReady)
    {
        hasNotifiedStartupReady = true;
        notifyStartupReadyCb();
    }
}

ProcessServiceManager::State ProcessServiceManager::getState() const
{
    return state;
}

ProcessServiceManager::TerminateSource ProcessServiceManager::getTerminateSource() const
{
    return terminateSource;
}

ProcessServiceManager::ServiceAndProcess& ProcessServiceManager::getServiceAndProcess(const std::string& name)
{
    const auto i = map.find(name);
    if(i == map.end())
    {
        std::cout << "service map corruption" << std::endl;
        abort();
    }

    return i->second;
}

void ProcessServiceManager::terminateDependencyServices(const std::string& name)
{
    std::vector<ProcessService> services = pruneDependencies(name, dependencyGraph.terminalMap, configure);

    for(auto service : services)
    {
        auto& serviceAndProcess = getServiceAndProcess(service.name);
        if(serviceAndProcess.childProcess != nullptr)
        {
            if(ChildProcess::ProcessState::DEAD == serviceAndProcess.childProcess->getProcessState())
            {
                ioc.post([this, name = service.name](){
                    this->terminateDependencyServices(name);
                });
            }else
            {
                serviceAndProcess.childProcess->startTerminating();
            }
        }else
        {
            serviceAndProcess.restartTimer.reset();
            ioc.post([this, name = service.name](){
                this->terminateDependencyServices(name);
            });       
        }
    }
}

void ProcessServiceManager::startTerminating()
{
    if(!checkAndSetTerminatingState(TerminateSource::TERMINAL_CB))
    {
        return;
    }

    int restartingProcessNum = 0;

    if(dependencyGraph.terminalMap.empty())
    {
        for(auto& i : map)
        {
            if(i.second.restartTimer != nullptr)
            {
                i.second.restartTimer.reset();
            }

            if(i.second.childProcess != nullptr)
            {
                i.second.childProcess->startTerminating();
            }else
            {
                restartingProcessNum++;
            }
        }
    }else
    {
        for(auto& i : map)
        {
            if(i.second.restartTimer != nullptr)
            {
                i.second.restartTimer.reset();
            }
            ////这个服务进程退出没有依赖
            if(dependencyGraph.terminalMap.find(i.first) == dependencyGraph.terminalMap.end())
            {
                if(i.second.childProcess != nullptr)
                {
                    i.second.childProcess->startTerminating();
                }else
                {
                    restartingProcessNum++;
                    terminateDependencyServices(i.second.service.name);
                }
            }
        }
    }

    if(map.empty() || map.size() == restartingProcessNum)
    {
        state = State::TERMINARED;
        ioc.post(notifyTerminationReadyCb);
    }
}

void ProcessServiceManager::forceTerminateAndNotifyTerminationReadyCb()
{
    if(state == State::TERMINARED)
    {
        return;
    }

    state = State::TERMINARED;
    map.clear();
    ioc.post(notifyTerminationReadyCb);
}

bool ProcessServiceManager::checkAndSetTerminatingState(TerminateSource terminateSource)
{
    if(state == State::STARTING)
    {
        std::cout << "erminating all processes forcefully" << std::endl;
        forceTerminateAndNotifyTerminationReadyCb();
        return false;
    }

    if(state == State::TERMINATING)
    {
        std::cout << "containerInitd is already terminating " << std::endl;
        return false;
    }

    this->terminateSource = terminateSource;
    return true;
}

std::ostream& operator<<(std::ostream& os, ProcessServiceManager::State state)
{
    os << getProcessManagerStateString(state);
    return os;
}

std::ostream& operator<<(std::ostream& os, ChildProcess::ProcessState state)
{
    os << getChildProcessStateString(state);
    return os;
}

std::ostream& operator<<(std::ostream& os, ProcessServiceManager::TerminateSource terminateSource)
{
    os << getTerminateSourceString(terminateSource);
    return os;
}

std::shared_ptr<ChildProcess> ProcessServiceManager::newChildProcess(const ProcessService& svc)
{
    return childProcessCreator(svc, [this, name = svc.name](auto state){
        stateChangeCb(name, state);
    });
}

void ProcessServiceManager::rememberWhenChildProcessBecameReady(const std::string& name)
{
    getServiceAndProcess(name).runningTimestamp = std::chrono::steady_clock::now().time_since_epoch();
}

bool ProcessServiceManager::checkAllChildProcessesIsRunning()
{
    if(!dependencyGraph.startupMap.empty())
    {
        return false;
    }

    //检查所有的进程是不是都在RUNNING
    for(const auto& i : map)
    {
        if(i.second.childProcess == nullptr || i.second.childProcess->getProcessState() != ChildProcess::ProcessState::RUNNING)
        {
            return false;
        }
    }
    return true;
}

void ProcessServiceManager::resetServiceAndProcess(ServiceAndProcess& serviceAndProcess)
{
    serviceAndProcess.childProcess.reset();
    serviceAndProcess.restartTimer.reset();
}

void ProcessServiceManager::unexpectedChildProcessDeath(const std::string& name)
{
    auto& serviceAndProcess = getServiceAndProcess(name);
    resetServiceAndProcess(serviceAndProcess);
    if(serviceAndProcess.service.action == ProcessService::FailureAction::REBOOT_CONTAINER)
    {
       std::cerr << "critical " << name << " has terminated, exiting " << std::endl;
       terminateWithCleanup();
    }else if(!serviceAndProcess.runningTimestamp)
    {
        std::cerr << name << " has terminated before ready, exiting" << std::endl;
        terminateWithCleanup();
    }else if(std::chrono::steady_clock::now().time_since_epoch() - *serviceAndProcess.runningTimestamp <  std::chrono::minutes(1)) //某个进程1min内重启，整个container退出
    {
        std::cerr << "has terminated before ready more than one minute, exiting " << std::endl;
        terminateWithCleanup();
    }else
    {
        serviceAndProcess.runningTimestamp = boost::none;
        serviceAndProcess.restartTimer = std::make_shared<SteadyTimer>(ioc);
        serviceAndProcess.restartTimer->expires_from_now(std::chrono::milliseconds(300));
        serviceAndProcess.restartTimer->async_wait([this, name](const boost::system::error_code& ec){
            this->restartTimeoutHandler(ec, name);
        });
    }
}

void ProcessServiceManager::unexpectedChildProcessDeathDuringTermination(const std::string& name)
{
    auto& serviceAndProcess = getServiceAndProcess(name);
    if(ProcessService::FailureAction::REBOOT_CONTAINER == serviceAndProcess.service.action)
    {
        std::cerr << " critical " << name << " has terminated, exiting " << std::endl;
        terminateWithCleanup();
    }else
    {
        terminateDependencyServices(name);
    }
}

void ProcessServiceManager::terminateWithCleanup()
{
    if(state == State::TERMINARED)
    {
        return;
    }

    state == State::TERMINARED;

    ioc.post([this](){
        map.clear();
        exit(EXIT_FAILURE);
    });
}

void ProcessServiceManager::restartTimeoutHandler(const boost::system::error_code& error, const std::string& name)
{
    if(error)
    {
        return;
    }

    auto& serviceAndProcess  = getServiceAndProcess(name);

    serviceAndProcess.childProcess = newChildProcess(serviceAndProcess.service);
}

bool ProcessServiceManager::areAllChildProcessesDead() const
{
    for(const auto& i : map)
    {
        if(i.second.childProcess != nullptr)
        {
            auto state = i.second.childProcess->getProcessState();
            if((state != ChildProcess::ProcessState::DEAD)&&(state != ChildProcess::ProcessState::EXPECTDEAD))
            {
                return false;
            }
        }
    }
    return true;
}

void ProcessServiceManager::stateChangeCb(const std::string& name, ChildProcess::ProcessState state)
{
    std::cout << "process manager state: " << this->state << " service name " << name << " state " << state
        << "terminateSource: " << terminateSource << std::endl;
    
    switch (this->state)
    {
    case State::STARTING:
        if(state == ChildProcess::ProcessState::RUNNING)
        {
            //记录进程启动时间戳
            rememberWhenChildProcessBecameReady(name);

            if(checkAllChildProcessesIsRunning())
            {
                this->state = State::RUNNING;
                notifyStartupReady();
            }else
            {
                //获取可以启动的服务列表 runnableServices
                std::vector<ProcessService> runnableServices = pruneDependencies(name, dependencyGraph.startupMap, configure);
                for(auto service : runnableServices)
                {
                    map.insert(std::make_pair(service.name, ServiceAndProcess({service, newChildProcess(service)})));
                }
            }
        }else if (state == ChildProcess::ProcessState::DEAD)
        {
            unexpectedChildProcessDeath(name);
        }
        break;
    case State::RUNNING:
         if(ChildProcess::ProcessState::DEAD == state)
         {
            unexpectedChildProcessDeath(name);
         }else if(ChildProcess::ProcessState::RUNNING == state)
         {
            rememberWhenChildProcessBecameReady(name);
         }
        break;
    case State::TERMINATING:
        if(ChildProcess::ProcessState::DEAD == state || ChildProcess::ProcessState::EXPECTDEAD == state)
        {
            auto& serviceAndProcess = getServiceAndProcess(name);
            resetServiceAndProcess(serviceAndProcess);
            if(areAllChildProcessesDead())
            {
                this->state = State::TERMINARED;
                std::cout << "all process terminated " <<std::endl;
                notifyTerminationReadyCb();
                break;
            }
        }
        if(terminateSource == TerminateSource::TERMINAL_CB)
        {
            //
            if(state == ChildProcess::ProcessState::EXPECTDEAD)
            {
                terminateDependencyServices(name);
            }else if(state == ChildProcess::ProcessState::DEAD)
            {
                unexpectedChildProcessDeathDuringTermination(name);
            }
        }
        break;
    case State::TERMINARED:
        break;
    }
}

void ProcessServiceManager::handlePid(int pid, int exitStatus)
{
    for(const auto& i : map)
    {
        if(pid == i.second.childProcess->getProcessPid())
        {
            i.second.childProcess->processTerminated(exitStatus);
            return ;
        }
    }

    std::cout << "detected termination of unkown pid " << pid << ", terminated with " << getProcessExitDescription(exitStatus) << std::endl;
}

void ProcessServiceManager::signalHandler()
{
    while (true)
    {
        int exitStatus;
        auto pid = waitpid(-1, &exitStatus, WNOHANG); //* Don't block waiting.  */
        if(pid <= 0)
        {
            break;
        }

        handlePid(pid, exitStatus);
    }

    asyncWaitSignal();
}

void ProcessServiceManager::asyncWaitSignal()
{
    signalSet->async_wait([this](const boost::system::error_code& ec, int signal_number){
        this->signalHandler();
    });
}
