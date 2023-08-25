
#ifndef HTTP_CONNECTION_H
#define HTTP_CONNECTION_H

#include <arpa/inet.h> //sockaddr_in
#include <sys/uio.h>   //readv/writev
#include <iostream>
#include <sys/types.h>
#include <assert.h>

#include "http_response.h"
#include "http_request.h"


class HttpConnection
{

public:
    static bool isET;
    static const char *srcDir;
    static std::atomic<int> userCount;
    static int epollFd;

    HttpConnection();
    ~HttpConnection();

    void initHttpConn(int socketFd, const sockaddr_in &addr);

    // 每个连接中定义的对缓冲区的读写接口
    ssize_t readBuffer(int *saveErrno);
    ssize_t writeBuffer(int *saveErrno);

    // 关闭HTTP连接的接口
    void closeHttpConn();
    // 定义处理该HTTP连接的接口，主要分为request的解析和response的生成
    bool handleHttpConn();

    // 其他方法
    const char *getIP() const;
    int getPort() const;
    int getFd() const;
    sockaddr_in getAddr() const;

    inline int writeBytes()
    {
        return _iov[1].iov_len + _iov[0].iov_len;
    }

    inline bool isKeepAlive() const
    {
        return _request.isKeepAlive();
    }
    
    
private:
    int _fd; // HTTP连接对应的描述符
    struct sockaddr_in _addr; //连接的地址
    bool _isClosed; // 标记是否关闭连接
    

    int _iovCnt;
    struct iovec _iov[2];

    Buffer _readBuffer;  // 读缓冲区
    Buffer _writeBuffer; // 写缓冲区

    HttpRequest _request;
    HttpResponse _response;

    
};


const char *HttpConnection::srcDir;
std::atomic<int> HttpConnection::userCount;
bool HttpConnection::isET;
int HttpConnection::epollFd;

HttpConnection::HttpConnection()
{
    _fd = -1;
    _addr = {0};
    _isClosed = true;
}

HttpConnection::~HttpConnection()
{
    closeHttpConn();
}

void HttpConnection::initHttpConn(int fd, const sockaddr_in &addr)
{

    userCount++;
    _addr = addr;
    _fd = fd;
    _writeBuffer.initPtr();
    _readBuffer.initPtr();
    _isClosed = false;
}

void HttpConnection::closeHttpConn()
{
    _response.unmapFile_();
    if (_isClosed == false)
    {
        _isClosed = true;
        userCount--;
        close(_fd);
    }
}

ssize_t HttpConnection::readBuffer(int *saveErrno)
{
    ssize_t len = -1;
    do
    {
        len = _readBuffer.readFd(_fd, saveErrno);
        spdlog::info("fd:{}===>Read bytes: {}", _fd, len);
        if (len <= 0)
        {
            break;
        }
    } while (isET);
    return len;
}

ssize_t HttpConnection::writeBuffer(int *saveErrno)
{
    ssize_t len = -1;
    do
    {
        len = writev(_fd, _iov, _iovCnt);
        if (len <= 0)
        {
            *saveErrno = errno;
            break;
        }
        if (_iov[0].iov_len + _iov[1].iov_len == 0)
        {
            break;
        } /* 传输结束 */
        else if (static_cast<size_t>(len) > _iov[0].iov_len)
        {
            _iov[1].iov_base = (uint8_t *)_iov[1].iov_base + (len - _iov[0].iov_len);
            _iov[1].iov_len -= (len - _iov[0].iov_len);
            if (_iov[0].iov_len)
            {
                _writeBuffer.initPtr();
                _iov[0].iov_len = 0;
            }
        }
        else
        {
            _iov[0].iov_base = (uint8_t *)_iov[0].iov_base + len;
            _iov[0].iov_len -= len;
            _writeBuffer.updateReadPtr(len);
        }
    } while (isET || writeBytes() > 10240);
    return len;
}

bool HttpConnection::handleHttpConn()
{
    _request.init();
    if (_readBuffer.readableBytes() <= 0)
    {

        spdlog::info("fd:{}===>ReadBuffer is empty!", _fd);
        return false;
    }
    else if (_request.parse(_readBuffer))
    {
        _response.init(srcDir, _request.path(), _request.isKeepAlive(), 200);
    }
    else
    {

         spdlog::error("fd:{}===>400 error", _fd);
        _response.init(srcDir, _request.path(), false, 400);
    }

    _response.makeResponse(_writeBuffer);
    /* 响应头 */
    _iov[0].iov_base = const_cast<char *>(_writeBuffer.curReadPtr());
    _iov[0].iov_len = _writeBuffer.readableBytes();
    _iovCnt = 1;

    /* 文件 */
    if (_response.fileLen() > 0 && _response.file())
    {
        _iov[1].iov_base = _response.file();
        _iov[1].iov_len = _response.fileLen();
        _iovCnt = 2;
    }
    return true;
}

int HttpConnection::getFd() const
{
    return _fd;
}

struct sockaddr_in HttpConnection::getAddr() const
{
    return _addr;
}

const char *HttpConnection::getIP() const
{
    return inet_ntoa(_addr.sin_addr);
}

int HttpConnection::getPort() const
{
    return _addr.sin_port;
}


#endif
