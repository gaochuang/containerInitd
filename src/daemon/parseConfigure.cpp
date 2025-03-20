#include "parseConfigure.hpp"

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

void readConfigurationImpl(const std::string& path, Configure& conf, std::istream& is)
{

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
