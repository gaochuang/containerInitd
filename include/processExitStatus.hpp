#ifndef PROCESS_EXIT_STATUS_HPP
#define PROCESS_EXIT_STATUS_HPP

#include <string>

namespace containerInitd
{
    bool isSuccessExit(int status, int sig);
    std::string getProcessExitDescription(int status);
}

#endif
