#include "environment.hpp"

#include <sstream>
#include <unistd.h>

using namespace containerInitd;

namespace
{
    std::string buildNameValue(const std::string& name, const std::string& value)
    {
        std::ostringstream os;
        os << name << '=' << value;
        return os.str();
    }

    //剥离name=value,将其存储在map中<name, value>
    std::pair<std::string, std::string> splitNameAndValue(const char* nameAndValue)
    {
        const char* pos;
        for (pos = nameAndValue; *pos != '='; ++pos);
        return std::make_pair(std::string(nameAndValue, pos), std::string(pos + 1));
    }
}

void Environment::set(const std::string& name, const std::string val)
{
    auto ret = map.insert(std::make_pair(name, val));
    if(!ret.second)
    {
        ret.first->second = val;
    }
}

void Environment::unset(const std::string& name)
{
    map.erase(name);
}
std::vector<std::string> Environment::get() const
{
    std::vector<std::string> ret;
    for(const auto& i : map)
    {
        ret.push_back(buildNameValue(i.first, i.second));
    }
    return ret;
}

void Environment::copyEnviroments()
{
    for (auto i = environ; *i != nullptr; ++i)
    {
        map.insert(splitNameAndValue(*i));
    }
}
