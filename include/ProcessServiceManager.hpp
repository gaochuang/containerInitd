#ifndef PROCESS_SERVICE_MANAGER_HPP
#define PROCESS_SERVICE_MANAGER_HPP

#include "childProcess.hpp"
#include "processService.hpp"
#include "configure.hpp"
#include "processServiceDependency.hpp"

#include <memory>
#include <string>
#include <unordered_map>
#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/deadline_timer.hpp>
#include <boost/asio.hpp>

namespace containerInitd
{
    class ProcessServiceManager
    {
    public:
      enum class State
      {
          STARTING,
          RUNNING,
          TERMINATING,
          TERMINARED,
      };

      enum class TerminateSource
      {
          TERMINAL_CB,
          KILL_ALL,
      };
      
      using SteadyTimer = boost::asio::basic_waitable_timer<std::chrono::steady_clock>;
      using NotifyStartupReadyCb = std::function<void()>;

      using NotifyTerminationReadyCb = std::function<void()>;
      using ChildProcessCreator = std::function<std::shared_ptr<ChildProcess>(const ProcessService& svc, const ChildProcess::StateChangeCallCB& stateChangeCb)>;

      explicit ProcessServiceManager(boost::asio::io_context& io, const NotifyStartupReadyCb& notifyStartupReadyCb, 
                            const NotifyTerminationReadyCb& notifyTerminationReadyCb,
                            const ChildProcessCreator& childProcessCreator,
                            const Configure& configure);
    ~ProcessServiceManager();

    ProcessServiceManager(const ProcessServiceManager&) = delete;
    ProcessServiceManager(ProcessServiceManager&&) = delete;
    ProcessServiceManager& operator=(const ProcessServiceManager&) = delete;
    ProcessServiceManager& operator=(ProcessServiceManager&&) = delete;

    void start(const ServiceDependencyGraph& dependencyGraph);
    State getState() const;
    TerminateSource getTerminateSource() const;
    void startTerminating();
    void forceTerminateAndNotifyTerminationReadyCb();
    void notifyStartupReady();

    private:
      struct ServiceAndProcess;
      boost::asio::io_context& ioc;
      std::shared_ptr<boost::asio::signal_set> signalSet;
      NotifyStartupReadyCb notifyStartupReadyCb;
      NotifyTerminationReadyCb notifyTerminationReadyCb;
      ChildProcessCreator childProcessCreator;
      State state;
      Configure configure;
      TerminateSource terminateSource;
      bool hasNotifiedStartupReady;
      ServiceDependencyGraph dependencyGraph;
      std::unordered_map<std::string, ServiceAndProcess> map;

      std::shared_ptr<ChildProcess> newChildProcess(const ProcessService& svc);
      void signalHandler();
      void asyncWaitSignal();
      void handlePid(int pid, int exitStatus);
      void stateChangeCb(const std::string& name, ChildProcess::ProcessState state);

      bool checkAndSetTerminatingState(TerminateSource terminateSource);
      void terminateDependencyServices(const std::string& name);

      ServiceAndProcess& getServiceAndProcess(const std::string& name);
      void rememberWhenChildProcessBecameReady(const std::string& name);
      bool checkAllChildProcessesIsRunning();
      void resetServiceAndProcess(ServiceAndProcess& serviceAndProcess);
      void unexpectedChildProcessDeath(const std::string& name);
      void terminateWithCleanup();
      void restartTimeoutHandler(const boost::system::error_code& error, const std::string& name);
      bool areAllChildProcessesDead() const;
      void unexpectedChildProcessDeathDuringTermination(const std::string& name);
    };
}

#endif
