#ifndef PARSE_TIME_DURATOIN_HPP
#define PARSE_TIME_DURATOIN_HPP

#include <chrono>
#include <iosfwd>
#include <string>

namespace containerInitd
{
    std::chrono::steady_clock::duration parseDuration(const std::string& duration);
}

namespace std
{
    namespace chrono
    {
        std::istream& operator>>(std::istream& is, std::chrono::steady_clock::duration& duration);
    }

}

#endif
