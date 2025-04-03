#include "processServiceDependency.hpp"

#include <unordered_map>
#include <string>
#include <vector>
#include <set>
#include <iostream>
#include <algorithm>
#include <sstream>

using namespace containerInitd;

void checkAllProcessServiceExist(const std::set<std::string>& startServiceSet, const std::set<std::string>&dependServiceSet,
                                 const std::unordered_map<std::string,  std::vector<std::string>>& dependServiceMap)
{
    std::set<std::string>::iterator it;
    for(it = dependServiceSet.begin(); it != dependServiceSet.end(); it++)
    {
        auto pos = startServiceSet.find(*it);
        if(startServiceSet.end() == pos)
        {
            std::ostringstream os;
            auto count = dependServiceMap.at(*it).size();
            os << "has: " << count << " process service denpend this service " << *it << std::endl;
            throw os.str();
        }
    }
}

std::string printCycle(const std::string& service, const std::vector<std::string>& trace)
{
    std::string msg("Circular dependency: ");
    bool found = false;
    for (const auto& svc : trace) {
        if (found) {
            msg += svc + "->";
        }
        if (service == svc) {
            msg += service + "->";
            found = true;
        }
    }
    return msg.substr(0, msg.size() - 2);  // 移除最后的 "->"
}

bool findService(const std::string& name, const std::vector<std::string>& vec)
{
    for(const auto& i : vec)
    {
        if(i == name)
        {
            return true;
        }
    }

    return false;
}

void detectCycleDFS(const std::string& svc,
                                     const std::unordered_map<std::string, std::vector<std::string>>& dependencyMap,
                                     std::vector<std::string>& trace,
                                     std::vector<std::string>& visited)
{
    std::string msg;
    visited.push_back(svc);
    if(findService(svc, trace))
    {
        msg = printCycle(svc, trace);
        return;
    }

    trace.push_back(svc);
    for(const auto & svc : dependencyMap.at(svc))
    {
        detectCycleDFS(svc, dependencyMap, trace, visited);
    }

    trace.pop_back();
}

void checkCircularDependencyProcesService(const std::unordered_map<std::string,  std::vector<std::string>>& dependServiceMap)
{
    std::vector<std::string> visited;
    std::vector<std::string> trace;

    for(const auto& kv : dependServiceMap)
    {
        if(!findService(kv.first, visited))
        {
            detectCycleDFS(kv.first, dependServiceMap,trace, visited);
        }
    }
}

void createStartupDenpendency(const Configure& configure, ServiceMap& nameAndServiceMap)
{
    for(const auto& i : configure.servicesMap)
    {
        const ProcessService& svc = i.second;
        if(!svc.startAfter.empty())
        {
            nameAndServiceMap.insert(std::make_pair(svc.name, svc.startAfter));
        }
    }
}

void createTerminalDependency(const Configure& configure, ServiceMap& nameAndServiceMap)
{
    for(const auto& i : configure.servicesMap)
    {
        const ProcessService& svc = i.second;
        for(const auto& name : svc.startAfter)
        {
            auto stopList = nameAndServiceMap.find(name);
            if(nameAndServiceMap.end() == stopList)
            {
                std::vector<std::string> stopAfter{svc.name};
                nameAndServiceMap.insert(std::make_pair(name, stopAfter));
            }else
            {
                std::vector<std::string>& vec = stopList->second;
                if(std::find(vec.begin(), vec.end(), svc.name) == vec.end())
                {
                    vec.push_back(svc.name);
                }
            }
        }
    }
}

void checkProcessServiceIsValid(const Configure& configure)
{
    std::unordered_map<std::string, std::vector<std::string>> startServiceMap;

    std::set<std::string> startServiceSet; //需要启动的服务

    std::set<std::string> dependServiceSet; //有被依赖的服务

    std::unordered_map<std::string,  std::vector<std::string>>dependServiceMap; //依赖该服务的图

    for(const auto & kv : configure.servicesMap)
    {
        const auto & svc = kv.second;

        // {'A', ['B', 'C', 'D']}
        // {'E', ['F', 'G','H']}
        startServiceMap.insert(std::make_pair(svc.name, svc.startAfter));

        startServiceSet.insert(svc.name);

        std::set<std::string> startAfterSet;
    
        for(auto const &i : svc.startAfter)
        {
            //服务启动依赖自身
            //{'E', ['E', 'F', 'G','H']}        `
            if(i == svc.name)
            {
                std::ostringstream os;
                os << " process service: " << svc.name <<  " start up dependent on it self " << std::endl;
                std::cout << os.str() << std::endl;
                throw os.str();
            }

            //检查是否startAfter中有重复依赖 
            if(!startAfterSet.insert(i).second)
            {
                std::ostringstream os;
                os << "duplicate process service: " << svc.name << "in start after. " << std::endl;
                std::cout << os.str() << std::endl;
                throw os.str();
            }

            //被依赖的服务
            dependServiceSet.insert(i);
            //转换成前面是服务，后面是依赖次服务启动的服务
            dependServiceMap[i].push_back(svc.name);
        }
    }

    checkAllProcessServiceExist(startServiceSet, dependServiceSet, dependServiceMap);

    checkCircularDependencyProcesService(dependServiceMap);
}

ServiceDependencyGraph containerInitd::createServiceDependencyGraph(const Configure& configure)
{
    checkProcessServiceIsValid(configure);

    ServiceDependencyGraph denpendency;

    createStartupDenpendency(configure, denpendency.startupMap);

    return denpendency;
}
