#ifndef ENVIRONMENT_HPP
#define ENVIRONMENT_HPP

#include <unordered_map>
#include <string>
#include <vector>

namespace containerInitd
{

class Environment
{
public:
    Environment() = default;
    ~Environment() = default;

    void set(const std::string& name, const std::string val);
    void unset(const std::string& name);
    std::vector<std::string> get() const;
    void copyEnviroments();


private:
    std::unordered_map<std::string, std::string> map;
};

}

#endif
