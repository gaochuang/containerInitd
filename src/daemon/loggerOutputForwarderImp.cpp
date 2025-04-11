#include "loggerOutputForwarderImp.hpp"

#include <unistd.h>
#include <sys/uio.h>

using namespace containerInitd;

LoggerOutputForwarderImp::LoggerOutputForwarderImp(boost::asio::io_context& io, int outPutFd):
                          outputFd(outPutFd),
                          streamDescriptorPtr(std::make_shared<boost::asio::posix::stream_descriptor>(io))
    
{
    int fd[2];
    pipe2(fd, O_CLOEXEC | O_DIRECT);

    auto flag = fcntl(fd[0], F_GETFL, 0);

    fcntl(fd[0], F_SETFL, flag | O_NONBLOCK);

    streamDescriptorPtr->assign(fd[0]);
    childFd = fd[1];

    asyncReadSome();

}

LoggerOutputForwarderImp::~LoggerOutputForwarderImp()
{
    if(childFd >= 0)
    {
        close(childFd);
    }
}

int LoggerOutputForwarderImp::getChildFd() const
{
    return childFd;
}

void LoggerOutputForwarderImp::closeChildFd()
{
    close(childFd);
    childFd = -1;
}

void LoggerOutputForwarderImp::setLogPrefix(const std::string& str)
{
    this->prefix = str;
}

void LoggerOutputForwarderImp::readRemaining()
{
    int fd = streamDescriptorPtr->native_handle();
    int initialUnreadBytes = 0;

    ioctl(fd, FIONREAD, &initialUnreadBytes); //获取剩余可读的字节数

    while(1)
    {
        char buffer[PIPE_BUF];
        const size_t ssize = TEMP_FAILURE_RETRY(::read(fd, buffer, sizeof(buffer)));
        if(ssize <= 0)
        {
            break;
        }
        write(buffer, static_cast<size_t>(ssize));

        initialUnreadBytes -= static_cast<int>(ssize);
        if(initialUnreadBytes <= 0)
        {
            break;
        }
    }
}

void LoggerOutputForwarderImp::handleRead(const boost::system::error_code& error)
{
    if(error)
    {
        return;
    }

    if(readAll())
    {
        asyncReadSome();
    }
}

void LoggerOutputForwarderImp::asyncReadSome()
{
    streamDescriptorPtr->async_read_some(boost::asio::null_buffers(), [this](const boost::system::error_code& ec, size_t){
        handleRead(ec);
    });
}

bool LoggerOutputForwarderImp::LoggerOutputForwarderImp::readAll()
{
    while (1)
    {
        char buffer[PIPE_BUF];
        ssize_t ssize = TEMP_FAILURE_RETRY(::read(streamDescriptorPtr->native_handle(), buffer, sizeof(buffer)));
        if((ssize < 0 ) && (errno == EAGAIN))
        {
            return true;
        }

        if(ssize <= 0)
        {
            return false;
        }
        write(buffer, static_cast<size_t>(ssize));
    }
}

void LoggerOutputForwarderImp::write(const char* buffer, size_t size)
{
    /*
   构建 iovec 数组来描述三段要写入的数据：

   顺序	    内容说明	        用途
   iov[0]	日志前缀（prefix）	比如 "[INFO] ", "[ERROR] "
   iov[1]	日志正文（buffer）	实际日志内容
   iov[2]	换行符（\n）	    保证一行一条日志
  */
    struct iovec iov[] =
    {
        {const_cast<char*>(prefix.data()), prefix.size()},
        {const_cast<char*>(buffer), size},
        {const_cast<char*>("\n"), 1U},
    };

    if(buffer[size - 1U] == '\n')
    {
        TEMP_FAILURE_RETRY(::write(outputFd, iov, 2U));
    }else
    {
        TEMP_FAILURE_RETRY(::write(outputFd, iov, 3U));
    }
}
