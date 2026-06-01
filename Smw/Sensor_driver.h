/**
 * @brief: 传感器驱动基类,注册工程父类
 */

 #pragma once

 #include <atomic>
 #include <cstdint> //提供"固定大小"的整数类型
 #include <cstring>
 #include <functional>
 #include <string>
 #include "utility/ClassFactory.h"  //同级
 #include "DataBase.h"

/* 传感器状态 */
enum class SensorStatus {
    kNull = 0,  //未创建
    kDefault,   //已创建未初始化
    kReady,     //已初始化
    kCapturing, //采集中
    kError,     //故障
};

/**
 * @brief: 定义回调类型
 * std::function<void()> 一个没有参数,没有返回值的函数
 * std::function<void(const DataBase*)> 参数是 const DataBase* 返回值是 void
 * using 起别名
 */
using OnSensorData = std::function<void(const DataBase*)>;
using OnSensorError = std::function<void(const std::string& name, int error_code)>;

/**
 * @brief: 传感器驱动基类
 * 后续所有传感器都继承
 */
class SensorBase {
public:
    SensorBase(const std::string& name, const std::string& devnode)
        : name_(name), devnode_(devnode), status_(SensorStatus::kDefault) {}
    SensorBase() : status_(SensorStatus::kDefault) {}
    virtual ~SensorBase() = default;

    /* 传感器接口定义 */
    virtual int Init() = 0;
    virtual int Start() = 0;
    virtual int Stop() = 0;
    virtual int Release() = 0;

    /* SubLoop 接口：用于 select 监听 */
    virtual int fd() const { return -1; }           // 返回文件描述符
    virtual int ReadData() { return 0; }            // 非阻塞读取数据，触发 on_data_ 回调

    /* 状态查询 */
    SensorStatus GetStatus() const { return status_; }
    const std::string& GetName() const { return name_; }
    const std:: string& GetDevnode() const { return devnode_; }
    void SetName(const std::string& name) { name_ = name; }
    void SetDevonode(const std::string& devnode) {devnode_ = devnode; }

    /* 注入回调,回调由main函数传入,在读线程循环里面执行 */
    void SetDataCallback(OnSensorData cb) { on_data_ = std::move(cb); }
    void SetErrorCallback(OnSensorError cb) { on_error_ = std::move(cb); }

protected:
    std::string name_;
    std::string devnode_;
    /* devnode 设备文件路径, 打开传感器需要拿到这个路径*/
    /* 该数值在被监控后传入到了info里面*/
    std::atomic<SensorStatus> status_;
    OnSensorData on_data_;
    OnSensorError on_error_;
};

/* 驱动注册表, 也就是创建需要监听哪些设备的存储方式(目前是main函数传入)*/
/* main函数注册的时候传入回调函数 */
struct DriverEntry {
    std::string subsystem;
    std::string vendor_id;   // USB 厂商 ID，空表示匹配所有
    std::string product_id;  // USB 产品 ID，空表示匹配所有
    std::string driver_name;
    OnSensorData onData;
    OnSensorError onError;
};