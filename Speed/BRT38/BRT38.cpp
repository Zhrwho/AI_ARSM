/**
 * @brief:速度传感器驱动
 */

#include "BRT38.h"
#include "unistd.h"

#include <sys/ioctl.h>
#include <fcntl.h>
#include <termios.h>

/* 返回ch字符在sign数组中的序号 */
// Hex:  A表示10， B表示11...
int BRT38::getIndex0fSigns(char ch)
{
    if(ch >= '0' && ch <= '9')
    {
        return ch - '0';
    }
    else if ( ch >= 'A' && ch <= 'F')
    {
        return ch - 'A' + 10;
    }
    else if ( ch >= 'a' && ch <= 'f')
    {
        return ch - 'a' + 10;
    }
    return -1;
}

/* 十六进制数转换为十进制数 */
long BRT38::hexToDec(char* source, int len)
{
    long sum = 0;
    long t = 1;
    for ( int i = len - 1; i >= 0; i--)
    {
        /* *(source + i) 指针偏移 + 解引用， 等价于 source[i]*/
        sum += t * getIndex0fSigns(source[i]);
        t *= 16;
    }
    return sum;
}


int BRT38::Init()
{
    
    sleep(1);
    /* 打开Linux串口设备文件，对应该传感器 */
    fd_ = open(devnode_.c_str(), O_RDWR | O_NOCTTY | O_NDELAY);
    if(fd_ == -1)
    {
        log_error("[Serial:%s] 打开失败： %s\n", name_.c_str(), strerror(errno));
        status_ = SensorStatus::kError;
        return -1;
    }

    /* 配置串口参数 */
    struct termios new_cfg = { 0 };
    cfmakeraw(&new_cfg);            // 设置为原始模式   
	cfsetspeed(&new_cfg, B9600);    // 将波特率设置为9600

	new_cfg.c_cflag |= CREAD;       // 使能接收 
	new_cfg.c_cflag &= ~CSIZE;      // 将数据位相关的比特位清零
	new_cfg.c_cflag |= CS8;         // 将数据位数设置为8位
	new_cfg.c_cflag &= ~PARENB;
	new_cfg.c_iflag &= ~INPCK;      // 设置为无校验模式
	new_cfg.c_cflag &= ~CSTOPB;     // 将停止位设置为1位
	new_cfg.c_cc[VTIME] = 0;        // 将 MIN 和 TIME 设置为 0
	new_cfg.c_cc[VMIN] = 0;

    /* 清空缓冲区域 tcflush 将输出缓冲器清空，把输入缓冲区清空。缓冲区里的数据都废弃 */
    int ret = tcflush(fd_, TCIOFLUSH);
    if(ret < 0) {
        log_error("tcflush error%s\n", strerror(errno));
        status_ = SensorStatus::kError;
        return -1;
    }

    /* 写入配置，使配置生效 把串口配置真正应用到串口设备 即上述写的波特率等都生效 */
    ret = tcsetattr(fd_, TCSANOW, &new_cfg);
    if(ret < 0)
    {
        log_error("tcsetattr error%s\n", strerror(errno));
        status_ = SensorStatus::kError;
        return -1;
    }
    status_ = SensorStatus::kReady;
    log_info("[Serial:%s] 初始化成功\n", name_.c_str());
    return 0;
}

int BRT38::Start()
{
    if(status_ != SensorStatus::kReady)
    {
        log_error("[Serial:%s] 状态不正确，无法启动\n", name_.c_str());
        return -1;
    }

    status_ = SensorStatus::kCapturing;
    stop_ = false;
    /* 创建一个新的采集线程 */
    /**
     * read_thread_ ： 线程ID保存位置 pthread_t 创建
     * ReadThread ： 线程入口函数，线程启动后执行这个函数
     */
    pthread_create(&read_thread_, nullptr, ReadThread, this);
    log_info("[Serial:%s] 已启动采集\n", name_.c_str());
    return 0;
}

int BRT38::Stop()
{
    stop_ = true;
    if(read_thread_ != 0)
    {
        pthread_join(read_thread_, nullptr);
        read_thread_ = 0;
    }
    status_ = SensorStatus::kReady;
    log_info("[Serial:%s] 已停止采集\n", name_.c_str());
    return 0;
}

int BRT38::Release()
{
    if( fd_ != -1)
    {
        close(fd_);
        fd_ = -1;
        log_info("[Serial:%s] 已释放\n", name_.c_str());
    }
    return 0;
}

void* BRT38::ReadThread(void* arg)
{
    auto* driver = static_cast<BRT38*>(arg);
    driver->ReadLoop();
    return nullptr;
}

void BRT38::ReadLoop()
{
    log_info("[Serial:%s] 采集线程启动\n", name_.c_str());
    /* （）就是调用 SpeedData 的构造函数，创建 SpeedData 对象*/
    std::shared_ptr<SpeedData> tmp = std::make_shared<SpeedData>();
    uint32_t fid = 0;
    while(!stop_)
    {
        sleep(1);
        /* Linux程序通过串口 把数组readcmd_命令 发送给BRT38传感器*/
        /* write：往串口里写数据，即发送给传感器*/   
        int ret = write(fd_, readcmd_, 8);
        if( ret < 0 ) {
            log_error("write fail%s\n", strerror(errno));
        }

        /* 把整个串口接收缓冲区清空 */
        bzero(buf_, SUDU_Buffer_Length);
        /* 从串口读数据，即读取传感器传过来的数据 */
        ret = read(fd_, buf_, 10);
        if(ret > 0)
        {
            int i = 0;
            for(; i < ret - 1; i++)
            {
                /* 解析的是测试数据 */

                /* 找到数据头，然后把 i 移动到数据区 */
                if(buf_[i] == 0x03 && buf_[i+1] == 0x04)
                {
                    /* 跳过数据头，得到后面传入的数据*/
                    i += 2;
                    break;
                }    
            }

            /* 解析数据buf_ ，暂时只解析了转速，id和频率都是自己设的，测试用*/
            char buf[5];
            /* strncpy 从buf_中取出 4个字符存到buf中*/
            strncpy(buf, buf_ + i, 4);
            tmp ->n = hexToDec(buf,4); //转速
            tmp->frameId = fid++; //帧号ID
            tmp->frequency = 10;  //采集频率
            /* 编码器值 → 实际位移 */
            tmp->n = (float)tmp->n*0.00915f;
            /* get表示从tmp中取出裸指针 */
            if(on_data_) on_data_(tmp.get());
        }
        else if(ret < 0)
        {
            if(errno == EAGAIN || errno == EWOULDBLOCK)
            {
                    usleep(1000);
                    continue;

            }
            log_error("[Serial:%s] 读取错误: %s\n", name_.c_str(), strerror(errno));
            if( on_error_ ) on_error_(name_, errno);
            status_ = SensorStatus::kError;
            break;
        }
    }
    log_info("[Serial:%s] 采集线程退出\n", name_.c_str());
}