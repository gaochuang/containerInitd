#ifndef CHILD_PROCESS_IMP_HPP
#define CHILD_PROCESS_IMP_HPP

#include <boost/asio/io_context.hpp>
#include <boost/asio.hpp>
#include <boost/asio/deadline_timer.hpp>
#include <chrono>
#include <memory>
#include <string>
#include <unistd.h>

#include "childProcess.hpp"
#include "processService.hpp"
#include "childProcess.hpp"
#include "loggerOutputForwarder.hpp"

namespace containerInitd
{


class ProcessChildImp : public ChildProcess
{
public:
    using LoggerOutputForwarderCreator = std::function<std::shared_ptr<LoggerOutputForwarder>(int)>;

    using SteadyTimer = boost::asio::basic_waitable_timer<std::chrono::steady_clock>;
    using StreamDescriptor = boost::asio::posix::stream_descriptor;

    ProcessChildImp(boost::asio::io_context& io, const ProcessService& service, const StateChangeCallCB& stateChangeCb, const LoggerOutputForwarderCreator& loggetOutputCreator);
    ~ProcessChildImp();

    ProcessState getProcessState() const override;
    pid_t getProcessPid() const override;
    void processTerminated(int exitStatus) override;
    void startTerminating() override;
    void startTerminating(int sig) override;

    void asyncRead();

private:
   boost::asio::io_context& ioc;
   const std::string name;
   std::chrono::steady_clock::duration heartbeatTimeout;
   std::chrono::steady_clock::duration stopTimeout;
   std::chrono::steady_clock::duration watchdogTimeout;
   
   StateChangeCallCB stateChangeCb;
   std::shared_ptr<LoggerOutputForwarder> outputForwarder;
   std::shared_ptr<LoggerOutputForwarder> errputForwarder;
   ProcessState state;
   pid_t pid;
   std::shared_ptr<SteadyTimer> heartbeatTimer;
   int unackedHeartbeatCount;
   int heartbeatThreshold;

   std::shared_ptr<SteadyTimer> watchdogTimer;
   std::shared_ptr<SteadyTimer> startTimer;
   std::shared_ptr<SteadyTimer> stopTimer;
   std::shared_ptr<StreamDescriptor> streamDescriptor;
   int savedTerminationSignal;
   std::string exitReason;

   void handleRead(const boost::system::error_code& ec);
   void startTimeoutHandler(const boost::system::error_code& ec);
   void killProcess();
   void setProcessState(ProcessState state);
   void startHeartBeat();
   void heartBeatTimeoutHandle(const boost::system::error_code& ec);
   void sendHeartBeatRequestMessage();
   void startWatchdog();
   void watchdogTimeoutHandle(const boost::system::error_code& ec);
};

}

#endif
