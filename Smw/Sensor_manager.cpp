/**
 * @brief:实现传感器插拔监听
 */
#include "Sensor_manager.h"
#include "utility/ClassFactory.h"
#include "utility/Logger.h"

#include <cstring>
#include <sys/select.h>
#include <unistd.h> /* 系统调用 read() 等*/
#include <libudev.h>


SensorManger::SensorManger(EventLoop* mainLoop)
    : mainLoop_(mainLoop)
    , ioThreadPool_(mainLoop_, "SubLoop")
    , running_(false)
    , udev_(nullptr)
    , udev_mon_(nullptr)
{
}

SensorManger::~SensorManger() {
    Stop();
}

void SensorManger::setThreadNum(int num) {
    ioThreadPool_.setThreadNum(num);
}

/**
 * @brief: main函数调用,注册需要监听的传感器驱动到driver_registry_(vector)里面
 * DriverEntry结构体:存储 subsystem vendor_id product_id driver_name 和回调
 */
void SensorManger::RegisterDriver(const DriverEntry& entry)
{
    driver_registry_.push_back(entry);
    log_info("\n[Manager] 注册监听驱动: %s (子系统=%s, VID=%s, PID=%s)",
            entry.driver_name.c_str(),
            entry.subsystem.c_str(),
            entry.vendor_id.empty() ? "*" : entry.vendor_id.c_str(),
            entry.product_id.empty() ? "*" : entry.product_id.c_str());
}

/**
 * @brief：创建函数扫描所有已存在设备
 * 扫描后，如果有已存在的设备，那就直接Addsensor开始监听；
 * enumerate：扫描当前所有设备，再开启热插拔
 * sacn 完之后再开启 Hotthread
 */
bool SensorManger::ScanAllDevice()
{
    struct udev_enumerate *enumerate = udev_enumerate_new(udev_);
    if(!enumerate) {
        log_error("  udev_enumerate_new() 失败");
        return false;
    }

    /* 过滤子系统 */
    udev_enumerate_add_match_subsystem(enumerate, "usb");
    udev_enumerate_add_match_subsystem(enumerate, "tty");
    udev_enumerate_add_match_subsystem(enumerate, "net");
    /*扫描*/
    int ret = udev_enumerate_scan_devices(enumerate);
    if(ret != 0)
    {
        log_error("  udev_enumerate_scan_devices() 失败");
        udev_enumerate_unref(enumerate);
        return false;
    }
    /* 获取链表 devs:链表起始节点指针*/
    struct udev_list_entry *devs = udev_enumerate_get_list_entry(enumerate);
    /* 遍历 udev_list_entry：链表节点类型 */
    struct udev_list_entry *entry;
    udev_list_entry_foreach(entry, devs)
    {
        /* 从"设备路径"创建真正的"设备对象" ,链表节点存储的是路径 */
        const char* syspath = udev_list_entry_get_name(entry);
        struct udev_device *dev = udev_device_new_from_syspath(udev_, syspath);

        DeviceInfo info = ParseDeviceEvent(dev);
        if(info.devnode.empty()) continue;
        udev_device_unref(dev);
        AddSensor(info.subsystem, info.vendor_id, info.product_id, info.devnode);
    }
    udev_enumerate_unref(enumerate);
    sacndevice = true;
    return true;
}

/* 监听前置准备，创建监听,启动热插拔监控 */
int SensorManger::Start()
{
    udev_ = udev_new();
    if(!udev_) {
        log_error("[Manager] udev_new 失败");
        return -1;
    }

    /* 创建一个udev事件监听器  udev 是netlink通道名字*/
    udev_mon_ = udev_monitor_new_from_netlink(static_cast<struct udev*>(udev_), "udev");
    if(!udev_mon_) {
        log_error("[Manager] udev_monitor 失败");
        /* 释放 void*类型转换为Linux库函数需要的 struct udev*类型 */
        udev_unref(static_cast<struct udev*>(udev_));
        udev_ = nullptr;
        return -1;
    }

    /* 定义过滤规则 监听usb tty net三种类型设备*/
    auto* mon = static_cast<struct udev_monitor*>(udev_mon_);
    udev_monitor_filter_add_match_subsystem_devtype(mon, "usb", nullptr);
    udev_monitor_filter_add_match_subsystem_devtype(mon, "tty", nullptr);
    udev_monitor_filter_add_match_subsystem_devtype(mon, "net", nullptr);
    udev_monitor_enable_receiving(mon);

    /* 启动 SubLoop 线程池 */
    ioThreadPool_.start();

    /* 扫描已存在设备 */
    bool ret = ScanAllDevice();
    if(!ret) return -1;

    /* 把 udev fd 注册到 mainLoop_，由 mainLoop_ 的 select 统一监听 */
    int udev_fd = udev_monitor_get_fd(mon);
    mainLoop_->registerUdevFd(udev_fd, [this]() { handleUdevEvent(); });

    running_ = true;

    log_info("\n[Manager] 热插拔监控已启动 (SubLoop线程数: %zu)",
             ioThreadPool_.getAllLoops().size());
    return 0;
}

void SensorManger::Stop()
{
    running_ = false;

    /* 停止 mainLoop_（由外部负责 quit，这里只清理自身资源）*/
    mainLoop_->quit();

    {
        std::lock_guard<std::mutex> guard(lock_);
        for(auto& pair : slots_)
        {
            auto& slot = pair.second;
            log_info("[Manager] 停止传感器: %s", slot->devnode.c_str());
            slot->running = false;

            /* 从 SubLoop 注销传感器 */
            if (slot->driver->fd() >= 0 && slot->ioLoop) {
                slot->ioLoop->unregisterSensor(slot->driver.get());
            }

            slot->driver->Stop();
            slot->driver->Release();
        }
        slots_.clear();
    }
    /* 释放监听设备 */
    if (udev_mon_) {
        udev_monitor_unref(static_cast<struct udev_monitor*>(udev_mon_));
        udev_mon_ = nullptr;
    }
    if( udev_) {
        udev_unref(static_cast<struct udev*>(udev_));
        udev_ = nullptr;
    }

    log_info("[Manager] 已停止");
}

// ==================== udev 热插拔事件回调 ====================
/**
 * @brief: 在 mainLoop_ 线程中执行，处理 udev 热插拔事件
 * 通过 queueInLoop 将传感器操作投递到目标 SubLoop 线程执行
 */
void SensorManger::handleUdevEvent() {
    auto* mon = static_cast<struct udev_monitor*>(udev_mon_);
    struct udev_device* dev = udev_monitor_receive_device(mon);
    if (!dev) return;

    DeviceInfo info = ParseDeviceEvent(dev);
    udev_device_unref(dev);

    if (info.devnode.empty()) return;

    if (info.action == "bind" || (info.subsystem == "tty" && info.action == "add")) {
        log_info("\n[Manager] 设备插入: %s (%s:%s)",
                 info.devnode.c_str(), info.vendor_id.c_str(),
                 info.product_id.c_str());

        /* 先检查驱动是否已注册，避免无意义的投递 */
        const DriverEntry* entry = FindDriver(info.subsystem, info.vendor_id, info.product_id);
        if (!entry) {
            log_info("[Manager] 驱动监听未被注册 (子系统=%s, VID=%s, PID=%s)",
                     info.subsystem.c_str(), info.vendor_id.c_str(), info.product_id.c_str());
            return;
        }

        /* 获取目标 SubLoop，通过 queueInLoop 投递任务 */
        EventLoop* ioLoop = ioThreadPool_.getNextLoop();
        std::string subsystem = info.subsystem;
        std::string vendor_id = info.vendor_id;
        std::string product_id = info.product_id;
        std::string devnode = info.devnode;
        ioLoop->queueInLoop([this, subsystem, vendor_id, product_id, devnode]() {
            AddSensor(subsystem, vendor_id, product_id, devnode);
        });
    }
    else if (info.action == "remove") {
        log_info("\n[Manager] 设备移除: %s", info.devnode.c_str());

        /* 找到传感器所在的 SubLoop，投递移除任务 */
        std::lock_guard<std::mutex> guard(lock_);
        auto it = slots_.find(info.devnode);
        if (it != slots_.end()) {
            EventLoop* ioLoop = it->second->ioLoop;
            std::string devnode = info.devnode;
            ioLoop->queueInLoop([this, devnode]() {
                RemoveSensor(devnode);
            });
        }
    }
}

/**
 * @brief: 把监听到的设备信息存储到info里面,解析;
 */
SensorManger::DeviceInfo SensorManger::ParseDeviceEvent(void* udev_dev)
{
    auto* dev = static_cast<struct udev_device*>(udev_dev);
    DeviceInfo info;
    /* 从设备对象读取属性深拷贝出来之后,方便后续释放 */
    const char* action = udev_device_get_action(dev);
    const char* subsystem = udev_device_get_subsystem(dev);
    const char* devnode = udev_device_get_devnode(dev);
    /* vid pid */
    const char* vendor_id = udev_device_get_property_value(dev, "ID_VENDOR_ID");
    const char* product_id = udev_device_get_property_value(dev, "ID_MODEL_ID");

    if(action) info.action = action;
    if(subsystem) info.subsystem = subsystem;
    if(devnode) info.devnode = devnode;
    if(vendor_id) info.vendor_id = vendor_id;
    if(product_id) info.product_id = product_id;

    return info;
}

/* 匹配驱动,在main里面注册的那里去找*/
/**
 * @brief:找现在的这个设备是否被注册了,即是否需要监听
 */
const DriverEntry* SensorManger::FindDriver(const std::string& subsystem,
                                            const std::string& vendor_id,
                                            const std::string& product_id) {
    for (const auto& entry : driver_registry_) {
        if(entry.subsystem != subsystem) continue;
        if(!entry.vendor_id.empty() && entry.vendor_id != vendor_id) continue;
        if(!entry.product_id.empty() && entry.product_id != product_id) continue;
        return &entry;
    }
    return nullptr;
}

// ==================== 传感器生命周期 ====================
//用工厂创建子对象，初始化，并把设备存储到slots（已注册的设备）里面
//1.slots里面存储所有用工厂注册过的子对象类;
//2.driver_registry_存储所有main函数里面说明要监听的对象;
int SensorManger::AddSensor(const std::string& subsystem,
                            const std::string& vendor_id,
                            const std::string& product_id,
                            const std::string& devnode) {
    std::lock_guard<std::mutex> guard(lock_);

    if(slots_.count(devnode)) {
        log_info("[Manager] 传感器 %s 已存在", devnode.c_str());
        return -1;
    }

    /* 确保已经在main函数被注册到driver_registry_,即需要监听*/
    const DriverEntry* entry = FindDriver(subsystem, vendor_id, product_id);
    if(!entry) {
        if(sacndevice)
        //只对插入新设备时 进行匹配驱动注册，扫描设备时不打印
        {
        log_info("[Manager] 驱动监听未被注册 (子系统=%s, VID=%s, PID=%s)",
               subsystem.c_str(), vendor_id.c_str(), product_id.c_str());
        }
        return -1;
    }

    /* 获取一个 SubLoop（轮询） */
    EventLoop* ioLoop = ioThreadPool_.getNextLoop();

    /* 工厂创建驱动，创建子类对象(已经注册过的父类工厂可以通过设备名直接创建子类对象) */
    std::unique_ptr<SensorBase> driver = CreateObject<SensorBase>(entry->driver_name);
    if(!driver) {
        log_error("[Manager] 驱动创建失败");
        return -1;
    }

    /* 创建成功就注入回调, 输入回调给具体的创建的驱动子类对象  */
    driver->SetName(entry->driver_name);
    driver->SetDevonode(devnode);
    driver->SetDataCallback(entry->onData);
    driver->SetErrorCallback(entry->onError);

    /* 初始化驱动,调用驱动函数*/
    if(driver->Init() != 0)
    {
        log_error("[Manager] 驱动初始化失败");
        return -1;
    }
    /* 启动驱动 */
    if( driver->Start() != 0)
    {
        log_error("[Manager] 驱动启动失败");
        return -1;
    }

    /* 将传感器注册到 SubLoop，由 select 统一监听 */
    if (driver->fd() >= 0) {
        ioLoop->registerSensor(driver.get());
        log_info("\n[Manager] 传感器 %s 已注册到 SubLoop (fd=%d)",
               devnode.c_str(), driver->fd());
    }

    /* 创建slot, 存到slots里面 用devnode做key */
    auto slot = std::make_unique<SensorSlot>();
    slot->devnode = devnode;
    slot->driver_name = entry->driver_name;
    slot->driver = std::move(driver);
    slot->running = true;
    slot->ioLoop = ioLoop;

    /* 存到slots里面,  用devnode设备文件路径作为索引 */
    slots_[devnode] = std::move(slot);
    log_info("\n[Manager] 传感器 %s 已添加 (驱动=%s, SubLoop=%p)",
           devnode.c_str(), entry->driver_name.c_str(), ioLoop);
    return 0;
}

/**
 * @brief: 移除传感器,调用驱动的stop函数
 */
void SensorManger::RemoveSensor(const std::string& devnode) {
    std::lock_guard<std::mutex> guard(lock_);

    auto it = slots_.find(devnode);
    if ( it == slots_.end())
    {
        log_info("[Manager] 传感器 %s 不存在", devnode.c_str());
        return;
    }

    auto& slot = it->second;
    log_info("[Manager] 移除传感器: %s", devnode.c_str());

    /* 从 SubLoop 注销传感器 */
    if (slot->driver->fd() >= 0 && slot->ioLoop) {
        slot->ioLoop->unregisterSensor(slot->driver.get());
    }

    slot->running = false;
    slot->driver-> Stop();
    slot->driver->Release();
    slots_.erase(it);
}

/* 查看已经监控了哪些传感器 */
void SensorManger::ListSensors() const {
    std::lock_guard<std::mutex> guard(lock_);
    log_info("\n========= 当前传感器列表 (%zu 个) =========", slots_.size());
    /*     std::map<std::string, std::unique_ptr<SensorSlot>> slots_; */
    for(const auto& pair : slots_) {
        const char* st = "未知";
        switch(pair.second->driver->GetStatus()) {
            case SensorStatus::kNull: st = "未创建"; break;
            case SensorStatus::kDefault: st = "未初始化"; break;
            case SensorStatus::kReady: st = "就绪"; break;
            case SensorStatus::kCapturing: st = "采集中"; break;
            case SensorStatus::kError: st = "故障"; break;
        }
        log_info("  %s -> %s [%s] (SubLoop=%p)",
               pair.first.c_str(), pair.second->driver_name.c_str(), st,
               pair.second->ioLoop);
    }
    printf("==========================================\n\n");
}
