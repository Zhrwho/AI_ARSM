#include <iostream>
#include "../ini_file.h"//返回上一级目录找到这个文件
#include "../value.h"
/* value可能不匹配 */
using namespace std;

int main()
{
    // 1. 获取单例
    IniFile& ini = IniFile::instance();

    // 2. 加载配置文件
    if (!ini.load("config.ini"))
    {
        cout << "ERROR: 加载配置文件失败！" << endl;
        return -1;
    }
    cout << "========================================" << endl;
    cout << "       配置文件加载成功！" << endl;
    cout << "========================================" << endl;

    // 3. 显示所有配置内容
    cout << "\n【全部配置信息】" << endl;
    ini.show();

    // 4. 读取测试（读取你项目里的所有关键配置）
    cout << "\n========================================" << endl;
    cout << "           读取配置测试" << endl;
    cout << "========================================" << endl;

    // 读取 Feature
    string device_type = ini.get("Feature configuration", "device_type");
    ini.show();
    ini.set("Feature configuration", "device_type","111");
    ini.save("new_config.ini");

    return 0;
}