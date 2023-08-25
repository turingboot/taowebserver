
#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <unordered_map>
#include <fcntl.h>  // fcntl()
#include <unistd.h> // close()
#include <assert.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <memory>
#include <signal.h>


#include "../spdlog/spdlog.h"
#include "../epoller/epoller.h"
#include "../threadpool/thread_pool.h"
#include "../http/http_connection.h"
#include "../timer/timer.h"
#include "../db/skiplist.h"

class TaoWebserver
{
public:
    TaoWebserver(int port, int trigMode, int timeoutMS, bool optLinger, int threadNum);
    ~TaoWebserver();

    void run(); // 一切的开始

private:
    // 对服务端的socket进行设置，最后可以得到listenFd
    bool initSocket_();

    void initEventMode_(int trigMode);

    void addClientConnection(int fd, sockaddr_in addr); // 添加一个HTTP连接
    void closeConn_(HttpConnection *client);            // 关闭一个HTTP连接

    void handleListen_();
    void handleWrite_(HttpConnection *client);
    void handleRead_(HttpConnection *client);

    void onRead_(HttpConnection *client);
    void onWrite_(HttpConnection *client);
    void onProcess_(HttpConnection *client);

    void sendError_(int fd, const char *info);
    void extentTime_(HttpConnection *client);

    static const int MAX_FD = 65536;
    static int setFdNonblock(int fd);

    int port_;
    int timeoutMS_; /* 毫秒MS,定时器的默认过期时间 */
    bool isClose_;
    int listenFd_;
    bool openLinger_;
    char *srcDir_; // 需要获取的路径

    uint32_t listenEvent_;
    uint32_t connectionEvent_;

    std::unique_ptr<HeapTimer> timer_;
    std::unique_ptr<ThreadPool> threadpool_;
    std::unique_ptr<Epoller> epoller_;
    std::unique_ptr<SkipList<std::string,std::string>> db_sk;
    std::unordered_map<int, HttpConnection> users_;
};


TaoWebserver::TaoWebserver(
    int port, int trigMode, int timeoutMS, bool optLinger, int threadNum) : port_(port), openLinger_(optLinger), timeoutMS_(timeoutMS), isClose_(false),
                                                                            timer_(new HeapTimer()), threadpool_(new ThreadPool(threadNum)), epoller_(new Epoller()),db_sk(new SkipList<std::string,std::string>(4))
{
    // 获取当前工作目录的绝对路径
    srcDir_ = getcwd(nullptr, 256);

    spdlog::info("resources dir:{}", srcDir_);
    strncat(srcDir_, "/resources/", 16);
    HttpConnection::userCount = 0;
    HttpConnection::srcDir = srcDir_;

    initEventMode_(trigMode);
    if (!initSocket_())
        isClose_ = true;

    //添加两个可以登录系统的默认账号
    db_sk->insert("root","123456");
    db_sk->insert("admin","123456");
}

TaoWebserver::~TaoWebserver()
{
    close(listenFd_);
    isClose_ = true;
    free(srcDir_);
}

void TaoWebserver::initEventMode_(int trigMode)
{
    listenEvent_ = EPOLLRDHUP;
    connectionEvent_ = EPOLLONESHOT | EPOLLRDHUP;
    switch (trigMode)
    {
    case 0:
        break;
    case 1:
        connectionEvent_ |= EPOLLET;
        break;
    case 2:
        listenEvent_ |= EPOLLET;
        break;
    case 3:
        listenEvent_ |= EPOLLET;
        connectionEvent_ |= EPOLLET;
        break;
    default:
        listenEvent_ |= EPOLLET;
        connectionEvent_ |= EPOLLET;
        break;
    }
    HttpConnection::isET = (connectionEvent_ & EPOLLET);
}

void TaoWebserver::run()
{
    int timeMS = -1; // epoll wtimeout==-1 就是无事件一直阻塞wait
    if (!isClose_)
    {
        std::string art = R"(
  _____  _    ___  
 |_   _|/ \  / _ \ 
   | | / _ \| | | |
   | |/ ___ \ |_| |
   |_/_/   \_\___/ 
        )";

        spdlog::info(art);
    }
    while (!isClose_)
    {
        if (timeoutMS_ > 0)
        {
            timeMS = timer_->getNextTrick();
        }
        int eventCnt = epoller_->wait(timeMS);
        for (int i = 0; i < eventCnt; ++i)
        {
            int fd = epoller_->getEventFd(i);
            uint32_t events = epoller_->getEvents(i);

            if (fd == listenFd_)
            {
                spdlog::info("fd:{}===>HandleListen", fd);
                handleListen_();
            }
            else if (events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR))
            {
                spdlog::info("fd:{}===>EPOLLRDHUP | EPOLLHUP | EPOLLERR", fd);
                closeConn_(&users_[fd]);
            }
            else if (events & EPOLLIN)
            {
                spdlog::info("fd:{}===>EPOLLIN", fd);
                handleRead_(&users_[fd]);
            }
            else if (events & EPOLLOUT)
            {
                spdlog::info("fd:{}===>EPOLLOUT", fd);
                handleWrite_(&users_[fd]);
            }
            else
            {
                spdlog::info("fd:{}===>Unexpected event", fd);
            }
        }
    }
}

void TaoWebserver::sendError_(int fd, const char *info)
{
    assert(fd > 0);
    int ret = send(fd, info, strlen(info), 0);
    if (ret < 0)
    {
        spdlog::info("fd:{}===>send error to client unsuccessful.", fd);
    }
    close(fd);
}

void TaoWebserver::closeConn_(HttpConnection *client)
{
    assert(client);
    spdlog::info("fd:{}===>Client quit.", client->getFd());
    epoller_->delFd(client->getFd());
    client->closeHttpConn();
}

void TaoWebserver::addClientConnection(int fd, sockaddr_in addr)
{
    assert(fd > 0);
    users_[fd].initHttpConn(fd, addr);
    if (timeoutMS_ > 0)
    {
        timer_->addHeapTimer(fd, timeoutMS_, std::bind(&TaoWebserver::closeConn_, this, &users_[fd]));
    }
    epoller_->addFd(fd, EPOLLIN | connectionEvent_);
    setFdNonblock(fd);
}

void TaoWebserver::handleListen_()
{
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    do
    {
        int fd = accept(listenFd_, (struct sockaddr *)&addr, &len);
        if (fd <= 0)
        {
            return;
        }
        else if (HttpConnection::userCount >= MAX_FD)
        {
            sendError_(fd, "Server busy!");
            spdlog::info("Clients is full");
            return;
        }
        addClientConnection(fd, addr);
    } while (listenEvent_ & EPOLLET);
}

void TaoWebserver::handleRead_(HttpConnection *client)
{
   
    extentTime_(client);
    threadpool_->submit(std::bind(&TaoWebserver::onRead_, this, client));
}

void TaoWebserver::handleWrite_(HttpConnection *client)
{
    
    extentTime_(client);
    threadpool_->submit(std::bind(&TaoWebserver::onWrite_, this, client));
}

void TaoWebserver::extentTime_(HttpConnection *client)
{
  
    if (timeoutMS_ > 0)
    {
        timer_->update(client->getFd(), timeoutMS_);
    }
}

void TaoWebserver::onRead_(HttpConnection *client)
{
    
    int ret = -1;
    int readErrno = 0;
    ret = client->readBuffer(&readErrno);
    // std::cout << ret << std::endl;
    if (ret <= 0 && readErrno != EAGAIN)
    {

        spdlog::error("fd:{}===>do not read data!",client->getFd());
        closeConn_(client);
        return;
    }
    onProcess_(client);
}

void TaoWebserver::onProcess_(HttpConnection *client)
{
    if (client->handleHttpConn())
    {
        epoller_->modFd(client->getFd(), connectionEvent_ | EPOLLOUT);
    }
    else
    {
        epoller_->modFd(client->getFd(), connectionEvent_ | EPOLLIN);
    }
}

void TaoWebserver::onWrite_(HttpConnection *client)
{
   
    int ret = -1;
    int writeErrno = 0;
    ret = client->writeBuffer(&writeErrno);
    if (client->writeBytes() == 0)
    {
        /* 传输完成 */
        if (client->isKeepAlive())
        {
            onProcess_(client);
            return;
        }
    }
    else if (ret < 0)
    {
        if (writeErrno == EAGAIN)
        {
            /* 继续传输 */
            epoller_->modFd(client->getFd(), connectionEvent_ | EPOLLOUT);
            return;
        }
    }
    closeConn_(client);
}

bool TaoWebserver::initSocket_()
{
    int ret;
    struct sockaddr_in addr;
    
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port_);
    struct linger optLinger = {0};
    if (openLinger_)
    {
        /* 优雅关闭: 直到所剩数据发送完毕或超时 */
        optLinger.l_onoff = 1;
        optLinger.l_linger = 1;
    }

    listenFd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (listenFd_ < 0)
    {
        spdlog::error("Create socket error!");
        return false;
    }

    ret = setsockopt(listenFd_, SOL_SOCKET, SO_LINGER, &optLinger, sizeof(optLinger));
    if (ret < 0)
    {
        close(listenFd_);
        spdlog::error("Init linger error!");
        return false;
    }

    int optval = 1;
    /* 端口复用 */
    /* 只有最后一个套接字会正常接收数据。 */
    ret = setsockopt(listenFd_, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval, sizeof(int));
    if (ret == -1)
    {
        spdlog::error("Set socket setsockopt error!");
        close(listenFd_);
        return false;
    }

    ret = bind(listenFd_, (struct sockaddr *)&addr, sizeof(addr));
    if (ret < 0)
    {
        spdlog::error("Bind port {} error!");
        close(listenFd_);
        return false;
    }

    ret = listen(listenFd_, 6);
    if (ret < 0)
    {
        spdlog::error("Listen port {} error!",port_);
        close(listenFd_);
        return false;
    }
    ret = epoller_->addFd(listenFd_, listenEvent_ | EPOLLIN);
    if (ret == 0)
    {
        spdlog::error("Add listenevent to epoll error!",port_);
        close(listenFd_);
        return false;
    }
    setFdNonblock(listenFd_);
    spdlog::info("Server port: {}",port_);
    return true;
}

int TaoWebserver::setFdNonblock(int fd)
{
    
    return fcntl(fd, F_SETFL, fcntl(fd, F_GETFD, 0) | O_NONBLOCK);
}


#endif // WEBSERVER_H