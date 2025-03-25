#include "parseTimeDuration.hpp"

#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <functional>

namespace 
{

long int strToLongInt(const std::string& str)
{
    size_t pos;
    long int ret = std::stol(str, &pos);
    if(pos != str.size() || ret <=0 )
    {
        throw std::invalid_argument("invaild duration: " + str);
    }
    return ret;
}

std::chrono::steady_clock::duration parse(const std::string& quantity, const std::string& qualifier)
{
    static const std::unordered_map<std::string, std::function<std::chrono::steady_clock::duration(long int)>> unit_map = {
        {"us", [](long int q) { return std::chrono::microseconds(q); }},
        {"ms", [](long int q) { return std::chrono::milliseconds(q); }},
        {"s",  [](long int q) { return std::chrono::seconds(q); }},
        {"min", [](long int q) { return std::chrono::minutes(q); }}
    };

    size_t start = qualifier.find_first_not_of(' ');
    if(std::string::npos == start)
    {
        throw "Invalid qualifier: empty string";
    }

    std::string trimmed_qualifier = qualifier.substr(start);
    auto it = unit_map.find(trimmed_qualifier);
    if(unit_map.end() == it)
    {
        throw std::invalid_argument("Invalid qualifier: " + trimmed_qualifier);
    }

    try 
    {
        return it->second(strToLongInt(quantity));
    } 
    catch (const std::exception& e) 
    {
        throw std::invalid_argument("Invalid quantity: " + trimmed_qualifier);
    }
}

}

std::chrono::steady_clock::duration parseDuration(const std::string& duration)
{
    const auto index(duration.find_first_not_of("0123456789"));
    if (index == 0)
    {
        throw "invalid duration";
    }

    if (index == std::string::npos)
    {
        return ::parse(duration.substr(0U, index), "ms");
    }
        
    return ::parse(duration.substr(0U, index), duration.substr(index));
}

std::istream& std::chrono::operator>>(std::istream& is, std::chrono::steady_clock::duration& duration)
{
    try
    {
        std::string str;
        is >> str;
        duration = parseDuration(str);
    }
    catch (...)
    {
        is.clear(std::ios_base::failbit);
    }
    return is;
}
