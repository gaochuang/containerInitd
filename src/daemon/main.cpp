#include <iostream>
#include <unistd.h>

#include "parseConfigure.hpp"

using namespace containerInitd;

int main()
{
    std::string configFilesDirectory = "/etc/containerInit.d";
    auto cnf =  parseConfigureFromDirectory(configFilesDirectory);
    return 0;
}

