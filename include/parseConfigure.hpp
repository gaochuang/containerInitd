#ifndef PARSE_CONFIGURE_HPP
#define PARSE_CONFIGURE_HPP

#include "configure.hpp"
#include "parseConfigure.hpp"
#include <iosfwd>

namespace containerInitd
{
    Configure parseConfigure(const std::string path);
    Configure parseConfigure(std::istream& is);
}

#endif
