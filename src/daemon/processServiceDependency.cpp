#include "processServiceDependency.hpp"

#include <unordered_map>
#include <string>
#include <vector>
#include <unordered_set>
#include <iostream>
#include <sstream>

using namespace containerInitd;

ServiceDependencyGraph createServiceDependencyGraph(const Configure& configure)
{
    std::unordered_map<std::string, std::vector<std::string>> startServiceMap;

    std::unordered_map<std::string, std::vector<std::string>> dependServiceMap;

    std::unordered_set<std::string> startServiceSet; //需要启动的服务

    for(const auto & kv : configure.servicesMap)
    {
        const auto & svc = kv.second;

        // {'A', ['B', 'C', 'D']}
        // {'E', ['F', 'G','H']}
        startServiceMap.insert(std::make_pair(svc.name, svc.startAfter));

        startServiceSet.insert(svc.name);
        for(auto const &i : svc.startAfter)
        {
            //服务启动依赖自身
            if(i == svc.name)
            {
                std::ostringstream os;
                os << " process service: " << svc.name <<  " start up dependent on itself " << std::endl;
                std::cout << os.str() << std::endl;
                throw os.str();
            }
        }

    }
}