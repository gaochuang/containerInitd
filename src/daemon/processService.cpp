#include "processService.hpp"
#include <string>
#include <iostream>

using namespace containerInitd;

bool ProcessService::Heartbeat::operator==(const Heartbeat& other) const
{
    return ((interval == other.interval) && (failureThreshold == other.failureThreshold));
}

bool ProcessService::Heartbeat::operator!=(const Heartbeat& other) const
{
    return ((interval != other.interval) && (failureThreshold != other.failureThreshold));
}

bool ProcessService::operator==(const ProcessService& service) const
{
    return ((name == service.name) && (type == service.type) && (action == service.action) &&
            (standardOut == service.standardOut) && (standardErr == service.standardOut) && 
            (startTimeout == service.startTimeout) && (stopTimeout == service.stopTimeout) &&
            (argv == service.argv) && (env == service.env) && (startAfter == service.startAfter)
    );
}

bool ProcessService::operator!=(const ProcessService& service) const
{
    return !(*this == service);
}

std::istream& containerInitd::operator>>(std::istream& is, ProcessService::Type& type)
{
    std::string s;
    is >> s;
    if("basic" == s)
    {
        type = ProcessService::Type::BASIC;
    }else if("notify" == s)
    {
        type = ProcessService::Type::NOTIFY;
    }else
    {
        is.clear(std::ios_base::failbit);
    }

    return is;
}

std::istream& containerInitd::operator>>(std::istream& is, ProcessService::FailureAction& action)
{
    std::string s;
    is >> s;

    if("restart" == s)
    {
        action = ProcessService::FailureAction::RESTART;
    }else if("reboot_container" == s)
    {
        action = ProcessService::FailureAction::REBOOT_CONTAINER;
    }else
    {
        is.clear(std::ios_base::failbit);
    }
    return is;
}

std::istream& containerInitd::operator>>(std::istream& is, ProcessService::LoggerOut& out)
{
    std::string s;
    is >> s;
    if("init_process" == s)
    {
        out = ProcessService::LoggerOut::INIT_PROCESS;
    }else if("stand" == s)
    {
        out = ProcessService::LoggerOut::STAND;
    }else
    {
        is.clear(std::ios_base::failbit);
    }
    return is;
}
