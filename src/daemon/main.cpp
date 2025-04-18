#include <iostream>
#include <unistd.h>
#include <linux/prctl.h>
#include <sys/prctl.h>
#include <string.h>

#include "parseConfigure.hpp"
#include "processServiceDependency.hpp"
#include "ProcessServiceManager.hpp"
#include "childProcessImp.hpp"
#include "processService.hpp"
#include "loggerOutputForwarderImp.hpp"

using namespace containerInitd;

void notifyReady()
{

}

void notifyTerminationReadyCb()
{

}

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

    bool notifyTerminationReadyCalled = false;

    boost::asio::io_context ioc;

    ProcessServiceManager processServiceManager(ioc, notifyReady,
     [&notifyTerminationReadyCalled](){
        if(notifyTerminationReadyCalled)
        {
            return;
        }
        notifyTerminationReadyCalled = true;},
       [&ioc](const ProcessService& service, const ChildProcess::StateChangeCallCB& stateChangeCb){
        return std::make_shared<ProcessChildImp>(ioc, service, stateChangeCb, [&ioc](int fd){
            return std::make_shared<LoggerOutputForwarderImp>(ioc, fd);
        });
       },
       cnf);

    return 0;
}
