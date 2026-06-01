/**
 * @brief:实现传感器插拔监听
 * Agent重构
 */

 #pragma once

 #include <atomic>
 #include <map>
 #include <memory>
 #include <mutex>
 #include <string>
 #include <vector>

 #include "Sensor_driver.h"
 #include "utility/EventLoop.h"
 #include "utility/EventLoopThreadPool.h"

/**
 * 传感器管理: 热插拔监控 + 驱动注册 + 驱动生命周期管理
 * 采用 Reactor 依赖注入模式：外部创建 mainLoop，SensorManger 只持有指针
 */
class SensorManger {
public:
    explicit SensorManger(EventLoop* mainLoop);
    ~SensorManger();

    /* 设置 SubLoop 线程数（必须在 Start() 之前调用）*/
    void setThreadNum(int num);

    /* 注册驱动工厂,注册需要监听的工厂? 在main函数里传入*/
    void RegisterDriver(const DriverEntry& entry);

    /* 扫描所有已存在设备，扫描后，如果有已存在的设备，那就直接Addsensor开始监听 */
    bool ScanAllDevice();

    /* 启动热插拔监控 */
    int Start();
    void Stop();
    /* 查询 */
    void ListSensors() const;

private:
    /* 每个传感器的运行时信息 */
    struct SensorSlot {
        std::string devnode;
        std::string driver_name;
        std::unique_ptr<SensorBase> driver;
        std::atomic<bool> running;
        EventLoop* ioLoop;  // 传感器绑定的 SubLoop
    };

    /* 添加/移除传感器 */
    int AddSensor(const std::string& subsystem,
                  const std::string& vendor_id,
                  const std::string& product_id,
                  const std::string& devnode);
    void RemoveSensor(const std::string& devnode);

    /* udev 事件回调（在 mainLoop_ 中执行）*/
    void handleUdevEvent();

    /* 匹配驱动,在main里面注册的那里去找*/
    const DriverEntry* FindDriver(const std::string& subsystem,
                                  const std::string& vendor_id,
                                  const std::string& product_id);
    /* udev 设备信息提取, info暂存信息  */
    struct DeviceInfo {
        std::string action;
        std::string subsystem;
        std::string devnode;
        std::string vendor_id;
        std::string product_id;
    };
    /* 设备信息提取 */
    DeviceInfo ParseDeviceEvent(void* udev_dev);

    std::vector<DriverEntry> driver_registry_;
    /* 存储已插入的传感器的map */
    std::map<std::string, std::unique_ptr<SensorSlot>> slots_;
    mutable std::mutex lock_;

    // 线程模型（Reactor 依赖注入）
    EventLoop* mainLoop_;                       // 外部传入，不拥有生命周期
    EventLoopThreadPool ioThreadPool_;          // SubLoop 线程池

    std::atomic<bool> running_;
    struct udev* udev_;
    struct udev_monitor* udev_mon_; //设备热插拔监听器
    bool sacndevice = false;
};
