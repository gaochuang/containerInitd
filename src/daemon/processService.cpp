#include "processService.hpp"
#include <string>

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
    //
    return !(*this == service);
}
