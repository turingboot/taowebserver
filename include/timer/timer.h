#ifndef TIMER_H
#define TIMER_H

#include <queue>
#include <deque>
#include <unordered_map>
#include <ctime>
#include <chrono>
#include <functional>
#include <memory>

typedef std::function<void()> TimeoutCallBack;
typedef std::chrono::high_resolution_clock Clock;
typedef std::chrono::milliseconds MS;
typedef Clock::time_point TimeStamp;

class HeapTimerNode
{
public:
    int id;             // 定时器id
    TimeStamp expire;   // 过期时间
    TimeoutCallBack cb; // 回调函数用于关闭过期的http连接

    // 按照时间排序
    bool operator<(const HeapTimerNode &t)
    {
        return expire < t.expire;
    }
};

class HeapTimer
{
    typedef std::shared_ptr<HeapTimerNode> SP_HeapTimerNode;

public:
    HeapTimer() { heap_.reserve(64); }
    ~HeapTimer() { clear(); }
    // 设置定时器
    void addHeapTimer(int id, int timeout, const TimeoutCallBack &cb);
    // 处理过期连接的定时器
    void handle_expired_event();
    // 下一次处理过期定时器的时间
    int getNextTrick();
    // 更新指定id的定时器
    void update(int id, int timeout);
    // 删除指定id节点，并且用指针触发处理函数
    void work(int id);

    void pop();
    void clear();

private:
    void del_(size_t i);                    // 删除定时器
    void siftup_(size_t i);                 // 向上调整定时器
    bool siftdown_(size_t index, size_t n); // 向下调整定时器
    void swapNode_(size_t i, size_t j);     // 交换定时器

    std::vector<HeapTimerNode> heap_;
    std::unordered_map<int, size_t> ref_; // 映射一个fd对应的定时器在heap_中的位置
};


void HeapTimer::siftup_(size_t i)
{

    size_t j = (i - 1) / 2;
    while (j >= 0)
    {
        if (heap_[j] < heap_[i])
        {
            break;
        }
        swapNode_(i, j);
        i = j;
        j = (i - 1) / 2;
    }
}

void HeapTimer::swapNode_(size_t i, size_t j)
{

    std::swap(heap_[i], heap_[j]);
    ref_[heap_[i].id] = i;
    ref_[heap_[j].id] = j;
}

bool HeapTimer::siftdown_(size_t index, size_t n)
{
    size_t i = index;
    size_t j = i * 2 + 1;
    while (j < n)
    {
        if (j + 1 < n && heap_[j + 1] < heap_[j])
            j++;
        if (heap_[i] < heap_[j])
            break;
        swapNode_(i, j);
        i = j;
        j = i * 2 + 1;
    }
    return i > index;
}

void HeapTimer::del_(size_t index)
{
    /* 删除指定位置的结点 */
    /* 将要删除的结点换到队尾，然后调整堆 */
    size_t i = index;
    size_t n = heap_.size() - 1;

    if (i < n)
    {
        swapNode_(i, n);
        if (!siftdown_(i, n))
        {
            siftup_(i);
        }
    }
    /* 队尾元素删除 */
    ref_.erase(heap_.back().id);
    heap_.pop_back();
}

void HeapTimer::addHeapTimer(int id, int timeout, const TimeoutCallBack &call_back)
{
    size_t i;
    if (ref_.count(id) == 0)
    {
        /* 新节点：堆尾插入，调整堆 */
        i = heap_.size();
        ref_[id] = i;
        heap_.push_back({id, Clock::now() + MS(timeout), call_back});
        siftup_(i);
    }
    else
    {
        /* 已有结点：调整堆 */
        i = ref_[id];
        heap_[i].expire = Clock::now() + MS(timeout);
        heap_[i].cb = call_back;
        if (!siftdown_(i, heap_.size()))
        {
            siftup_(i);
        }
    }
}

void HeapTimer::work(int id)
{
    /* 删除指定id结点，并触发回调函数 */
    if (heap_.empty() || ref_.count(id) == 0)
    {
        return;
    }
    size_t i = ref_[id];
    HeapTimerNode node = heap_[i];
    node.cb();
    del_(i);
}

void HeapTimer::update(int id, int timeout)
{
    /* 调整指定id的结点 */
    heap_[ref_[id]].expire = Clock::now() + MS(timeout);
    ;
    siftdown_(ref_[id], heap_.size());
}

void HeapTimer::handle_expired_event()
{
    /* 清除超时结点 */
    if (heap_.empty())
    {
        return;
    }
    while (!heap_.empty())
    {
        HeapTimerNode node = heap_.front();
        if (std::chrono::duration_cast<MS>(node.expire - Clock::now()).count() > 0)
        {
            break;
        }
        node.cb();
        pop();
    }
}

void HeapTimer::pop()
{
    del_(0);
}

void HeapTimer::clear()
{
    ref_.clear();
    heap_.clear();
}

int HeapTimer::getNextTrick()
{
    handle_expired_event();
    size_t res = -1;
    if (!heap_.empty())
    {
        res = std::chrono::duration_cast<MS>(heap_.front().expire - Clock::now()).count();
        if (res < 0)
        {
            res = 0;
        }
    }
    return res;
}

#endif // TIMER_H