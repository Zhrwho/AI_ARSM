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

    /* udev 热插拔 fd 注册 */
    void registerUdevFd(int fd, std::function<void()> cb);

    /* 线程判断 */
    bool isInLoopThread() const;

private:
    void handleRead();
    void handleUdevEvent();
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

    /* udev 热插拔 */
    int udevFd_;
    std::function<void()> udevCallback_;

    /* 线程 id，用于判断调用者是否在当前 Loop 线程 */
    pid_t threadId_;
};