#include <iostream>
#include <unistd.h>
#include <linux/prctl.h>
#include <sys/prctl.h>
#include <string.h>

#include "parseConfigure.hpp"
#include "processServiceDependency.hpp"

using namespace containerInitd;

int main()
{
    std::string configFilesDirectory = "/etc/containerInit.d";
    auto cnf =  parseConfigureFromDirectory(configFilesDirectory);
    auto dependency = createServiceDependencyGraph(cnf);

    if(-1 == prctl(PR_SET_CHILD_SUBREAPER, 1))
    {
        std::cerr << "prctl(PR_SET_CHILD_SUBREAPER, 1) failed, err: " << strerror(errno) << std::endl;
        return EXIT_FAILURE;
    }

    return 0;
}
