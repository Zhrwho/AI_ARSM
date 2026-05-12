/**
 * @brief：日志模块
 */

#pragma once

#include "Singleton.h"
#include <string>
#include <iostream>
//把 std 命名空间里的 string 引入当前作用域
using std::string;

#include <sstream>
using std::ostringstream;

#include <fstream>
using std::ofstream;

//可以添加作用域SMW:作用在名字为SMW文件夹下

//初始化宏定义,初始化得到单例对象（引用接收）
#define Logger_Init(file_name) Logger::instance().open(file_name)

//定义可变参数日志宏，...表示接收可变参数
/**
 * 功能：自动展开，打印文件名和行号，##__VA_ARGS__表示所有可变参数
 */
#define log_debug(format, ...) \
Logger::instance().log( \
    Logger::LOG_DEBUG, \
    __FILE__, \
    __LINE__, \
    format, \
    ##__VA_ARGS__)
#define log_info(format, ...) \
    Logger::instance().log(Logger::LOG_INFO, __FILE__, __LINE__, format, ##__VA_ARGS__)
#define log_warn(format, ...) \
    Logger::instance().log(Logger::LOG_WARN, __FILE__, __LINE__, format, ##__VA_ARGS__)
#define log_error(format, ...) \
    Logger::instance().log(Logger::LOG_ERROR, __FILE__, __LINE__, format, ##__VA_ARGS__)
#define log_fatal(format, ...) \
    Logger::instance().log(Logger::LOG_FATAL, __FILE__, __LINE__, format, ##__VA_ARGS__)
 
class Logger : public Singleton<Logger>
/* 让Logger成为一个支持单例的类 */
{
    friend class Singleton<Logger>;
public:
/**
 * 枚举，定义5个日志等级，枚举就是给整数起了有意义的名字，可读性高，
 * 用整数表示状态，然后又去用数组来映射，这里第五个COUNT，刚好可以用来表示数组长度
 */
    enum Level
    {
        LOG_DEBUG = 0,
        LOG_INFO,
        LOG_WARN,
        LOG_ERROR,
        LOG_FATAL,
        LOG_COUNT
        /*统计枚举数量，如此处count是整数5，就是日志的等级个数5*/
    };

    void open(const std::string &filename); //打开日志文件
    void close();
    //format是格式化字符串，根据format，再从...来传入多个参数，例如"id=%d name=%s"，传入两个参数
    void log(Level level, const char* file, int line, const char* format, ...);
    void set_max_size(int bytes); //设置最大日志大小
    void set_level(Level level);
    void set_console(bool console);

private:
/* default: 不自己写构造函数逻辑，让编译器帮我生成一个“什么都不做的构造函数*/
    // Logger() = default;
    // ~Logger() = default; cpp里面要实现就不能这样写？
    Logger();
    ~Logger();

private:
    void rotate();

private:
    std::string m_filename;
    ofstream m_fout;
    /*ofstream输出文件流，专门用来往文件里写东西，日志最终要写入log文件；*/

    int m_max = 0;//最大日志大小
    int m_len = 0;
    int m_level = LOG_DEBUG;
    bool m_console = true;
    static const char* s_level[LOG_COUNT];
};