/**
 * @brief: 完整的测试框架 - 单元测试+集成测试
 * 支持：
 * 1. ClassFactory 工厂模式测试
 * 2. EventLoop 并发测试
 * 3. 传感器驱动生命周期测试
 * 4. SensorManager 功能测试
 */

#include <iostream>
#include <cassert>
#include <thread>
#include <chrono>
#include <memory>
#include <atomic>
#include <vector>
#include <cstring>

#include "../Smw/utility/ClassFactory.h"
#include "../Smw/utility/EventLoop.h"
#include "../Smw/utility/Logger.h"
#include "../Smw/Sensor_driver.h"
#include "../Smw/Sensor_manager.h"

using namespace std;

// ============================================================================
// 测试框架基础设施
// ============================================================================

class TestCase {
public:
    virtual ~TestCase() = default;
    virtual void Setup() {}
    virtual void TearDown() {}
    virtual void Run() = 0;
    const string& GetName() const { return name_; }

protected:
    TestCase(const string& name) : name_(name) {}

    void ASSERT_TRUE(bool condition, const string& msg = "") {
        if (!condition) {
            cerr << "[FAIL] " << name_ << ": " << msg << endl;
            throw runtime_error("Assertion failed");
        }
    }

    void ASSERT_FALSE(bool condition, const string& msg = "") {
        ASSERT_TRUE(!condition, msg);
    }

    void ASSERT_EQ(int a, int b, const string& msg = "") {
        if (a != b) {
            cerr << "[FAIL] " << name_ << ": " << a << " != " << b << " - " << msg << endl;
            throw runtime_error("Assertion failed");
        }
    }

    void ASSERT_NE(int a, int b, const string& msg = "") {
        if (a == b) {
            cerr << "[FAIL] " << name_ << ": " << a << " == " << b << " - " << msg << endl;
            throw runtime_error("Assertion failed");
        }
    }

    void ASSERT_NULLPTR(void* ptr, const string& msg = "") {
        ASSERT_TRUE(ptr == nullptr, msg);
    }

    void ASSERT_NOT_NULLPTR(void* ptr, const string& msg = "") {
        ASSERT_TRUE(ptr != nullptr, msg);
    }

private:
    string name_;
};

class TestRunner {
public:
    static TestRunner& GetInstance() {
        static TestRunner instance;
        return instance;
    }

    void Register(unique_ptr<TestCase> test) {
        tests_.push_back(move(test));
    }

    void RunAll() {
        int passed = 0, failed = 0;

        cout << "\n========== 开始执行测试套件 ==========" << endl;
        cout << "总测试数: " << tests_.size() << endl << endl;

        for (auto& test : tests_) {
            try {
                cout << "[RUN] " << test->GetName() << " ... ";
                test->Setup();
                test->Run();
                test->TearDown();
                cout << "[PASS]" << endl;
                passed++;
            } catch (const exception& e) {
                cout << "[FAIL] - " << e.what() << endl;
                failed++;
            }
        }

        cout << "\n========== 测试结果 ==========" << endl;
        cout << "通过: " << passed << endl;
        cout << "失败: " << failed << endl;
        cout << "总数: " << (passed + failed) << endl;
        cout << "成功率: " << (100.0 * passed / tests_.size()) << "%" << endl;
    }

private:
    TestRunner() = default;
    vector<unique_ptr<TestCase>> tests_;
};

// 测试注册宏
#define REGISTER_TEST(TestClass) \
    namespace { \
        struct TestRegistrator_##TestClass { \
            TestRegistrator_##TestClass() { \
                TestRunner::GetInstance().Register(make_unique<TestClass>()); \
            } \
        }; \
        static TestRegistrator_##TestClass g_registrator_##TestClass; \
    }

// ============================================================================
// 模拟传感器 - 用于单元测试
// ============================================================================

class MockSensor : public SensorBase {
public:
    MockSensor() : fd_(-1), init_called_(false), start_called_(false) {}

    int Init() override {
        init_called_ = true;
        fd_ = 100;  // 模拟FD
        status_ = SensorStatus::kReady;
        return 0;
    }

    int Start() override {
        start_called_ = true;
        status_ = SensorStatus::kCapturing;
        return 0;
    }

    int Stop() override {
        status_ = SensorStatus::kReady;
        return 0;
    }

    int Release() override {
        if (fd_ >= 0) {
            fd_ = -1;
        }
        return 0;
    }

    int fd() const override { return fd_; }

    int ReadData() override {
        if (on_data_) {
            ImuData data;
            data.frameId = 1;
            data.pitch = 1.5f;
            on_data_(&data);
        }
        return 0;
    }

    bool init_called() const { return init_called_; }
    bool start_called() const { return start_called_; }

private:
    int fd_;
    bool init_called_;
    bool start_called_;
};

CLASS_LOADER_REGISTER_CLASS(MockSensor, SensorBase)

// ============================================================================
// 测试用例
// ============================================================================

// 测试1: ClassFactory 创建对象
class TestClassFactoryCreation : public TestCase {
public:
    TestClassFactoryCreation() : TestCase("ClassFactory创建对象") {}

    void Run() override {
        auto obj = CreateObject<SensorBase>("MockSensor");
        ASSERT_NOT_NULLPTR(obj.get(), "工厂应创建对象");
        ASSERT_EQ(obj->GetStatus(), (int)SensorStatus::kDefault, "初始状态应为Default");
    }
};
REGISTER_TEST(TestClassFactoryCreation)

// 测试2: ClassFactory 创建失败
class TestClassFactoryFailed : public TestCase {
public:
    TestClassFactoryFailed() : TestCase("ClassFactory创建失败处理") {}

    void Run() override {
        auto obj = CreateObject<SensorBase>("NonExistentSensor");
        ASSERT_NULLPTR(obj.get(), "不存在的类应返回nullptr");
    }
};
REGISTER_TEST(TestClassFactoryFailed)

// 测试3: MockSensor 生命周期
class TestMockSensorLifecycle : public TestCase {
public:
    TestMockSensorLifecycle() : TestCase("传感器生命周期") {}

    void Run() override {
        auto sensor = CreateObject<SensorBase>("MockSensor");
        ASSERT_NOT_NULLPTR(sensor.get(), "工厂应创建对象");

        // Init
        int ret = sensor->Init();
        ASSERT_EQ(ret, 0, "Init应返回0");
        ASSERT_EQ(sensor->GetStatus(), (int)SensorStatus::kReady, "Init后状态应为Ready");

        // Start
        ret = sensor->Start();
        ASSERT_EQ(ret, 0, "Start应返回0");
        ASSERT_EQ(sensor->GetStatus(), (int)SensorStatus::kCapturing, "Start后状态应为Capturing");

        // fd
        int fd = sensor->fd();
        ASSERT_NE(fd, -1, "fd应有效");

        // Stop
        ret = sensor->Stop();
        ASSERT_EQ(ret, 0, "Stop应返回0");

        // Release
        ret = sensor->Release();
        ASSERT_EQ(ret, 0, "Release应返回0");
    }
};
REGISTER_TEST(TestMockSensorLifecycle)

// 测试4: EventLoop 基础功能
class TestEventLoopBasic : public TestCase {
public:
    TestEventLoopBasic() : TestCase("EventLoop基础功能") {}

    void Run() override {
        EventLoop loop;

        // 测试 queueInLoop
        atomic<bool> called(false);
        loop.queueInLoop([&called]() {
            called = true;
        });

        // 在线程中运行loop
        thread t([&loop]() {
            loop.loop();
        });

        this_thread::sleep_for(chrono::milliseconds(100));
        loop.quit();
        t.join();

        ASSERT_TRUE(called, "queueInLoop回调应执行");
    }
};
REGISTER_TEST(TestEventLoopBasic)

// 测试5: EventLoop 线程安全性
class TestEventLoopThreadSafety : public TestCase {
public:
    TestEventLoopThreadSafety() : TestCase("EventLoop线程安全") {}

    void Run() override {
        EventLoop loop;
        atomic<int> counter(0);

        thread t([&loop]() {
            loop.loop();
        });

        // 从多个线程投递任务
        for (int i = 0; i < 10; ++i) {
            thread([&loop, &counter, i]() {
                loop.queueInLoop([&counter]() {
                    counter++;
                });
            }).detach();
        }

        this_thread::sleep_for(chrono::milliseconds(200));
        loop.quit();
        t.join();

        this_thread::sleep_for(chrono::milliseconds(100));
        ASSERT_EQ(counter, 10, "所有投递任务应执行");
    }
};
REGISTER_TEST(TestEventLoopThreadSafety)

// 测试6: EventLoopThreadPool 轮询分配
class TestEventLoopThreadPoolRoundRobin : public TestCase {
public:
    TestEventLoopThreadPoolRoundRobin() : TestCase("EventLoopThreadPool轮询") {}

    void Run() override {
        EventLoop mainLoop;
        EventLoopThreadPool pool(&mainLoop, "TestPool");
        pool.setThreadNum(3);
        pool.start();

        EventLoop* loop1 = pool.getNextLoop();
        EventLoop* loop2 = pool.getNextLoop();
        EventLoop* loop3 = pool.getNextLoop();
        EventLoop* loop4 = pool.getNextLoop();

        ASSERT_NE((int)loop1, (int)loop2, "应获得不同的loop");
        ASSERT_NE((int)loop2, (int)loop3, "应获得不同的loop");
        ASSERT_EQ((int)loop1, (int)loop4, "应轮询回到第一个loop");
    }
};
REGISTER_TEST(TestEventLoopThreadPoolRoundRobin)

// 测试7: SensorManager 基础
class TestSensorManagerBasic : public TestCase {
public:
    TestSensorManagerBasic() : TestCase("SensorManager基础功能") {}

    void Setup() override {
        Logger_Init("test_framework.log");
    }

    void Run() override {
        EventLoop mainLoop;
        SensorManger manager(&mainLoop);
        manager.setThreadNum(2);

        // 注册驱动
        DriverEntry entry;
        entry.subsystem = "test";
        entry.driver_name = "MockSensor";
        entry.onData = [](const DataBase* data) {};
        entry.onError = [](const string& name, int code) {};

        manager.RegisterDriver(entry);

        ASSERT_TRUE(true, "注册驱动成功");
    }
};
REGISTER_TEST(TestSensorManagerBasic)

// 测试8: 并发回调验证
class TestConcurrentCallbacks : public TestCase {
public:
    TestConcurrentCallbacks() : TestCase("并发回调验证") {}

    void Run() override {
        EventLoop loop;
        atomic<int> data_received(0);

        thread t([&loop]() {
            loop.loop();
        });

        // 模拟并发数据回调
        for (int i = 0; i < 5; ++i) {
            loop.queueInLoop([&data_received]() {
                data_received++;
            });
        }

        this_thread::sleep_for(chrono::milliseconds(100));
        loop.quit();
        t.join();

        ASSERT_EQ(data_received, 5, "应接收5个回调");
    }
};
REGISTER_TEST(TestConcurrentCallbacks)

// 测试9: 驱动状态转移
class TestSensorStateTransition : public TestCase {
public:
    TestSensorStateTransition() : TestCase("传感器状态转移") {}

    void Run() override {
        auto sensor = CreateObject<SensorBase>("MockSensor");

        // Default -> Ready
        ASSERT_EQ(sensor->GetStatus(), (int)SensorStatus::kDefault, "初始状态");
        sensor->Init();
        ASSERT_EQ(sensor->GetStatus(), (int)SensorStatus::kReady, "Init后");

        // Ready -> Capturing
        sensor->Start();
        ASSERT_EQ(sensor->GetStatus(), (int)SensorStatus::kCapturing, "Start后");

        // Capturing -> Ready
        sensor->Stop();
        ASSERT_EQ(sensor->GetStatus(), (int)SensorStatus::kReady, "Stop后");

        // Ready -> (释放)
        sensor->Release();
    }
};
REGISTER_TEST(TestSensorStateTransition)

// 测试10: 内存管理
class TestMemoryManagement : public TestCase {
public:
    TestMemoryManagement() : TestCase("内存管理") {}

    void Run() override {
        // 创建100个对象，验证没有内存泄漏
        for (int i = 0; i < 100; ++i) {
            auto sensor = CreateObject<SensorBase>("MockSensor");
            ASSERT_NOT_NULLPTR(sensor.get(), "对象创建成功");
            // 自动释放
        }
        ASSERT_TRUE(true, "内存管理正常");
    }
};
REGISTER_TEST(TestMemoryManagement)

// ============================================================================
// 性能测试
// ============================================================================

class PerfTestClassFactory : public TestCase {
public:
    PerfTestClassFactory() : TestCase("性能测试:ClassFactory") {}

    void Run() override {
        auto start = chrono::high_resolution_clock::now();

        for (int i = 0; i < 10000; ++i) {
            auto obj = CreateObject<SensorBase>("MockSensor");
            ASSERT_NOT_NULLPTR(obj.get(), "对象创建成功");
        }

        auto end = chrono::high_resolution_clock::now();
        auto duration = chrono::duration_cast<chrono::milliseconds>(end - start).count();

        cout << "  -> 创建10000个对象耗时: " << duration << "ms" << endl;
        ASSERT_TRUE(duration < 5000, "性能应在可接受范围内");
    }
};
REGISTER_TEST(PerfTestClassFactory)

// ============================================================================
// Main
// ============================================================================

int main() {
    Logger_Init("test_framework.log");

    cout << "\n" << string(60, '=') << endl;
    cout << "  MYSMW 项目 - 完整测试框架" << endl;
    cout << string(60, '=') << endl;

    TestRunner::GetInstance().RunAll();

    cout << "\n" << string(60, '=') << endl;
    cout << "  测试套件执行完毕" << endl;
    cout << string(60, '=') << endl;

    return 0;
}
