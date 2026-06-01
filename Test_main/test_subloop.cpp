/**
 * @brief: 测试 EventLoop + select 监听多个 fd
 */

#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <memory>
#include <unistd.h>
#include <fcntl.h>

#include "../Smw/utility/EventLoop.h"
#include "../Smw/utility/EventLoopThread.h"
#include "../Smw/utility/EventLoopThreadPool.h"
#include "../Smw/Sensor_driver.h"

// 模拟传感器类
class MockSensor : public SensorBase {
public:
    MockSensor(int fd) : fd_(fd) {}
    ~MockSensor() override {}

    int Init() override { return 0; }
    int Start() override { return 0; }
    int Stop() override { return 0; }
    int Release() override { fd_ = -1; return 0; }

    int fd() const override { return fd_; }
    int ReadData() override {
        char buf[256];
        ssize_t n = ::read(fd_, buf, sizeof(buf) - 1);
        if (n > 0) {
            buf[n] = '\0';
            std::cout << "  [Sensor fd=" << fd_ << "] 读取: " << buf;
            if (on_data_) {
                DataBase data;
                on_data_(&data);
            }
        }
        return 0;
    }

private:
    int fd_;
};

void test_single_eventloop() {
    std::cout << "\n========== 测试1: 单个 EventLoop 监听多个 fd ==========" << std::endl;

    int pipe1[2], pipe2[2], pipe3[2];
    pipe(pipe1); pipe(pipe2); pipe(pipe3);
    fcntl(pipe1[0], F_SETFL, O_NONBLOCK);
    fcntl(pipe2[0], F_SETFL, O_NONBLOCK);
    fcntl(pipe3[0], F_SETFL, O_NONBLOCK);

    EventLoop loop;
    MockSensor sensor1(pipe1[0]);
    MockSensor sensor2(pipe2[0]);
    MockSensor sensor3(pipe3[0]);

    sensor1.SetDataCallback([](const DataBase*) {
        std::cout << "    → 传感器1 回调执行" << std::endl;
    });
    sensor2.SetDataCallback([](const DataBase*) {
        std::cout << "    → 传感器2 回调执行" << std::endl;
    });
    sensor3.SetDataCallback([](const DataBase*) {
        std::cout << "    → 传感器3 回调执行" << std::endl;
    });

    loop.registerSensor(&sensor1);
    loop.registerSensor(&sensor2);
    loop.registerSensor(&sensor3);

    std::cout << "  注册了 3 个传感器, fd: "
              << sensor1.fd() << ", " << sensor2.fd() << ", " << sensor3.fd() << std::endl;

    std::thread writer([&]() {
        for (int i = 0; i < 3; ++i) {
            std::string msg = "sensor_data_" + std::to_string(i) + "\n";
            write(pipe1[1], msg.c_str(), msg.size());
            write(pipe2[1], msg.c_str(), msg.size());
            write(pipe3[1], msg.c_str(), msg.size());
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        // 先注销传感器，再退出
        loop.unregisterSensor(&sensor1);
        loop.unregisterSensor(&sensor2);
        loop.unregisterSensor(&sensor3);
        loop.quit();
    });

    std::cout << "  EventLoop 开始运行..." << std::endl;
    loop.loop();
    writer.join();

    close(pipe1[0]); close(pipe1[1]);
    close(pipe2[0]); close(pipe2[1]);
    close(pipe3[0]); close(pipe3[1]);

    std::cout << "  测试1 完成 ✓" << std::endl;
}

void test_thread_pool() {
    std::cout << "\n========== 测试2: EventLoopThreadPool ==========" << std::endl;

    int pipes[6][2];
    for (int i = 0; i < 6; ++i) {
        pipe(pipes[i]);
        fcntl(pipes[i][0], F_SETFL, O_NONBLOCK);
    }

    EventLoop mainLoop;
    EventLoopThreadPool pool(&mainLoop, "TestPool");
    pool.setThreadNum(3);
    pool.start();

    std::cout << "  创建了 " << pool.getAllLoops().size() << " 个 SubLoop" << std::endl;

    // 传感器和它们所在的 SubLoop
    struct SensorInfo {
        std::unique_ptr<MockSensor> sensor;
        EventLoop* ioLoop;
    };
    std::vector<SensorInfo> sensors;

    for (int i = 0; i < 6; ++i) {
        auto sensor = std::make_unique<MockSensor>(pipes[i][0]);
        sensor->SetDataCallback([i](const DataBase*) {
            std::cout << "    → 传感器" << i << " 回调执行 (线程: "
                      << std::this_thread::get_id() << ")" << std::endl;
        });

        EventLoop* ioLoop = pool.getNextLoop();
        ioLoop->registerSensor(sensor.get());
        sensors.push_back({std::move(sensor), ioLoop});
    }

    std::cout << "  6 个传感器已分配到 3 个 SubLoop" << std::endl;

    // 写入数据
    std::thread writer([&]() {
        for (int round = 0; round < 2; ++round) {
            for (int i = 0; i < 6; ++i) {
                std::string msg = "sensor" + std::to_string(i) + "_data\n";
                write(pipes[i][1], msg.c_str(), msg.size());
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
    });

    writer.join();
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // 先注销所有传感器，再销毁
    for (auto& info : sensors) {
        info.ioLoop->unregisterSensor(info.sensor.get());
    }
    sensors.clear();

    // 关闭所有 pipe
    for (int i = 0; i < 6; ++i) {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }

    std::cout << "  测试2 完成 ✓" << std::endl;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "    EventLoop + select 架构测试" << std::endl;
    std::cout << "========================================" << std::endl;

    test_single_eventloop();
    test_thread_pool();

    std::cout << "\n========================================" << std::endl;
    std::cout << "         所有测试完成 ✓" << std::endl;
    std::cout << "========================================" << std::endl;

    return 0;
}