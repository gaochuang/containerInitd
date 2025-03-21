#ifndef CONFIGURE_HPP
#define CONFIGURE_HPP

#include "processService.hpp"

#include <unordered_map>
#include <string>

namespace containerInitd
{
    struct Configure
    {
       std::unordered_map<std::string, ProcessService> servicesMap;
       struct Logger
       {
            std::string loggerDevice;
            bool loggerForward;
            int loggerLevel;
       }Logger;
    };
}

#endif
