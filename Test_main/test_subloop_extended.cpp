/**
 * @brief SubLoop 扩展功能测试 - 验证 SubLoop 实际运行和并发性能
 */

#include <iostream>
#include <atomic>
#include <thread>
#include <chrono>
#include <memory>
#include <vector>
#include <unistd.h>
#include <sys/syscall.h>

#include "../Smw/utility/EventLoop.h"
#include "../Smw/utility/EventLoopThread.h"
#include "../Smw/utility/EventLoopThreadPool.h"
#include "../Smw/utility/Logger.h"

using namespace std;

// 获取当前线程 ID
static pid_t GetThreadId() {
    return static_cast<pid_t>(syscall(SYS_gettid));
}

// ============================================================================
// Test 1: SubLoop 能否在不同线程独立运行
// ============================================================================

class TestSubLoopIndependentExecution {
public:
    TestSubLoopIndependentExecution() : name_("SubLoop 独立运行验证") {}

    string GetName() const { return name_; }

    void Run() {
        EventLoop mainLoop;
        EventLoopThreadPool pool(&mainLoop, "TestPool");
        pool.setThreadNum(2);
        pool.start();

        // 获取 2 个 SubLoop
        EventLoop* subLoop1 = pool.getNextLoop();
        EventLoop* subLoop2 = pool.getNextLoop();

        // 验证 SubLoop 在不同线程
        pid_t tid1 = 0, tid2 = 0;
        atomic<int> counter1(0), counter2(0);

        // 投递任务到 SubLoop1
        subLoop1->queueInLoop([&tid1, &counter1]() {
            tid1 = GetThreadId();
            counter1++;
        });

        // 投递任务到 SubLoop2
        subLoop2->queueInLoop([&tid2, &counter2]() {
            tid2 = GetThreadId();
            counter2++;
        });

        this_thread::sleep_for(chrono::milliseconds(100));

        if (tid1 != 0 && tid2 != 0 && tid1 != tid2 && counter1 == 1 && counter2 == 1) {
            cout << "  ✅ PASS - SubLoop 独立运行成功 (TID1=" << tid1 << ", TID2=" << tid2 << ")" << endl;
        } else {
            cout << "  ❌ FAIL - SubLoop 未正确分离 (TID1=" << tid1 << ", TID2=" << tid2
                 << ", C1=" << counter1 << ", C2=" << counter2 << ")" << endl;
        }
    }

private:
    string name_;
};

// ============================================================================
// Test 2: SubLoop 能否处理管道 fd（模拟 I/O）
// ============================================================================

class TestSubLoopFdHandling {
public:
    TestSubLoopFdHandling() : name_("SubLoop fd 事件处理") {}

    string GetName() const { return name_; }

    void Run() {
        // 创建管道
        int pipe_fd[2];
        if (::pipe(pipe_fd) < 0) {
            cout << "  ❌ FAIL - 创建管道失败" << endl;
            return;
        }

        EventLoopThread thread;
        EventLoop* subLoop = thread.startLoop();

        atomic<int> data_received(0);

        // 在 SubLoop 中执行 I/O 操作
        subLoop->queueInLoop([pipe_fd, &data_received]() {
            const char* test_data = "TEST";
            if (::write(pipe_fd[1], test_data, 4) > 0) {
                char buffer[10] = {0};
                int n = ::read(pipe_fd[0], buffer, sizeof(buffer) - 1);
                if (n > 0) {
                    data_received++;
                }
            }
        });

        this_thread::sleep_for(chrono::milliseconds(100));

        if (data_received == 1) {
            cout << "  ✅ PASS - SubLoop fd 处理成功" << endl;
        } else {
            cout << "  ❌ FAIL - SubLoop fd 处理失败 (received=" << data_received << ")" << endl;
        }

        subLoop->quit();
        ::close(pipe_fd[0]);
        ::close(pipe_fd[1]);
    }

private:
    string name_;
};

// ============================================================================
// Test 3: 多个 SubLoop 并发处理不同数据
// ============================================================================

class TestMultipleSubLoopConcurrency {
public:
    TestMultipleSubLoopConcurrency() : name_("多 SubLoop 并发处理") {}

    string GetName() const { return name_; }

    void Run() {
        EventLoop mainLoop;
        EventLoopThreadPool pool(&mainLoop, "TestPool");
        pool.setThreadNum(3);
        pool.start();

        vector<int> results(3, -1);
        vector<EventLoop*> loops;

        // 获取 3 个 SubLoop
        for (int i = 0; i < 3; ++i) {
            loops.push_back(pool.getNextLoop());
        }

        // 在每个 SubLoop 上执行独立任务
        for (int i = 0; i < 3; ++i) {
            int idx = i;
            loops[i]->queueInLoop([&results, idx]() {
                // 模拟轻微延迟，验证并发
                this_thread::sleep_for(chrono::milliseconds(10));
                results[idx] = idx + 100;
            });
        }

        this_thread::sleep_for(chrono::milliseconds(150));

        bool all_correct = (results[0] == 100 && results[1] == 101 && results[2] == 102);
        if (all_correct) {
            cout << "  ✅ PASS - 多 SubLoop 并发成功 (results: " << results[0]
                 << " " << results[1] << " " << results[2] << ")" << endl;
        } else {
            cout << "  ❌ FAIL - 多 SubLoop 并发失败 (results: " << results[0]
                 << " " << results[1] << " " << results[2] << ")" << endl;
        }
    }

private:
    string name_;
};

// ============================================================================
// Test 4: SubLoop 能否安全接收来自其他线程的任务
// ============================================================================

class TestMainLoopToSubLoopCommunication {
public:
    TestMainLoopToSubLoopCommunication() : name_("MainLoop → SubLoop 通信") {}

    string GetName() const { return name_; }

    void Run() {
        EventLoop mainLoop;
        EventLoopThreadPool pool(&mainLoop, "TestPool");
        pool.setThreadNum(1);
        pool.start();

        EventLoop* subLoop = pool.getNextLoop();
        atomic<int> task_count(0);

        // 投递 10 个任务到 SubLoop
        for (int i = 0; i < 10; ++i) {
            subLoop->queueInLoop([&task_count]() {
                task_count++;
            });
        }

        this_thread::sleep_for(chrono::milliseconds(200));

        if (task_count == 10) {
            cout << "  ✅ PASS - MainLoop → SubLoop 通信成功 (tasks executed: " << task_count << ")" << endl;
        } else {
            cout << "  ❌ FAIL - 部分任务未执行 (tasks executed: " << task_count << " / 10)" << endl;
        }
    }

private:
    string name_;
};

// ============================================================================
// Test 5: SubLoop 线程池轮询分配的正确性
// ============================================================================

class TestSubLoopRoundRobinAllocation {
public:
    TestSubLoopRoundRobinAllocation() : name_("SubLoop 轮询分配") {}

    string GetName() const { return name_; }

    void Run() {
        EventLoop mainLoop;
        EventLoopThreadPool pool(&mainLoop, "TestPool");
        pool.setThreadNum(3);
        pool.start();

        vector<EventLoop*> loops;

        // 获取 7 个 loop，应该循环利用前 3 个
        for (int i = 0; i < 7; ++i) {
            loops.push_back(pool.getNextLoop());
        }

        // 验证轮询逻辑：
        // 0: Loop1, 1: Loop2, 2: Loop3, 3: Loop1, 4: Loop2, 5: Loop3, 6: Loop1
        bool pattern_correct =
            (loops[0] == loops[3]) &&  // 0 和 3 相同
            (loops[3] == loops[6]) &&  // 3 和 6 相同
            (loops[1] == loops[4]) &&  // 1 和 4 相同
            (loops[2] == loops[5]) &&  // 2 和 5 相同
            (loops[0] != loops[1]) &&  // 不同的 loop 指针不同
            (loops[1] != loops[2]);    // 不同的 loop 指针不同

        if (pattern_correct) {
            cout << "  ✅ PASS - SubLoop 轮询分配正确" << endl;
        } else {
            cout << "  ❌ FAIL - SubLoop 轮询分配异常" << endl;
        }
    }

private:
    string name_;
};

// ============================================================================
// Test 6: SubLoop 处理高并发任务投递
// ============================================================================

class TestSubLoopHighConcurrency {
public:
    TestSubLoopHighConcurrency() : name_("SubLoop 高并发处理") {}

    string GetName() const { return name_; }

    void Run() {
        EventLoop mainLoop;
        EventLoopThreadPool pool(&mainLoop, "TestPool");
        pool.setThreadNum(2);
        pool.start();

        EventLoop* subLoop = pool.getNextLoop();
        atomic<int> counter(0);

        // 从多个线程并发投递任务
        vector<thread> threads;
        const int threads_count = 5;
        const int tasks_per_thread = 20;

        for (int i = 0; i < threads_count; ++i) {
            threads.emplace_back([&]() {
                for (int j = 0; j < tasks_per_thread; ++j) {
                    subLoop->queueInLoop([&counter]() {
                        counter++;
                    });
                }
            });
        }

        // 等待所有线程投递完成
        for (auto& t : threads) {
            t.join();
        }

        // 等待所有任务执行
        this_thread::sleep_for(chrono::milliseconds(500));

        int expected = threads_count * tasks_per_thread;
        if (counter == expected) {
            cout << "  ✅ PASS - SubLoop 高并发处理成功 (tasks: " << counter << "/" << expected << ")" << endl;
        } else {
            cout << "  ❌ FAIL - SubLoop 高并发处理失败 (tasks: " << counter << "/" << expected << ")" << endl;
        }
    }

private:
    string name_;
};

// ============================================================================
// Test 7: SubLoop 内务队列容量和性能
// ============================================================================

class TestSubLoopPerformance {
public:
    TestSubLoopPerformance() : name_("SubLoop 性能测试") {}

    string GetName() const { return name_; }

    void Run() {
        EventLoop mainLoop;
        EventLoopThreadPool pool(&mainLoop, "TestPool");
        pool.setThreadNum(1);
        pool.start();

        EventLoop* subLoop = pool.getNextLoop();
        atomic<int> counter(0);

        auto start = chrono::high_resolution_clock::now();

        // 投递 1000 个任务
        const int task_count = 1000;
        for (int i = 0; i < task_count; ++i) {
            subLoop->queueInLoop([&counter]() {
                counter++;
            });
        }

        // 等待执行
        this_thread::sleep_for(chrono::milliseconds(300));

        auto end = chrono::high_resolution_clock::now();
        auto duration = chrono::duration_cast<chrono::milliseconds>(end - start).count();

        if (counter == task_count) {
            cout << "  ✅ PASS - SubLoop 性能测试成功" << endl;
            cout << "    投递并执行 " << task_count << " 个任务耗时: " << duration << "ms" << endl;
            cout << "    平均每个任务: " << (double)duration / task_count << "ms" << endl;
        } else {
            cout << "  ❌ FAIL - 任务执行不完整 (executed: " << counter << "/" << task_count << ")" << endl;
        }
    }

private:
    string name_;
};

// ============================================================================
// Main
// ============================================================================

int main() {
    Logger_Init("test_subloop_extended.log");

    cout << "\n" << string(70, '=') << endl;
    cout << "  MYSMW - SubLoop 扩展功能测试\n";
    cout << "  测试 SubLoop 的实际运行、并发、性能等功能\n";
    cout << string(70, '=') << endl << endl;

    int passed = 0, failed = 0;

    vector<function<void()>> tests = {
        [&](){ TestSubLoopIndependentExecution test; test.Run(); passed++; },
        [&](){ TestSubLoopFdHandling test; test.Run(); passed++; },
        [&](){ TestMultipleSubLoopConcurrency test; test.Run(); passed++; },
        [&](){ TestMainLoopToSubLoopCommunication test; test.Run(); passed++; },
        [&](){ TestSubLoopRoundRobinAllocation test; test.Run(); passed++; },
        [&](){ TestSubLoopHighConcurrency test; test.Run(); passed++; },
        [&](){ TestSubLoopPerformance test; test.Run(); passed++; },
    };

    cout << "[RUN TEST 1] " << TestSubLoopIndependentExecution().GetName() << endl;
    TestSubLoopIndependentExecution test1; test1.Run();

    cout << "\n[RUN TEST 2] " << TestSubLoopFdHandling().GetName() << endl;
    TestSubLoopFdHandling test2; test2.Run();

    cout << "\n[RUN TEST 3] " << TestMultipleSubLoopConcurrency().GetName() << endl;
    TestMultipleSubLoopConcurrency test3; test3.Run();

    cout << "\n[RUN TEST 4] " << TestMainLoopToSubLoopCommunication().GetName() << endl;
    TestMainLoopToSubLoopCommunication test4; test4.Run();

    cout << "\n[RUN TEST 5] " << TestSubLoopRoundRobinAllocation().GetName() << endl;
    TestSubLoopRoundRobinAllocation test5; test5.Run();

    cout << "\n[RUN TEST 6] " << TestSubLoopHighConcurrency().GetName() << endl;
    TestSubLoopHighConcurrency test6; test6.Run();

    cout << "\n[RUN TEST 7] " << TestSubLoopPerformance().GetName() << endl;
    TestSubLoopPerformance test7; test7.Run();

    cout << "\n" << string(70, '=') << endl;
    cout << "  测试完成 (7 个 SubLoop 功能测试)\n";
    cout << string(70, '=') << endl;

    return 0;
}
