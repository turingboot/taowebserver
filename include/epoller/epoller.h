

#ifndef EPOLLER_H
#define EPOLLER_H

#include<sys/epoll.h> 
#include<fcntl.h> 
#include<unistd.h> 
#include<assert.h>
#include<vector>
#include<errno.h>

class Epoller {
public:
    explicit Epoller(int maxEvents=1024);
    ~Epoller();
    
    //将描述符fd加入epoll监控
    bool addFd(int fd, uint32_t events);
    //修改描述符fd对应的事件
    bool modFd(int fd, uint32_t events);
    //将描述符fd移除epoll的监控
    bool delFd(int fd);
    //用于返回监控的结果，成功时返回就绪的文件描述符的个数
    int wait(int timewait = -1);
    //获取fd的函数
    int getEventFd(size_t i) const;
    //获取events的函数
    uint32_t getEvents(size_t i) const;
    //获取epollerFd_
    int getEpollFd() const;

private:
    int epollerFd_; //这是标志epoll的描述符
 
    std::vector<struct epoll_event> events_; //就绪的事件
};


Epoller::Epoller(int maxEvents) : epollerFd_(epoll_create(5)), events_(maxEvents)
{
}

Epoller::~Epoller()
{
    close(epollerFd_);
}

bool Epoller::addFd(int fd, uint32_t events)
{
    epoll_event ev = {0};
    ev.data.fd = fd;
    ev.events = events;
    return 0 == epoll_ctl(epollerFd_, EPOLL_CTL_ADD, fd, &ev);
}

bool Epoller::modFd(int fd, uint32_t events)
{

    epoll_event ev = {0};
    ev.data.fd = fd;
    ev.events = events;
    return 0 == epoll_ctl(epollerFd_, EPOLL_CTL_MOD, fd, &ev);
}

bool Epoller::delFd(int fd)
{
    epoll_event ev = {0};
    return 0 == epoll_ctl(epollerFd_, EPOLL_CTL_DEL, fd, &ev);
}

// 用于返回监控的结果，成功时返回就绪的文件描述符的个数
int Epoller::wait(int timewait)
{
    return epoll_wait(epollerFd_, &events_[0], static_cast<int>(events_.size()), timewait);
}
// 获取fd的函数
int Epoller::getEventFd(size_t i) const
{

    return events_[i].data.fd;
}
// 获取events的函数
uint32_t Epoller::getEvents(size_t i) const
{

    return events_[i].events;
}

// 获取epoll_fd的函数
int Epoller::getEpollFd() const
{

    return epollerFd_;
}

#endif //EPOLLER_H