#ifndef LOGGER_PREFIX_BUILDER_HPP
#define LOGGER_PREFIX_BUILDER_HPP

#include <string>
#include <sys/types.h>

namespace containerInitd
{
    std::string loggerPrefixBulder(const std::string& str, pid_t pid);
}

#endif
