#ifndef PROCESS_SERVICE_DEPENDENCY_HPP
#define PROCESS_SERVICE_DEPENDENCY_HPP

#include <unordered_map>
#include <vector>
#include <string>

#include "configure.hpp"

namespace containerInitd
{   
    using ServiceMap = std::unordered_map<std::string, std::vector<std::string>>;

    struct ServiceDependencyGraph
    {
        ServiceMap startupMap;
        ServiceMap terminalMap;
    };

    ServiceDependencyGraph createServiceDependencyGraph(const Configure& configure);
}

#endif
