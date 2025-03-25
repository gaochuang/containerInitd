#ifndef PROCESS_SERICE_HPP
#define PROCESS_SERICE_HPP

#include <string>
#include <chrono>
#include <optional>
#include <vector>

namespace containerInitd
{
 class ProcessService
{
public:
    enum class Type
    {
        BASIC,
        NOTIFY,
    };

    enum class FailureAction
    {
        RESTART,
        REBOOT_CONTAINER,
    };

    enum class LoggerOut
    {
        INIT_PROCESS,
        STAND,
    };

    class Heartbeat
    {
    public:
        Heartbeat() = default;
        ~Heartbeat() = default;
    
        std::chrono::steady_clock::duration interval;
        int failureThreshold;
        bool operator==(const Heartbeat& other) const;
        bool operator!=(const Heartbeat& other) const;
    };
    
    ProcessService() = default;
    ~ProcessService() = default;

    std::string name;
    Type type;
    FailureAction action;
    LoggerOut standardOut;
    LoggerOut standardErr;
    std::chrono::steady_clock::duration startTimeout;
    std::chrono::steady_clock::duration stopTimeout;
    std::optional<Heartbeat> heartbeat;

    std::vector<std::string> argv;
    std::vector<std::string> env;
    std::vector<std::string> startAfter;

    bool operator==(const ProcessService& service) const;
    bool operator!=(const ProcessService& service) const;

};

std::istream& operator>>(std::istream& is, ProcessService::Type& type);

std::istream& operator>>(std::istream& is, ProcessService::FailureAction& action);

std::istream& operator>>(std::istream& is, ProcessService::LoggerOut& out);

}

#endif
