/**
 * @file main.cpp
 * @brief MYSMW 项目 - 主程序示例
 *
 * @description
 * 这是一个完整的 MYSMW 应用程序示例，展示如何：
 * 1. 初始化 SubLoop 线程池
 * 2. 注册传感器驱动
 * 3. 处理设备热插拔事件
 * 4. 在 SubLoop 中并发处理多个传感器的数据
 *
 * @usage
 * 编译（从 Test_main 目录）：
 *   g++ -o main main.cpp ../Smw/utility/*.cpp ../Smw/Sensor_manager.cpp \
 *       -std=c++17 -pthread -ludev
 *
 * 或者从项目根目录：
 *   g++ -o Test_main/main Test_main/main.cpp \
 *       Smw/utility/*.cpp Smw/Sensor_manager.cpp \
 *       -std=c++17 -pthread -ludev
 *
 * 运行（从 Test_main 目录）：
 *   ./main
 *
 * 退出：
 *   Ctrl+C
 *
 * @note
 * 用户需要修改的部分已用 [USER MODIFY] 标记
 * 请在这些地方添加自己的驱动和回调逻辑
 */

#include <csignal>
#include <unistd.h>
#include <thread>
#include <chrono>
#include <ctime>
#include <sys/syscall.h>

#include "../Smw/Sensor_manager.h"
#include "../Smw/utility/Logger.h"

// 全局指针，用于信号处理
static SensorManger* g_manager = nullptr;

/**
 * @brief 信号处理函数，优雅退出
 */
static void signal_handler(int sig) {
    log_info("\n\n========== 收到信号 %d，正在优雅关闭... ==========", sig);
    if (g_manager) {
        log_info("停止 SensorManager 和所有 SubLoop...");
        g_manager->Stop();
    }
}

/**
 * @brief 获取当前线程 ID（便于识别是哪个 SubLoop 线程）
 */
static pid_t GetCurrentThreadId() {
    return static_cast<pid_t>(syscall(SYS_gettid));
}

/**
 * @brief 获取当前时间戳（用于日志）
 */
static std::string GetTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    char buf[64];
    strftime(buf, sizeof(buf), "%H:%M:%S", localtime(&time));
    return std::string(buf);
}

/**
 * ============================================================================
 * 主程序
 * ============================================================================
 */
int main() {
    // ========================================================================
    // 第一步：初始化日志系统
    // ========================================================================
    Logger_Init("main.log");

    printf("\n");
    printf("╔════════════════════════════════════════════════════════════════════╗\n");
    printf("║                                                                    ║\n");
    printf("║          MYSMW - Sensor Management Middleware (SubLoop)           ║\n");
    printf("║                                                                    ║\n");
    printf("║  架构：MainLoop + SubLoop 线程池 + Reactor 事件驱动                 ║\n");
    printf("║  特性：设备热插拔监控、多驱动并发、高性能 I/O                     ║\n");
    printf("║                                                                    ║\n");
    printf("╚════════════════════════════════════════════════════════════════════╝\n");
    printf("\n");

    log_info("========== MYSMW 应用启动 ==========");
    log_info("日志文件: main.log");

    // ========================================================================
    // 第二步：创建主循环（MainLoop）
    // ========================================================================
    log_info("\n[初始化] 创建 MainLoop（主线程监听热插拔事件）");
    EventLoop loop;

    // ========================================================================
    // 第三步：创建传感器管理器（注入 MainLoop）
    // ========================================================================
    log_info("[初始化] 创建 SensorManager");
    SensorManger manager(&loop);
    g_manager = &manager;

    // ========================================================================
    // 第四步：配置 SubLoop 线程池
    // ========================================================================
    // [USER MODIFY] 修改这个数字以改变 SubLoop 线程数
    // 建议：传感器数量 = SubLoop 线程数（最优分配）
    const int SUBLOOP_THREAD_NUM = 3;

    log_info("[初始化] 配置 SubLoop 线程池: %d 个线程", SUBLOOP_THREAD_NUM);
    manager.setThreadNum(SUBLOOP_THREAD_NUM);
    printf("\n  📌 每个传感器将分配到一个 SubLoop 线程\n");
    printf("  📌 不同传感器的数据将并发处理\n");
    printf("  📌 MainLoop 只负责监听热插拔事件\n\n");

    // ========================================================================
    // 第五步：注册驱动（核心配置）
    // ========================================================================
    log_info("[初始化] 注册传感器驱动");
    printf("════════════════════════════════════════════════════════════════════\n");

    // ========================================================================
    // [USER MODIFY] 驱动 1：WT61 IMU 传感器
    // ========================================================================
    printf("\n✓ 驱动 1: WT61 IMU 传感器\n");
    DriverEntry entry1;
    entry1.subsystem = "usb";           // 子系统类型
    entry1.vendor_id = "05c8";          // USB VID (05c8:03eb)
    entry1.product_id = "03eb";         // USB PID
    entry1.driver_name = "WT61";        // 驱动类名（必须在工厂中注册）

    // 数据到达回调（在 SubLoop 线程中执行）
    entry1.onData = [](const DataBase* data) {
        const ImuData* idata = dynamic_cast<const ImuData*>(data);
        if (idata) {
            pid_t tid = GetCurrentThreadId();
            printf("[%s] [WT61] 帧%d | Pitch:%.2f° Roll:%.2f° Yaw:%.2f° | SubLoop线程TID:%d\n",
                   GetTimestamp().c_str(),
                   idata->frameId,
                   idata->pitch,
                   idata->roll,
                   idata->yaw,
                   tid);
        }
    };

    // 错误处理回调
    entry1.onError = [](const std::string& msg, int code) {
        log_error("[WT61] 错误: %s (code=%d)", msg.c_str(), code);
    };

    manager.RegisterDriver(entry1);
    log_info("  ✓ 已注册 WT61 驱动 (USB 05c8:03eb)");

    // ========================================================================
    // [USER MODIFY] 驱动 2：BRT38 速度传感器
    // ========================================================================
    printf("✓ 驱动 2: BRT38 速度传感器\n");
    DriverEntry entry2;
    entry2.subsystem = "tty";           // 串口子系统
    entry2.vendor_id = "1a86";          // USB VID (CH340 模块)
    entry2.product_id = "7523";         // USB PID
    entry2.driver_name = "BRT38";       // 驱动类名

    // 数据到达回调
    entry2.onData = [](const DataBase* data) {
        const SpeedData* sdata = dynamic_cast<const SpeedData*>(data);
        if (sdata) {
            pid_t tid = GetCurrentThreadId();
            printf("[%s] [BRT38] 帧%d | Speed:%.4f m/s | SubLoop线程TID:%d\n",
                   GetTimestamp().c_str(),
                   sdata->frameId,
                   sdata->n,
                   tid);
        }
    };

    // 错误处理回调
    entry2.onError = [](const std::string& msg, int code) {
        log_error("[BRT38] 错误: %s (code=%d)", msg.c_str(), code);
    };

    manager.RegisterDriver(entry2);
    log_info("  ✓ 已注册 BRT38 驱动 (TTY 1a86:7523)");

    // ========================================================================
    // [USER MODIFY] 驱动 3：添加更多驱动（示例）
    // ========================================================================
    // 如果需要添加更多驱动，按照上面的模式继续添加
    // DriverEntry entry3;
    // entry3.subsystem = "...";
    // entry3.driver_name = "...";
    // entry3.onData = [](const DataBase* data) { ... };
    // entry3.onError = [](const std::string& msg, int code) { ... };
    // manager.RegisterDriver(entry3);

    printf("════════════════════════════════════════════════════════════════════\n\n");

    // ========================================================================
    // 第六步：设置信号处理（Ctrl+C 优雅退出）
    // ========================================================================
    log_info("[初始化] 注册信号处理 (SIGINT, SIGTERM)");
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // ========================================================================
    // 第七步：启动 SensorManager
    // ========================================================================
    log_info("[初始化] 启动 SensorManager");
    printf("\n启动中...\n");
    printf("  1. 初始化 udev（设备监控）\n");
    printf("  2. 启动 SubLoop 线程池\n");
    printf("  3. 扫描已连接的设备\n");
    printf("  4. 注册 udev fd 到 MainLoop\n\n");

    if (manager.Start() != 0) {
        printf("❌ [错误] 管理器启动失败！\n");
        log_error("SensorManager 启动失败");
        return 1;
    }

    log_info("✅ SensorManager 启动成功");

    // ========================================================================
    // 第八步：运行 MainLoop（阻塞）
    // ========================================================================
    printf("╔════════════════════════════════════════════════════════════════════╗\n");
    printf("║                                                                    ║\n");
    printf("║  🟢 系统已启动，监控模式已激活                                      ║\n");
    printf("║                                                                    ║\n");
    printf("║  等待操作：                                                         ║\n");
    printf("║    • 插入 USB 传感器 (WT61 或 BRT38)                               ║\n");
    printf("║    • 传感器会被分配到 SubLoop 并开始采集                           ║\n");
    printf("║    • 数据会实时显示在下方                                          ║\n");
    printf("║                                                                    ║\n");
    printf("║  退出：按 Ctrl+C                                                   ║\n");
    printf("║                                                                    ║\n");
    printf("╚════════════════════════════════════════════════════════════════════╝\n");
    printf("\n");
    printf("────────────────────────────────────────────────────────────────────\n");
    printf("数据输出：\n");
    printf("────────────────────────────────────────────────────────────────────\n\n");

    log_info("========== MainLoop 运行中... ==========");
    log_info("等待传感器插入/移除事件");

    // 主循环：监听热插拔事件
    // 这个调用是阻塞的，直到 quit() 被调用（通过信号处理）
    loop.loop();

    printf("\n────────────────────────────────────────────────────────────────────\n");
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════════════╗\n");
    printf("║                                                                    ║\n");
    printf("║  🔴 系统已关闭                                                      ║\n");
    printf("║                                                                    ║\n");
    printf("║  清理进行中：                                                       ║\n");
    printf("║    • 停止所有 SubLoop 线程                                         ║\n");
    printf("║    • 关闭所有传感器连接                                            ║\n");
    printf("║    • 释放 udev 资源                                                ║\n");
    printf("║                                                                    ║\n");
    printf("╚════════════════════════════════════════════════════════════════════╝\n");

    log_info("========== 程序退出 ==========");
    log_info("所有资源已释放");

    return 0;
}

/**
 * ============================================================================
 * 高级用户指南
 * ============================================================================
 *
 * 1. 修改 SubLoop 线程数
 *    位置: SUBLOOP_THREAD_NUM
 *    说明: 一般设置为传感器数量，以获得最佳性能
 *
 * 2. 添加新的传感器驱动
 *    步骤:
 *    a. 在 DriverEntry 中定义新驱动的 USB VID/PID
 *    b. 指定 driver_name（必须与工厂中注册的驱动类名相同）
 *    c. 编写 onData 回调处理数据
 *    d. 编写 onError 回调处理错误
 *    e. 调用 manager.RegisterDriver(entryX)
 *
 * 3. 自定义数据处理
 *    在 onData 回调中：
 *    - 获取当前线程 ID：GetCurrentThreadId()
 *    - 确认是在哪个 SubLoop 中执行
 *    - 实现自己的业务逻辑（保存文件、网络传输等）
 *
 * 4. 错误处理
 *    在 onError 回调中处理异常情况
 *    常见错误：设备断开、读取失败等
 *
 * 5. 性能优化
 *    - 在 onData 中避免长时间阻塞
 *    - 如果需要耗时操作，使用 queueInLoop 投递到其他线程
 *    - 使用日志而不是 printf 获得更好的性能
 *
 * 6. 监控和调试
 *    - 查看 main.log 获取详细日志
 *    - 输出中的 "SubLoop线程TID" 帮助了解数据流向
 *    - 使用 strace 或 gdb 调试复杂问题
 *
 * ============================================================================
 */
