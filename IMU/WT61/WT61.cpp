/**
 * 实现的是驱动的撰写?意味不明
 * 最后read 的fd与返回值操作不明确
 */

#include "WT61.h"

#include <fcntl.h>  /* 文件控制 */
#include <unistd.h> /* unix标准接口*/
#include <termios.h>/* 串口配置 */
#include <cstdio>
#include <cerrno>
#include <cstring>

/* name在manager里面设置,(setname函数)由main函数填入entry后设置*/
WT61::WT61(const std::string& name, const std::string& devnode)
    : SensorBase(name,devnode), fd_(-1), read_thread_(0), stop_(false) { }
WT61::WT61() : fd_(-1), read_thread_(0), stop_(false) { }

WT61:: ~WT61() {
    if(fd_ >= 0) Release();
 }

/**
 * @brief:初始化open→配置termios→设置波特率→应用配置tcsetattr
 */
int WT61::Init(){
    /* devnode_这个设备在linux下生成的文件的路径/dev/ttyUSB0 */
    /* 可以认为devnode_才是真正的Linux设备名*/
    /* 由manager函数简体后从INFO传入devnode*/
    log_info("\n[Serial:%s] 打开设备 %s", name_.c_str(), devnode_.c_str());
    fd_ = open(devnode_.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
    if(fd_ < 0)
    {
        log_error("[Serial:%s] 打开失败: %s", name_.c_str(), strerror(errno));
        status_ = SensorStatus::kError;
        return -1;
    }
    
    /* 配置串口 115200 8N1 */
    struct termios tty {};
    /* 获取当前串口设备的配置参数 */
    tcgetattr(fd_, &tty);
    /* 设置输出波特率 */
    cfsetospeed(&tty, B115200);
    /* 设置输入波特率 */
    cfsetispeed(&tty, B115200);
    tty.c_cflag = CS8 | CLOCAL | CREAD;
    tty.c_iflag = 0;
    tty.c_oflag = 0;
    tty.c_lflag = 0;
    /* 设置属性 */
    tcsetattr(fd_, TCSANOW, &tty);

    status_ = SensorStatus::kReady;
    log_info("\n[Serial:%s] 初始化成功", name_.c_str());
    return 0;
}

/* 串口设备切换到采集状态,开启读线程不断采集数据 loop里面while(!stop)循环*/
int WT61::Start(){
    if(status_ != SensorStatus::kReady)
    {
        log_error("[Serial:%s] 状态不正确，无法启动", name_.c_str());
        return -1;
    }
    status_ = SensorStatus::kCapturing;
    stop_ = false;
    /* 创建该设备线程 读数据*/
    pthread_create(&read_thread_, nullptr, ReadThread, this);
    log_info("\n[Serial:%s] 已启动采集", name_.c_str());
    return 0;
}

int WT61::Stop(){
    stop_ = true;
    if(read_thread_ != 0){
        pthread_join(read_thread_, nullptr);
        read_thread_ = 0;
    }
    status_  = SensorStatus::kReady;
    log_info("[Serial:%s] 已停止采集", name_.c_str());
    return 0;
}

int WT61::Release(){
    if(fd_ >= 0){
        close(fd_);
        fd_ = -1;
    }
    status_ = SensorStatus::kDefault;
    log_info("[Serial:%s] 已释放", name_.c_str());
    return 0;
}

void* WT61::ReadThread(void* arg){
    /* 把线程接收到的万能指针恢复成真正的SerialDriver对象 */
    auto* driver = static_cast<WT61*>(arg);
    driver->ReadLoop();
    return nullptr;
}

/* 执行回调 on_data_ */
void WT61::ReadLoop(){
    log_info("\n[Serial:%s] 采集线程启动", name_.c_str());
    /* WT61是IMU传感器,返回IMU数据类型 */
    std::shared_ptr<ImuData> tmp = std::make_shared<ImuData>();

    while(!stop_) {
        char buf[1024];
        ssize_t n = read(fd_, buf, sizeof(buf));
        /*  不完善,这里应该写个协议解析,把串口读到的buf数据, 解析存进tmp里面 */

        /* 测试数据, 造一份假数据 存到Imudata里面, 方便测试 */
        /* 后续读到真的Imu数据要进行数据解析 */
        tmp->frameId = 1;
        tmp->pitch = 0;
        tmp->yaw = 1;
        tmp->roll = 2;


        if(n > 0) {
            /* 执行main传入的回调 */
            if(on_data_) {
                on_data_(tmp.get());
            }
        }
        else if ( n < 0) {
            if( errno == EAGAIN || errno == EWOULDBLOCK) {
                usleep(1000);
                continue;
            }
            log_error("[Serial:%s] 读取错误: %s", name_.c_str(), strerror(errno));
            if(on_error_) {
                on_error_(name_, errno);
            }
            status_ = SensorStatus::kError;
            break;
        }
    }
    log_info("[Serial:%s] 采集线程退出", name_.c_str());
}
