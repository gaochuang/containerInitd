#ifndef LOGGER_OUTPUT_FORWARDER_HPP
#define LOGGER_OUTPUT_FORWARDER_HPP

#include <string>

namespace containerInitd
{

class LoggerOutputForwarder
{
public:
    LoggerOutputForwarder() = default;
    virtual ~LoggerOutputForwarder() = default;

    virtual int getChildFd() const = 0;
    virtual void closeChildFd() = 0;

    virtual void setLogPrefix(const std::string& str) = 0;
    virtual void readRemaining() = 0;

    LoggerOutputForwarder(const LoggerOutputForwarder&) = delete;
    LoggerOutputForwarder(LoggerOutputForwarder&&) = delete;

    LoggerOutputForwarder& operator=(const LoggerOutputForwarder&) = delete;
    LoggerOutputForwarder& operator=(LoggerOutputForwarder&& ) = delete;
    
};

}

#endif
