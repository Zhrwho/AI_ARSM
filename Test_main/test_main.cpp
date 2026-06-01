/**
 * @brief: 测试真实传感器插拔
 * 测试内容：
 * 1. MainLoop 监听热插拔事件
 * 2. 传感器插入 → 分配给 SubLoop
 * 3. SubLoop 通过 select 监听传感器 fd
 * 4. 传感器移除 → 从 SubLoop 注销
 */

#include <csignal>
#include <unistd.h>

#include "../Smw/Sensor_manager.h"
#include "../Smw/utility/Logger.h"

static SensorManger* g_manager = nullptr;

static void signal_handler(int sig) {
    log_info("\n收到信号 %d, 正在退出...", sig);
    if (g_manager) g_manager->Stop();
}

int main() {
    Logger_Init("test_main.log");
    printf("============ 传感器热插拔测试 =========\n\n");

    // 1. 创建管理器
    SensorManger manager;
    g_manager = &manager;

    // 2. 设置 SubLoop 线程数
    manager.setThreadNum(3);

    // 3. 注册 WT61 驱动（IMU 传感器）
    DriverEntry entry1;
    entry1.subsystem = "usb";
    entry1.vendor_id = "05c8";
    entry1.product_id = "03eb";
    entry1.driver_name = "WT61";
    entry1.onData = [](const DataBase* data) {
        const ImuData* idata = dynamic_cast<const ImuData*>(data);
        if (idata) {
            printf("\n[WT61] frame=%d pitch=%.2f roll=%.2f yaw=%.2f (线程: %zu)\n",
                   idata->frameId, idata->pitch, idata->roll, idata->yaw,
                   std::hash<std::thread::id>{}(std::this_thread::get_id()));
        }
    };
    entry1.onError = [](const std::string& name, int code) {
        printf("[WT61] 错误: %s, code=%d\n", name.c_str(), code);
    };
    manager.RegisterDriver(entry1);

    // 4. 注册 BRT38 驱动（速度传感器）
    DriverEntry entry2;
    entry2.subsystem = "tty";
    entry2.vendor_id = "1a86";
    entry2.product_id = "7523";
    entry2.driver_name = "BRT38";
    entry2.onData = [](const DataBase* data) {
        const SpeedData* sdata = dynamic_cast<const SpeedData*>(data);
        if (sdata) {
            printf("\n[BRT38] frame=%d n=%.4f (线程: %zu)\n",
                   sdata->frameId, sdata->n,
                   std::hash<std::thread::id>{}(std::this_thread::get_id()));
        }
    };
    entry2.onError = [](const std::string& name, int code) {
        printf("[BRT38] 错误: %s, code=%d\n", name.c_str(), code);
    };
    manager.RegisterDriver(entry2);

    // 5. 信号处理
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // 6. 启动
    if (manager.Start() != 0) {
        printf("[错误] 管理器启动失败\n");
        return 1;
    }

    // 7. 主循环
    printf("\n------ 等待传感器插入... (Ctrl+C 退出) ------\n\n");

    while (true) {
        sleep(5);
        manager.ListSensors();
    }

    return 0;
}