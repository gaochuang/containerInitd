#include "loggerOutputForwarder.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/system/error_code.hpp>
#include <boost/asio/posix/stream_descriptor.hpp>
#include <memory>
#include <string>

namespace containerInitd
{

class LoggerOutputForwarderImp : public LoggerOutputForwarder
{
public:
    LoggerOutputForwarderImp(boost::asio::io_context& io, int outPutFd);

    ~LoggerOutputForwarderImp();

    int getChildFd() const override;

    void closeChildFd() override;

    void setLogPrefix(const std::string& str) override;
    void readRemaining() override;
private:
    void handleRead(const boost::system::error_code& error);
    bool readAll();
    void asyncReadSome();
    void write(const char* buffer, size_t size);

    int outputFd;
    std::string prefix;
    int childFd;
 
    std::shared_ptr<boost::asio::posix::stream_descriptor> streamDescriptorPtr;

};

}
