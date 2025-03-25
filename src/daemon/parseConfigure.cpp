#include "parseConfigure.hpp"
#include "environment.hpp"
#include "parseTimeDuration.hpp"

#include <fstream>
#include <system_error>
#include <chrono>
#include <syslog.h>
#include <algorithm>

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/filesystem.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/optional.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/range/iterator_range.hpp>

using namespace containerInitd;

namespace
{

template <typename T>
T get(const boost::property_tree::ptree& ptree, const std::string& parameter)
{
    try
    {
        return ptree.get<T>(parameter);
    }
    catch(const boost::property_tree::ptree_bad_data& e)
    {
        std::ostringstream os;
        os << "invalid: " << parameter << "data type: " << e.data<boost::property_tree::ptree::data_type>() << std::endl;
        throw (os.str());
    }

    catch(const boost::property_tree::ptree_bad_path& e)
    {
        std::ostringstream os;
        os << " missing " << parameter << std::endl;
        throw os.str();
    }
}

template <typename T>
std::vector<T> getVector(const boost::property_tree::ptree& ptree, const boost::property_tree::ptree::key_type& key)
{
    std::vector<T> ret;
    try
    {
        for(auto &i : ptree.get_child(key))
        {
            ret.push_back(i.second.get_value<T>());
        }
        return ret;
    }
    catch(const boost::property_tree::ptree_bad_data& e)
    {
        std::ostringstream os;
        os << " invalid key " << key << e.data<boost::property_tree::ptree::data_type>() << std::endl;
        throw os.str();
    }
    catch(const boost::property_tree::ptree_bad_path& e)
    {
        std::ostringstream os;
        os << " no key : " << key << std::endl;
        throw (os.str());
    }
}

template <typename T>
std::vector<T> getVectorOptional(const boost::property_tree::ptree& ptree, const boost::property_tree::ptree::key_type& key)
{
    try
    {
        std::vector<T> ret;
        auto childTree = ptree.get_child_optional(key);
        if(childTree)
        {
            for(auto &i : *childTree)
            {
                ret.push_back(i.second.get_value<T>());
            }

            return ret;
        }else
        {
            return {};
        }
    }
    catch (const boost::property_tree::ptree_bad_data& e)
    {
        std::ostringstream os;
        os << "invalid:  " << key << e.data<boost::property_tree::ptree::data_type>() << std::endl;
        throw os.str();
    }
}

template <typename T>
boost::optional<T> getOptional(const boost::property_tree::ptree& ptree, const std::string& parameter)
{
    try
    {
        return ptree.get_optional<T>(parameter);
    }
    catch(const boost::property_tree::ptree_bad_data& e)
    {
        std::ostringstream os;
        os << "invalid parameter" << parameter << " : " << e.data<boost::property_tree::ptree::data_type>() << std::endl;
        throw os.str();
    }
}

template <typename T>
T getOptionalWithDefaultValue(const boost::property_tree::ptree& ptree, const std::string& parameter, const T& defaultValue)
{
    const auto optValue = getOptional<T>(ptree, parameter);
    if(optValue)
    {
        return *optValue;
    }

    return defaultValue;
}

void addEnvironment(const boost::property_tree::ptree& tree, Environment& env)
{
    for(auto & i : tree.get_child(""))
    {
        env.set(get<std::string>(i.second, "name"), get<std::string>(i.second, "value"));
    }
}

ProcessService::Heartbeat getHeartbeat(const boost::property_tree::ptree& tree)
{
    ProcessService::Heartbeat heartbeart;
    heartbeart.interval = get<std::chrono::steady_clock::duration>(tree, "interval");
    heartbeart.failureThreshold = get<int>(tree, "failureThreshold");

    if(heartbeart.failureThreshold <= 0)
    {
        throw "threshold must be > 0 \n";
    }

    return heartbeart;
}

void setLogger(Configure& cnf, boost::property_tree::ptree& tree)
{
    cnf.Logger.loggerForward = true;
    cnf.Logger.loggerLevel = getOptionalWithDefaultValue<int>(tree, "logLevel", LOG_ERR);
    cnf.Logger.loggerDevice = getOptionalWithDefaultValue<std::string>(tree, "deviceName", "/dev/log");
}

}

void addProcessService(Configure& cnf, const Environment& env, const boost::property_tree::ptree& tree)
{
    ProcessService svc;

    svc.name = get<std::string>(tree, "name");
    svc.argv = getVector<std::string>(tree, "argv");
    if(svc.argv.empty())
    {
        throw "argv is empty, should not be empty.\n";
    }

    svc.type = getOptionalWithDefaultValue<ProcessService::Type>(tree, "type", ProcessService::Type::BASIC);
    svc.action = getOptionalWithDefaultValue<ProcessService::FailureAction>(tree, "faileAction", ProcessService::FailureAction::RESTART);
    svc.standardErr = getOptionalWithDefaultValue<ProcessService::LoggerOut>(tree, "standErr", ProcessService::LoggerOut::INIT_PROCESS);
    svc.standardOut = getOptionalWithDefaultValue<ProcessService::LoggerOut>(tree, "standOut", ProcessService::LoggerOut::INIT_PROCESS);

    auto timeout = getOptional<std::chrono::steady_clock::duration>(tree, "startTimeout");
    if(timeout)
    {
        if(svc.type == ProcessService::Type::BASIC)
        {
            throw "process is basic, don't need start timeout \n";
            svc.startTimeout = *timeout;
        }
    }else
    {
        svc.startTimeout = std::chrono::seconds(100);
    }

    svc.stopTimeout = getOptionalWithDefaultValue<std::chrono::steady_clock::duration>(tree, "stopTimeout", std::chrono::seconds(200));

    Environment environment;
    environment = env;

    for(const auto& i : tree.get_child(""))
    {
        if(i.first == "addEnv")
        {
            addEnvironment(i.second, environment);
        }else
        {
            if(i.first == "heartbeat")
            {
                svc.heartbeat = getHeartbeat(i.second);
            }
        }
    }

    svc.startAfter = getVectorOptional<std::string>(tree, "startAfter");


    cnf.servicesMap.insert(std::make_pair(svc.name, svc));
}

void readConfigurationImpl(Configure& conf, std::istream& is)
{
    Environment env;

    env.copyEnviroments();

    conf.Logger.loggerForward = false;

    boost::property_tree::ptree ptree;

    boost::property_tree::read_json(is, ptree);

    for(const auto& i : ptree.get_child(""))
    {
        if ("prcessService" == i.first)
        {
            addProcessService(conf, env, ptree);
        }else if("logger" == i.first)
        {
            setLogger(conf, ptree);
        }
    }
}

Configure containerInitd::parseConfigure(const std::string path)
{
    Configure conf;

    std::ifstream file(path);

    if(!file)
    {
        throw std::system_error(errno, std::system_category(), path);
    }

    readConfigurationImpl( conf, file);

    return conf;

}

Configure containerInitd::parseConfigure(std::istream& is)
{
    Configure conf;
    readConfigurationImpl(conf, is);

    return conf;
}
