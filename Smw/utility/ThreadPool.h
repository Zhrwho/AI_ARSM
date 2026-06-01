#pragma once

#include "Noncopyable.h"
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <atomic>

class ThreadPool : Noncopyable {
public:
    using Task = std::function<void()>;

    explicit ThreadPool(size_t threads = 4);
    ~ThreadPool();

    // 提交任务到线程池
    void submit(Task task);

    // 关闭线程池
    void shutdown();

    // 获取队列中待处理任务数
    size_t queueSize() const;

private:
    void workerFunc();

    std::vector<std::thread> workers_;
    std::queue<Task> tasks_;

    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::atomic<bool> stop_;
};