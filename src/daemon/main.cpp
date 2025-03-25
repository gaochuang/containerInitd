#include "parseConfigure.hpp"

#include <iostream>

using namespace containerInitd;

int main()
{
    const auto cnf = parseConfigure("/opt/init/containerinit.d");
    return 0;
}
