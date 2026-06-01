#pragma once

#include "Noncopyable.h"
#include <functional>
#include <vector>
#include <mutex>
#include <atomic>

// 前向声明
class SensorBase;

class EventLoop : Noncopyable {
public:
    using Functor = std::function<void()>;

    EventLoop();
    ~EventLoop();

    // 事件循环
    void loop();
    void quit();

    // 任务队列
    void runInLoop(Functor cb);
    void queueInLoop(Functor cb);
    void wakeup();

    /* 传感器管理 */
    void registerSensor(SensorBase* sensor);
    void unregisterSensor(SensorBase* sensor);
    size_t sensorCount() const;

private:
    void handleRead();
    void doPendingFunctors();
    int buildFdSet(fd_set& fds);

    std::atomic<bool> looping_;
    std::atomic<bool> quit_;
    std::atomic<bool> callingPendingFunctors_;

    int wakeupFd_;
    std::mutex mutex_;
    std::vector<Functor> pendingFunctors_;

    /* 传感器列表 */
    std::vector<SensorBase*> sensors_;
    mutable std::mutex sensorMutex_;
};