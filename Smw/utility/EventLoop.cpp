#include "EventLoop.h"
#include "../Sensor_driver.h"
#include "Logger.h"
#include <sys/eventfd.h>
#include <sys/select.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <algorithm>

static int createEventfd() {
    int evtfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (evtfd < 0) {
        log_error("[EventLoop] 创建 eventfd 失败");
        return -1;
    }
    return evtfd;
}

EventLoop::EventLoop()
    : looping_(false)
    , quit_(false)
    , callingPendingFunctors_(false)
    , wakeupFd_(createEventfd())
    , udevFd_(-1)
    , threadId_(0)
{
}

EventLoop::~EventLoop() {
    if (wakeupFd_ >= 0) {
        ::close(wakeupFd_);
    }
}

void EventLoop::loop() {
    looping_ = true;
    quit_ = false;
    threadId_ = static_cast<pid_t>(::syscall(SYS_gettid));

    while (!quit_) {
        // 1. 构建 fd_set（传感器 fd + wakeup fd）
        fd_set fds;
        int max_fd = buildFdSet(fds);

        // 把 udev fd 也加入监听
        if (udevFd_ >= 0) {
            FD_SET(udevFd_, &fds);
            max_fd = std::max(max_fd, udevFd_);
        }

        // 2. select 等待（10ms 超时）
        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 10000;  // 10ms

        int ret = ::select(max_fd + 1, &fds, nullptr, nullptr, &tv);

        // 3. 处理唤醒事件
        if (ret > 0 && FD_ISSET(wakeupFd_, &fds)) {
            handleRead();
        }

        // 4. 处理传感器数据（在当前 SubLoop 线程中执行）
        if (ret > 0) {
            std::lock_guard<std::mutex> lock(sensorMutex_);
            for (auto* sensor : sensors_) {
                int fd = sensor->fd();
                if (fd >= 0 && FD_ISSET(fd, &fds)) {
                    sensor->ReadData();  // 读取数据并触发 on_data_ 回调
                }
            }
        }

        // 5. 执行待处理任务
        doPendingFunctors();

        // 6. 处理 udev 热插拔事件（在锁外执行，避免死锁）
        if (ret > 0 && udevFd_ >= 0 && FD_ISSET(udevFd_, &fds)) {
            handleUdevEvent();
        }
    }

    looping_ = false;
}

int EventLoop::buildFdSet(fd_set& fds) {
    FD_ZERO(&fds);
    int max_fd = wakeupFd_;
    FD_SET(wakeupFd_, &fds);

    std::lock_guard<std::mutex> lock(sensorMutex_);
    for (auto* sensor : sensors_) {
        int fd = sensor->fd();
        if (fd >= 0) {
            FD_SET(fd, &fds);
            max_fd = std::max(max_fd, fd);
        }
    }
    return max_fd;
}

void EventLoop::quit() {
    quit_ = true;
    wakeup();
}

void EventLoop::runInLoop(Functor cb) {
    if (isInLoopThread()) {
        cb();
    } else {
        queueInLoop(std::move(cb));
    }
}

void EventLoop::queueInLoop(Functor cb) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        pendingFunctors_.push_back(std::move(cb));
    }
    wakeup();
}

void EventLoop::wakeup() {
    if (wakeupFd_ >= 0) {
        uint64_t one = 1;
        ::write(wakeupFd_, &one, sizeof(one));
    }
}

void EventLoop::handleRead() {
    if (wakeupFd_ >= 0) {
        uint64_t one = 0;
        ::read(wakeupFd_, &one, sizeof(one));
    }
}

void EventLoop::doPendingFunctors() {
    std::vector<Functor> functors;
    callingPendingFunctors_ = true;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        functors.swap(pendingFunctors_);
    }

    for (const auto& func : functors) {
        func();
    }

    callingPendingFunctors_ = false;
}

void EventLoop::registerSensor(SensorBase* sensor) {
    std::lock_guard<std::mutex> lock(sensorMutex_);
    sensors_.push_back(sensor);
    log_info("[EventLoop] 注册传感器 fd=%d, 当前传感器数: %zu", sensor->fd(), sensors_.size());
}

void EventLoop::unregisterSensor(SensorBase* sensor) {
    std::lock_guard<std::mutex> lock(sensorMutex_);
    sensors_.erase(std::remove(sensors_.begin(), sensors_.end(), sensor), sensors_.end());
    log_info("[EventLoop] 注销传感器 fd=%d, 当前传感器数: %zu", sensor->fd(), sensors_.size());
}

size_t EventLoop::sensorCount() const {
    std::lock_guard<std::mutex> lock(sensorMutex_);
    return sensors_.size();
}

void EventLoop::registerUdevFd(int fd, std::function<void()> cb) {
    udevFd_ = fd;
    udevCallback_ = std::move(cb);
}

void EventLoop::handleUdevEvent() {
    if (udevCallback_) {
        udevCallback_();
    }
}

bool EventLoop::isInLoopThread() const {
    return threadId_ == static_cast<pid_t>(::syscall(SYS_gettid));
}
