/**
 * @file main.cpp
 * @brief MYSMW 项目 - 主程序（INI 配置驱动）
 *
 * @description
 * 传感器元信息从 sensor_config.ini 读取，onData 回调逻辑在代码注册表中维护。
 * 添加已有类型的传感器：只改 INI 文件
 * 添加全新类型的传感器：写驱动 + 工厂注册 + INI 加一行 + 注册表加回调
 */

#include <csignal>
#include <unistd.h>
#include <thread>
#include <chrono>
#include <ctime>
#include <sys/syscall.h>
#include <unordered_map>

#include "../Smw/Sensor_manager.h"
#include "../Smw/utility/Logger.h"
#include "../Smw/utility/ini_file.h"

// 全局指针，用于信号处理
static SensorManger* g_manager = nullptr;

static void signal_handler(int sig) {
    log_info("\n\n========== 收到信号 %d，正在优雅关闭... ==========", sig);
    if (g_manager) {
        log_info("停止 SensorManager 和所有 SubLoop...");
        g_manager->Stop();
    }
}

static pid_t GetCurrentThreadId() {
    return static_cast<pid_t>(syscall(SYS_gettid));
}

static std::string GetTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    char buf[64];
    strftime(buf, sizeof(buf), "%H:%M:%S", localtime(&time));
    return std::string(buf);
}

// ==================== 回调注册表 ====================
// 驱动名 → 数据回调，驱动名 → 错误回调
static std::unordered_map<std::string, OnSensorData>  g_data_callbacks;
static std::unordered_map<std::string, OnSensorError> g_error_callbacks;

void InitCallbackRegistry() {
    // ── WT61 IMU 传感器 ──
    g_data_callbacks["WT61"] = [](const DataBase* data) {
        const ImuData* idata = dynamic_cast<const ImuData*>(data);
        if (idata) {
            pid_t tid = GetCurrentThreadId();
            printf("[%s] [WT61] 帧%d | Pitch:%.2f° Roll:%.2f° Yaw:%.2f° | TID:%d\n",
                   GetTimestamp().c_str(),
                   idata->frameId,
                   idata->pitch, idata->roll, idata->yaw,
                   tid);
        }
    };
    g_error_callbacks["WT61"] = [](const std::string& name, int code) {
        log_error("[WT61] 错误: %s (code=%d)", name.c_str(), code);
    };

    // ── BRT38 速度传感器 ──
    g_data_callbacks["BRT38"] = [](const DataBase* data) {
        const SpeedData* sdata = dynamic_cast<const SpeedData*>(data);
        if (sdata) {
            pid_t tid = GetCurrentThreadId();
            printf("[%s] [BRT38] 帧%d | Speed:%.4f m/s | TID:%d\n",
                   GetTimestamp().c_str(),
                   sdata->frameId, sdata->n,
                   tid);
        }
    };
    g_error_callbacks["BRT38"] = [](const std::string& name, int code) {
        log_error("[BRT38] 错误: %s (code=%d)", name.c_str(), code);
    };

    // ── OID_R5008D 速度传感器 ──
    g_data_callbacks["OID_R5008D"] = [](const DataBase* data) {
        const SpeedData* sdata = dynamic_cast<const SpeedData*>(data);
        if (sdata) {
            pid_t tid = GetCurrentThreadId();
            printf("[%s] [OID_R5008D] 帧%d | Speed:%.4f m/s | TID:%d\n",
                   GetTimestamp().c_str(),
                   sdata->frameId, sdata->n,
                   tid);
        }
    };
    g_error_callbacks["OID_R5008D"] = [](const std::string& name, int code) {
        log_error("[OID_R5008D] 错误: %s (code=%d)", name.c_str(), code);
    };
}

// ==================== 从 INI 遍历注册驱动 ====================
int RegisterDriversFromIni(SensorManger& manager) {
    IniFile& ini = IniFile::instance();
    if (!ini.load("sensor_config.ini")) {
        printf("⚠️  未找到 sensor_config.ini，使用默认配置\n");
        return 0;
    }

    printf("══════════════════════════════════════════════════════════════\n");
    printf("  从 sensor_config.ini 加载配置\n");
    printf("══════════════════════════════════════════════════════════════\n");

    auto sections = ini.GetSectionNames();
    int count = 0;

    for (const auto& section : sections) {
        std::string enabled = ini.get(section, "enabled");
        if(enabled != "true")
        {
            printf("[%s] 未启用，跳过\n", section.c_str());
            continue;
        }

        std::string driver_name = ini.get(section, "driver_name");

        DriverEntry entry;
        entry.subsystem  = (std::string)ini.get(section, "subsystem");
        entry.vendor_id  = (std::string)ini.get(section, "vendor_id");
        entry.product_id = (std::string)ini.get(section, "product_id");
        entry.driver_name = driver_name;

        // 从注册表取回调
        auto data_it = g_data_callbacks.find(driver_name);
        auto err_it  = g_error_callbacks.find(driver_name);

        if (data_it != g_data_callbacks.end()) {
            entry.onData = data_it->second;
        } else {
            printf("  ⚠️  [%s] 无 onData 回调注册\n", driver_name.c_str());
        }
        if (err_it != g_error_callbacks.end()) {
            entry.onError = err_it->second;
        } else {
            entry.onError = [driver_name](const std::string& n, int c) {
                log_error("[%s] 错误: %s (code=%d)", n.c_str(), c);
            };
        }

        manager.RegisterDriver(entry);
        printf("  ✓ [%s] %s (subsystem=%s, VID=%s, PID=%s)\n",
               section.c_str(),
               driver_name.c_str(),
               entry.subsystem.c_str(),
               entry.vendor_id.empty() ? "*" : entry.vendor_id.c_str(),
               entry.product_id.empty() ? "*" : entry.product_id.c_str());
        count++;
    }

    printf("══════════════════════════════════════════════════════════════\n");
    log_info("从 INI 注册了 %d 个驱动", count);
    return count;
}

// ==================== main ====================
int main() {
    Logger_Init("main.log");
    log_info("========== MYSMW 应用启动 ==========");

    // 1. 初始化回调注册表
    InitCallbackRegistry();

    // 2. 创建 MainLoop
    log_info("\n[初始化] 创建 MainLoop");
    EventLoop loop;

    // 3. 创建管理器，注入 MainLoop
    log_info("[初始化] 创建 SensorManager");
    SensorManger manager(&loop);
    g_manager = &manager;

    // 4. 配置 SubLoop 线程数
    manager.setThreadNum(3);
    printf("\n  📌 SubLoop 线程数: 3\n");

    // 5. 从 INI 读取配置，遍历注册
    RegisterDriversFromIni(manager);

    // 6. 信号处理
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // 7. 启动
    if (manager.Start() != 0) {
        printf("❌ 管理器启动失败！\n");
        return 1;
    }

    printf("\n------ 等待设备插入... (Ctrl+C 退出) ------\n\n");

    // 8. 主线程运行 MainLoop（阻塞）
    loop.loop();

    log_info("========== 程序退出 ==========");
    return 0;
}
