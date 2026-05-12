#include "../Logger.h"

int main()
{
    Logger_Init("Test/test.txt");

    log_debug("this is debug");
    log_info("this is info");
    log_warn("this is warn");

    //测试可变参数
    int id = 100;
    const char* name = "TOM";
    log_info("id = %d name = %s",id, name);

    return 0;
}