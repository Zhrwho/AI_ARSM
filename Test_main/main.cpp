#include <csignal> //信号处理
#include <unistd.h> //Linux/Unix系统接口头文件 如read等

/**
 * @brief:输入要监听的设备, 传入回调,执行函数操作
 * 输入需要监听的驱动, 通过 子系统\VID\PID来实现驱动匹配
 */

#include "../Smw/Sensor_manager.h"
#include "../Smw/utility/Logger.h"

static SensorManger* g_manager = nullptr;

/**
 * 信号处理函数,收到Linux信号时,安全关闭传感器管理器
 */
static void signal_handler(int sig) {
    log_info("\n收到信号 %d, 正在退出...", sig);
    if(g_manager) g_manager->Stop();
}

/* 传入需要监听的驱动(设备), 注入回调*/
int main() {
    /* 日志初始化,传入存储日志内容的文件名称 */
    Logger_Init("logger.log");
    printf("============ 传感器热插拔管理框架 =========\n\n");

    //1.创建主循环（主线程拥有）
    EventLoop loop;

    //2.创建管理器，注入 mainLoop
    SensorManger manager(&loop);
    g_manager = &manager;

    //3.注册驱动
    // 匹配规则：子系统 + VID + PID → 驱动工厂函数
    //  -----应该正常输出----
    DriverEntry entry1;
    entry1.subsystem = "usb";
    entry1.vendor_id = "05c8";
    entry1.product_id = "03eb";
    entry1.driver_name = "WT61";  //WT61驱动,WT61是一种IMU传感器,对应设置了他的驱动文件工厂
    entry1.onData = [](const DataBase* data) {
        const ImuData* Idata = dynamic_cast<const ImuData*>(data);
        //把回调收到的IMU数据打印出来，方便检查
        log_info(
            "\nWT61 frame=%d pitch=%.2f roll=%.2f yaw=%.2f",
            Idata->frameId,
            Idata->pitch,
            Idata->roll,
            Idata->yaw);
        log_info("\nData Received");
    };
    entry1.onError = [](const std::string& name, int error_code) {
        log_info("errno = %d", error_code);
    };
    manager.RegisterDriver(entry1);

    //-----FindeDriver能找到,但工厂没注册,应该找不到---
    DriverEntry entry2;
    entry2.subsystem = "usb";
    entry2.vendor_id = "1a83";
    entry2.product_id = "7525";
    entry2.driver_name = "MT61";  //WT61驱动,WT61是一种IMU传感器,对应设置了他的驱动文件工厂
    entry2.onData = [](const DataBase* data) {
        const ImuData* Idata = dynamic_cast<const ImuData*>(data);
        //把回调收到的IMU数据打印出来，方便检查
        log_info(
            "MT61 frame=%d pitch=%.2f roll=%.2f yaw=%.2f",
            Idata->frameId,
            Idata->pitch,
            Idata->roll,
            Idata->yaw);
        log_info("Data Received");
    };
    entry2.onError = [](const std::string& name, int error_code) {
        log_info("errno = %d", error_code);
    };
    manager.RegisterDriver(entry2);

    /* BRT38 */
    DriverEntry entry3;
    entry3.subsystem = "tty";
    entry3.vendor_id = "1a86";
    entry3.product_id = "7523";
    entry3.driver_name = "BRT38";
    entry3.onData = [](const DataBase* data) {
        const SpeedData* Idata = dynamic_cast<const SpeedData*>(data);
        //把回调收到的IMU数据打印出来，方便检查
        log_info(
            "\nBRT38 frame=%d pitch=%d n=%.4f",
            Idata->frameId,
            Idata->frequency,
            Idata->n);
        log_info("\nData Received");
    };
    entry3.onError = [](const std::string& name, int error_code) {
        log_info("errno = %d", error_code);
    };
    manager.RegisterDriver(entry3);


    //4.信号处理 (ctrl + c等信号)
    /* 以后如果收到 SIGINT，就让系统自动调用 signal_handler*/
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    //5.启动热插拔监控（注册 udev fd 到 mainLoop，启动 SubLoop 线程池）
    if(manager.Start() != 0) {
        log_error(" [main] 管理器启动失败");
        return 1;
    }

    // 6. 主线程运行 mainLoop（阻塞，监听 udev 热插拔事件）
    printf("\n------等待设备插入... (Ctrl+C 退出)------\n\n");
    loop.loop();

    return 0;
}
