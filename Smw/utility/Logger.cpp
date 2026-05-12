/**
 * @brief：日志模块
 */

#include "Logger.h"

#include <string.h>
#include <time.h>
#include <errno.h>
#include <cstring>
#include <chrono>
#include <stdarg.h>

const char* Logger::s_level[LOG_COUNT] = 
{
    "DEBUG",
    "INFO",
    "WARN",
    "ERROR",
    "FATAL"
};

//构造函数初始化列表
Logger::Logger():m_max(0), m_len(0), m_level(LOG_DEBUG), m_console(true)
{

}
Logger::~Logger()
{
    close();
}

void Logger::open(const string &filename)//打开日志文件
{
    m_filename = filename;
    /**
     * m_fout是ofstream类型的，是输出文件流，专门用于向文件写数据
     * 作用： 以追加模式打开日志文件；
     * open的借口需要const char*,所以c_str()，把string转成const char*
     * std::ios::app追加append：  表示日志不会覆盖旧内容
    */
    m_fout.open(filename.c_str(), std::ios::app);
    if(m_fout.fail())
    {
        /* throw 表示抛异常  logic_error标准库异常*/
        throw std::logic_error("open log file failed: " + filename);
    }
    /* 获取当前日志文件大小 (当前写入位置距离文件开头的字节数)*/
    m_len = m_fout.tellp();
}
void Logger::close()
{
    /* 断开 ofstream 和文件之间的连接 */
    m_fout.close();
}

/**
 * 作用：
 * 按等级过滤
 * 拼接日志头（时间 + level + 文件 + 行号 + 内容）
 * 处理 printf 风格可变参数
 * 输出到控制台和文件
 * 超限触发日志滚动
 */

void Logger::log(Level level, const char* file, int line, const char* format, ...)
{
    /* 日志等级过滤 */
    if( m_level > level) return; 
    if(m_fout.fail()) return;
    
    /*创建字符串输出流，往字符串里写东西的cout，用于日志拼接字符串*/
    ostringstream oss;
    char timestamp[100];
    memset(timestamp, 0, sizeof(timestamp));
    
    /**
     * 作用： 获取时间（秒+毫秒）
     * 实现步骤：time_t的格式化年月日等 + chrono的毫秒
     */
    /* system_clock表示系统时钟，即电脑当前真实时间; now()返回当前时间点； auto推到后类型，实际是std::chrono::time_point*/
    auto now = std::chrono::system_clock::now();
    /* time_point_cast<milliseconds> 把时间精度转换为“毫秒” */
    auto now_ms = std::chrono::time_point_cast<std::chrono::milliseconds>(now);
    /* time_since_epoch 获取“从1970到现在”的时间长度（毫秒），返回类型std::chrono::duration*/
    auto value = now_ms.time_since_epoch();
    /* count 调用duration，得到毫秒级时间戳 */
    auto duration = value.count();


    /**
     * to_time_t函数将时间点对象转换为time_t，精确到秒
     * 作用：用于后续获取年月日时分秒等格式化输出时间
     */
    std::time_t current_time = std::chrono::system_clock::to_time_t(now);
    /* 只拿出毫秒部分，上面duration部分的综合*/
    //std::chrono::milliseconds ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    auto ms = duration % 1000;

    /* 格式化输出时间 */
    char buffer[100];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", std::localtime(&current_time));
    /* 加上毫秒 */
    std::string result = std::string(buffer) + "." + std::to_string(ms);

    /* 拼接构造日志头 */
    /* 复制result到最开始定义的哪个timestamp数组里 */
    std::strcpy(timestamp, result.c_str());
    int len = 0;
    /* 时间打印格式：[时间] [级别] 文件:行号 */
    const char* fmt = "%s %s %s:%d";
    /* snprintf(NULL, 0, ...) 不用写入，只计算长度*/
    len = snprintf(NULL, 0, fmt, timestamp, s_level[level], file, line);
    if(len > 0)
    {
        char* buffer = new char[len + 1];
        snprintf(buffer, len + 1, fmt, timestamp, s_level[level], file, line);
        /* 把buffer最后一个置为0，保险补'\0' */
        buffer[len] = 0;
        /* 写入oss */
        oss << buffer;
        delete [] buffer;
        m_len += len;
    }

    /* 处理可变参数,传入内容 */
    va_list arg_ptr;
    va_start(arg_ptr, format);//读取数据
    /* 计算长度 */
    len = vsnprintf(NULL, 0, format, arg_ptr);
    va_end(arg_ptr);
    if(len > 0)
    {
        char* content = new char[len + 1];
        /* 重来一遍是因为前面用于计算长度之后，已经end了，失效了*/
        va_start(arg_ptr, format);
        /* 写入 */
        vsnprintf(content, len + 1, format, arg_ptr);
        va_end(arg_ptr);
        content[len] = 0;
        /* 把内容写入stringstream [时间] [级别] 文件:行号 内容 */
        oss << content;
        delete [] content;
        m_len += len;
    }
    oss << "\n"; //打印到log一行之后，就换行
    const string& str = oss.str();

    /* 是否打印到屏幕 */
    if(m_console) std::cout << str << std::endl;
    /* 写入log 文件,m_fout是ofstream文件输出流 */
    m_fout << str;
    m_fout.flush();//强制把缓冲区写入磁盘

    //m_max默认为0，默认为不启动；
    if(m_max > 0 && m_len >= m_max)
    {
        rotate();
    }

}
void Logger::set_max_size(int bytes) //设置最大日志大小
{
    m_max = bytes;
}
void Logger::set_level(Level level) //设置当前日志等级
{
    m_level = level;
}
void Logger::set_console(bool console)
{
    m_console = console;
}

/**
 * 日志滚动，超过大小就切文件
 */
void Logger::rotate()
{
    //log文件里调用，超过文件大小就切文件
    close();
    /* 获得时间戳 /从1970年1月1日到现在的秒数 */
    time_t ticks = time(NULL);
    /* 把时间戳转成本地时间结构tm*, 要求的返回类型是time_t指针，传入的是时间戳的地址变量 */
    struct tm* ptm = localtime(&ticks);

    char timestamp[32];
    /* 把整个 timestamp 数组全部清零 */
    memset(timestamp, 0, sizeof(timestamp));
    /* 格式化时间字符串 */
    strftime(timestamp, sizeof(timestamp), ".%%Y-%m-%d_%H-%M-%S", ptm);
    string filename = m_filename + timestamp;
    /* rename(old,new) 重命名文件，返回值为0表示成功*/
    if(rename(m_filename.c_str(), filename.c_str()) != 0)
    {
        /*strerror(errno)获取系统错误信息 */
        throw std::logic_error("rename log file failed: " + string(strerror(errno)));
    }
    /* 超过长度时，旧日志改名保存，然后重新打开open(m_filename)*/
    open(m_filename);
}

