#ifndef PARSE_CONFIGURE_HPP
#define PARSE_CONFIGURE_HPP

#include "configure.hpp"
#include <iosfwd>

namespace containerInitd
{
    Configure parseConfigureFromDirectory(const std::string& path);
    Configure parseConfigure(const std::string path);
    Configure parseConfigure(std::istream& is);
}

#endif
