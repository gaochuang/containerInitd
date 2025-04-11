#include "loggerPrefixBuilder.hpp"

#include <sstream>

namespace
{
    std::string loggerPrefixBulderImp(const std::string& str, pid_t pid)
    {
        std::ostringstream os;
        os << str << '[' << pid << "]: ";
        return os.str(); 
    }
}

std::string containerInitd::loggerPrefixBulder(const std::string& str, pid_t pid)
{
    // /usr/bin/myapp  -> myapp[123]

    auto pos = str.find_last_of('/');
    if(std::string::npos == pos)
    {
        return loggerPrefixBulderImp(str, pid);
    }

    return loggerPrefixBulderImp(str.substr(pos + 1), pid);
}
