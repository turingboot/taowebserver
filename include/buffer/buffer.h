#ifndef BUFFER_H
#define BUFFER_H

#include <vector>
#include <iostream>
#include <cstring>
#include <atomic>
#include <unistd.h>  //read() write()
#include <sys/uio.h> //readv() writev()
#include <assert.h>

#include "../spdlog/spdlog.h"

class Buffer
{
public:
    Buffer(int initBufferSize = 1024);
    ~Buffer() = default;

    // 缓存区中可以读取的字节数
    size_t writeableBytes() const;
    // 缓存区中可以写入的字节数
    size_t readableBytes() const;
    // 缓存区中已经读取的字节数
    size_t readBytes() const;

    // 获取当前读指针
    const char *curReadPtr() const;
    // 获取当前写指针
    const char *curWritePtrConst() const;
    char *curWritePtr();
    // 更新读指针
    void updateReadPtr(size_t len);
    void updateReadPtrUntilEnd(const char *end); // 将读指针直接更新到指定位置
    // 更新写指针
    void updateWritePtr(size_t len);
    // 将读指针和写指针初始化
    void initPtr();

    // 保证将数据写入缓冲区
    void ensureWriteable(size_t len);
    // 将数据写入到缓冲区
    void append(const char *str, size_t len);
    void append(const std::string &str);
    void append(const void *data, size_t len);
    void append(const Buffer &buffer);

    // IO操作的读与写
    ssize_t readFd(int fd, int *Errno);
    ssize_t writeFd(int fd, int *Errno);

    // 将缓冲区的数据转化为字符串
    std::string AlltoStr();


    void printContent()
    {
        std::cout << "pointer location info:" << _readPos << " " << _writePos << std::endl;
        for (int i = _readPos; i <= _writePos; ++i)
        {
            std::cout << _buffer[i] << " ";
        }
        std::cout << std::endl;
    }

private:
    // 返回指向缓冲区初始位置的指针
    char *begin_();
    const char *begin_() const;
    
    // 用于缓冲区空间不够时的扩容
    void allocateSpace(size_t len);

    std::vector<char> _buffer;          // 缓冲区实体 
    std::atomic<std::size_t> _readPos;  // 读指针，用于标识当前读到哪里了
    std::atomic<std::size_t> _writePos; // 写指针，用于标识当前写到哪里了
};


Buffer::Buffer(int initBuffersize) : _buffer(initBuffersize), _readPos(0), _writePos(0) {}


size_t Buffer::readableBytes() const
{
    return _writePos - _readPos;
}

size_t Buffer::writeableBytes() const
{
    return _buffer.size() - _writePos;
}

size_t Buffer::readBytes() const
{
    return _readPos;
}

const char *Buffer::curReadPtr() const
{
    return begin_() + _readPos;
}

const char *Buffer::curWritePtrConst() const
{
    return begin_() + _writePos;
}

char *Buffer::curWritePtr()
{
    return begin_() + _writePos;
}

void Buffer::updateReadPtr(size_t len)
{

    if(len <= readableBytes())
    _readPos += len;
}

void Buffer::updateReadPtrUntilEnd(const char *end)
{
   
    updateReadPtr(end - curReadPtr());
}

void Buffer::updateWritePtr(size_t len)
{
    _writePos += len;
}

void Buffer::initPtr()
{
    bzero(&_buffer[0], _buffer.size());
    _readPos = 0;
    _writePos = 0;
}

void Buffer::allocateSpace(size_t len)
{
    // 如果_buffer里面剩余的空间有len就进行调整，否则需要申请空间。
    // 剩余空间包括write指针之前的空间和可写的空间
    if (writeableBytes() + readBytes() < len)
    {
        _buffer.resize(_writePos + len + 1);
    }
    else
    {
        // 将读指针置为0,调整一下
        size_t readable = readableBytes();
        std::copy(begin_() + _readPos, begin_() + _writePos, begin_());
        _readPos = 0;
        _writePos = readable;
  
    }
}

void Buffer::ensureWriteable(size_t len)
{
    if (writeableBytes() < len)
    {
        allocateSpace(len);
    }
}

void Buffer::append(const char *str, size_t len)
{

    ensureWriteable(len);
    std::copy(str, str + len, curWritePtr());
    updateWritePtr(len);
}

void Buffer::append(const std::string &str)
{
    append(str.data(), str.length());
}

void Buffer::append(const void *data, size_t len)
{
    append(static_cast<const char *>(data), len);
}

void Buffer::append(const Buffer &buffer)
{
    append(buffer.curReadPtr(), buffer.readableBytes());
}

ssize_t Buffer::readFd(int fd, int *Errno)
{
    char buff[65535]; // 暂时的缓冲区
    struct iovec iov[2];
    const size_t writable = writeableBytes();

    iov[0].iov_base = begin_() + _writePos;
    iov[0].iov_len = writable;
    iov[1].iov_base = buff;
    iov[1].iov_len = sizeof(buff);

    const ssize_t len = readv(fd, iov, 2);
    if (len < 0)
    {
        spdlog::error("fd:{}===>Read data unsuccessfully.", fd);
        *Errno = errno;
    }
    else if (static_cast<size_t>(len) <= writable)
    {
        _writePos += len;
    }
    else
    {
        _writePos = _buffer.size();
        append(buff, len - writable);
    }
    return len;
}

ssize_t Buffer::writeFd(int fd, int *Errno)
{
    size_t readSize = readableBytes();
    ssize_t len = write(fd, curReadPtr(), readSize);
    if (len < 0)
    {
        spdlog::error("fd:{}===>Failed to write data to fd.", fd);
        *Errno = errno;
        return len;
    }
    _readPos += len;
    return len;
}

std::string Buffer::AlltoStr()
{
    std::string str(curReadPtr(), readableBytes());
    initPtr();
    return str;
}

char *Buffer::begin_()
{
    return &*_buffer.begin();
}

const char *Buffer::begin_() const
{
    return &*_buffer.begin();
}

#endif // BUFFER_H