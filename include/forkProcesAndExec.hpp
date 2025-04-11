#ifndef FORK_PROCESS_AND_EXEC_HPP
#define FORK_PROCESS_AND_EXEC_HPP

#include "uidAndGid.hpp"

#include <optional>
#include <unistd.h>
#include <vector>
#include <utility>
#include <string>

namespace  containerInitd
{

pid_t forkProcessAndExec(int stdout, int stderr, const std::vector<std::string>& argv,
                         const std::vector<std::string>& env, const std::optional<UidAndGid>& uidAndGid);

std::pair<pid_t, int> forkProcessAndExecWithSocket(int stdout, int stderr, const std::vector<std::string>& argv,
                             const std::vector<std::string>& env, const std::optional<UidAndGid>& uidAndGid);                
}
#endif
