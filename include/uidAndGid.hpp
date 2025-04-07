#ifndef UID_AND_GID_HPP
#define UID_AND_GID_HPP

#include <sys/types.h>
#include <unistd.h>
#include <utility>

namespace containerInitd
{
    using UidAndGid = std::pair<uid_t, gid_t>;
}

#endif
