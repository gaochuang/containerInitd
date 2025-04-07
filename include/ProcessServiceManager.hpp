#ifndef PROCESS_SERVICE_MANAGER_HPP
#define PROCESS_SERVICE_MANAGER_HPP

#include <boost/asio/io_context.hpp>

namespace containerInitd
{
    class ProcessServiceManager
    {
    public:
      enum class State
      {
          STARTING,
          RUNNING,
          TERMINATING,
          TERMINARED,
      };
      
      using NotifyStartupReadyCb = std::function<void()>;

      using NotifyTerminationReadyCb = std::function<void()>;

      ProcessServiceManager(const boost::asio::io_context& io, const NotifyStartupReadyCb& notifyStartupReadyCb, 
                            const NotifyTerminationReadyCb& notifyTerminationReadyCb);
    };
}

#endif
