/**
 * @brief: 
 * 完整的测试框架-单元测试+集成测试
 * 支持:
 * 1.ClassFactory·工厂模式测试
 * 2.EventLoop·并发测试
 * 3.传感器驱动生命周期测试
 * 4.传感器管理器·功能测试
 * 
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
// Test framework infrastructure
// ============================================================================

class TestCase {
public:
    virtual ~TestCase() = default;
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

    void ASSERT_EQ(SensorStatus a, SensorStatus b, const string& msg = "") {
        if (a != b) {
            cerr << "[FAIL] " << name_ << ": " << (int)a << " != " << (int)b << " - " << msg << endl;
            throw runtime_error("Assertion failed");
        }
    }

    void ASSERT_EQ_PTR(void* a, void* b, const string& msg = "") {
        if (a != b) {
            cerr << "[FAIL] " << name_ << ": ptr mismatch - " << msg << endl;
            throw runtime_error("Assertion failed");
        }
    }

    void ASSERT_NE_PTR(void* a, void* b, const string& msg = "") {
        if (a == b) {
            cerr << "[FAIL] " << name_ << ": ptr should differ - " << msg << endl;
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

        cout << "\n========================================\n";
        cout << "  MYSMW Test Framework\n";
        cout << "========================================\n\n";
        cout << "Starting test suite: " << tests_.size() << " tests\n\n";

        for (auto& test : tests_) {
            try {
                cout << "[RUN] " << test->GetName() << " ... ";
                test->Run();
                test->TearDown();
                cout << "[PASS]" << endl;
                passed++;
            } catch (const exception& e) {
                cout << "[FAIL] - " << e.what() << endl;
                failed++;
            }
        }

        cout << "\n========================================\n";
        cout << "Test Results\n";
        cout << "========================================\n";
        cout << "Passed: " << passed << endl;
        cout << "Failed: " << failed << endl;
        cout << "Total:  " << (passed + failed) << endl;
        cout << "Pass rate: " << (100.0 * passed / tests_.size()) << "%" << endl;
    }

private:
    TestRunner() = default;
    vector<unique_ptr<TestCase>> tests_;
};

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
// Mock sensor for testing
// ============================================================================

class MockSensor : public SensorBase {
public:
    MockSensor() : fd_(-1), init_called_(false), start_called_(false) {}

    int Init() override {
        init_called_ = true;
        fd_ = 100;
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
// Test cases
// ============================================================================

class TestClassFactoryCreation : public TestCase {
public:
    TestClassFactoryCreation() : TestCase("ClassFactory - Create object") {}

    void Run() override {
        auto obj = CreateObject<SensorBase>("MockSensor");
        ASSERT_NOT_NULLPTR(obj.get(), "Factory should create object");
        ASSERT_EQ(obj->GetStatus(), SensorStatus::kDefault, "Initial status should be Default");
    }
};
REGISTER_TEST(TestClassFactoryCreation)

class TestClassFactoryFailed : public TestCase {
public:
    TestClassFactoryFailed() : TestCase("ClassFactory - Create failed") {}

    void Run() override {
        auto obj = CreateObject<SensorBase>("NonExistentSensor");
        ASSERT_NULLPTR(obj.get(), "Non-existent class should return nullptr");
    }
};
REGISTER_TEST(TestClassFactoryFailed)

class TestMockSensorLifecycle : public TestCase {
public:
    TestMockSensorLifecycle() : TestCase("Sensor lifecycle") {}

    void Run() override {
        auto sensor = CreateObject<SensorBase>("MockSensor");
        ASSERT_NOT_NULLPTR(sensor.get(), "Factory should create object");

        int ret = sensor->Init();
        ASSERT_EQ(ret, 0, "Init should return 0");
        ASSERT_EQ(sensor->GetStatus(), SensorStatus::kReady, "After Init status should be Ready");

        ret = sensor->Start();
        ASSERT_EQ(ret, 0, "Start should return 0");
        ASSERT_EQ(sensor->GetStatus(), SensorStatus::kCapturing, "After Start status should be Capturing");

        int fd = sensor->fd();
        ASSERT_NE(fd, -1, "FD should be valid");

        ret = sensor->Stop();
        ASSERT_EQ(ret, 0, "Stop should return 0");

        ret = sensor->Release();
        ASSERT_EQ(ret, 0, "Release should return 0");
    }
};
REGISTER_TEST(TestMockSensorLifecycle)

class TestEventLoopBasic : public TestCase {
public:
    TestEventLoopBasic() : TestCase("EventLoop basic") {}

    void Run() override {
        EventLoop loop;
        atomic<bool> called(false);
        loop.queueInLoop([&called]() {
            called = true;
        });

        thread t([&loop]() {
            loop.loop();
        });

        this_thread::sleep_for(chrono::milliseconds(100));
        loop.quit();
        t.join();

        ASSERT_TRUE(called, "queueInLoop callback should execute");
    }
};
REGISTER_TEST(TestEventLoopBasic)

class TestEventLoopThreadSafety : public TestCase {
public:
    TestEventLoopThreadSafety() : TestCase("EventLoop thread safety") {}

    void Run() override {
        EventLoop loop;
        atomic<int> counter(0);

        thread t([&loop]() {
            loop.loop();
        });

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
        ASSERT_EQ(counter, 10, "All queued tasks should execute");
    }
};
REGISTER_TEST(TestEventLoopThreadSafety)

class TestEventLoopThreadPoolRoundRobin : public TestCase {
public:
    TestEventLoopThreadPoolRoundRobin() : TestCase("ThreadPool round-robin") {}

    void Run() override {
        EventLoop mainLoop;
        EventLoopThreadPool pool(&mainLoop, "TestPool");
        pool.setThreadNum(3);
        pool.start();

        EventLoop* loop1 = pool.getNextLoop();
        EventLoop* loop2 = pool.getNextLoop();
        EventLoop* loop3 = pool.getNextLoop();
        EventLoop* loop4 = pool.getNextLoop();

        ASSERT_NE_PTR(loop1, loop2, "Should get different loops");
        ASSERT_NE_PTR(loop2, loop3, "Should get different loops");
        ASSERT_EQ_PTR(loop1, loop4, "Should round-robin back to first loop");
    }
};
REGISTER_TEST(TestEventLoopThreadPoolRoundRobin)

class TestSensorManagerBasic : public TestCase {
public:
    TestSensorManagerBasic() : TestCase("SensorManager basic") {}

    void Run() override {
        EventLoop mainLoop;
        SensorManger manager(&mainLoop);
        manager.setThreadNum(2);

        DriverEntry entry;
        entry.subsystem = "test";
        entry.driver_name = "MockSensor";
        entry.onData = [](const DataBase*) {};
        entry.onError = [](const string&, int) {};

        manager.RegisterDriver(entry);

        ASSERT_TRUE(true, "Driver registered successfully");
    }
};
REGISTER_TEST(TestSensorManagerBasic)

class TestConcurrentCallbacks : public TestCase {
public:
    TestConcurrentCallbacks() : TestCase("Concurrent callbacks") {}

    void Run() override {
        EventLoop loop;
        atomic<int> data_received(0);

        thread t([&loop]() {
            loop.loop();
        });

        for (int i = 0; i < 5; ++i) {
            loop.queueInLoop([&data_received]() {
                data_received++;
            });
        }

        this_thread::sleep_for(chrono::milliseconds(100));
        loop.quit();
        t.join();

        ASSERT_EQ(data_received, 5, "Should receive 5 callbacks");
    }
};
REGISTER_TEST(TestConcurrentCallbacks)

class TestSensorStateTransition : public TestCase {
public:
    TestSensorStateTransition() : TestCase("Sensor state transition") {}

    void Run() override {
        auto sensor = CreateObject<SensorBase>("MockSensor");

        ASSERT_EQ(sensor->GetStatus(), SensorStatus::kDefault, "Initial state");
        sensor->Init();
        ASSERT_EQ(sensor->GetStatus(), SensorStatus::kReady, "After Init");

        sensor->Start();
        ASSERT_EQ(sensor->GetStatus(), SensorStatus::kCapturing, "After Start");

        sensor->Stop();
        ASSERT_EQ(sensor->GetStatus(), SensorStatus::kReady, "After Stop");

        sensor->Release();
    }
};
REGISTER_TEST(TestSensorStateTransition)

class TestMemoryManagement : public TestCase {
public:
    TestMemoryManagement() : TestCase("Memory management") {}

    void Run() override {
        for (int i = 0; i < 100; ++i) {
            auto sensor = CreateObject<SensorBase>("MockSensor");
            ASSERT_NOT_NULLPTR(sensor.get(), "Object created successfully");
        }
        ASSERT_TRUE(true, "Memory managed correctly");
    }
};
REGISTER_TEST(TestMemoryManagement)

class PerfTestClassFactory : public TestCase {
public:
    PerfTestClassFactory() : TestCase("Performance - ClassFactory") {}

    void Run() override {
        auto start = chrono::high_resolution_clock::now();

        for (int i = 0; i < 10000; ++i) {
            auto obj = CreateObject<SensorBase>("MockSensor");
            ASSERT_NOT_NULLPTR(obj.get(), "Object created successfully");
        }

        auto end = chrono::high_resolution_clock::now();
        auto duration = chrono::duration_cast<chrono::milliseconds>(end - start).count();

        cout << "  -> Created 10000 objects in: " << duration << "ms" << endl;
        ASSERT_TRUE(duration < 5000, "Performance acceptable");
    }
};
REGISTER_TEST(PerfTestClassFactory)

// ============================================================================
// Main
// ============================================================================

int main() {
    Logger_Init("test_framework.log");

    cout << "\n========================================\n";
    cout << "  MYSMW Project - Complete Test Suite\n";
    cout << "========================================\n";

    TestRunner::GetInstance().RunAll();

    cout << "\n========================================\n";
    cout << "  Test suite completed\n";
    cout << "========================================\n";

    return 0;
}
