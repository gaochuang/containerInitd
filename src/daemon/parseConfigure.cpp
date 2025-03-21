#include "parseConfigure.hpp"
#include "environment.hpp"

#include <fstream>
#include <system_error>

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

template<typename T>
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
        os << "missing \"" << parameter << '\"';
        throw os.str();
    }
}

template<typename T>
std::vector<T> getVector(const boost::property_tree::ptree& ptree, const boost::property_tree::ptree::key_type& key)
{
    try
    {
        std::vector<T> ret;
        for(auto & i : ptree.get_child(key))
        {
            ret.push_back(i.second.get_value<T>());
        }
        return ret;
    }
    catch(const boost::property_tree::ptree_bad_data& e)
    {
        std::ostringstream os;
        os << "invalid: " << key << " data type: " << e.data<boost::property_tree::ptree::data_type>() << std::endl;
        throw (os.str());
    }

    catch(const boost::property_tree::ptree_bad_path& e)
    {
        std::ostringstream os;
        os << "missing \"" << key << '\"';
        throw os.str();
    }
}

void addProcessService( const Configure& cnf, const Environment& env, const boost::property_tree::ptree& tree)
{
    ProcessService svc;

    svc.name = get<std::string>(tree, "name");
    svc.argv = getVector<std::string>(tree, "argv");

}

}

void readConfigurationImpl(const std::string& path, Configure& conf, std::istream& is)
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
        }
    }
}

Configure parseConfigure(const std::string path)
{
    Configure conf;

    std::ifstream file(path);

    if(!file)
    {
        throw std::system_error(errno, std::system_category(), path);
    }

    readConfigurationImpl(path, conf, file);

    return conf;

}

Configure parseConfigure(std::istream& is)
{

}
