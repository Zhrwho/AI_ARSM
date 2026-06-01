#pragma once

#include "Noncopyable.h"
#include "Thread.h"
#include "EventLoop.h"
#include <mutex>
#include <condition_variable>

class EventLoopThread : Noncopyable {
public:
    using ThreadInitCallback = std::function<void(EventLoop*)>;

    EventLoopThread(ThreadInitCallback cb = nullptr,
                    const std::string& name = "");
    ~EventLoopThread();

    EventLoop* startLoop();

private:
    void threadFunc();

    EventLoop* loop_;
    bool exiting_;
    Thread thread_;
    std::mutex mutex_;
    std::condition_variable cond_;
    ThreadInitCallback callback_;
};