#include <iostream>
#include "../ini_file.h"//返回上一级目录找到这个文件
#include "../value.h"

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
    string device_type = ini.get("Feature configuration", "device_type").toString();
    bool auto_enable   = ini.get("Feature configuration", "auto_enble").toBool();
    string net_dev     = ini.get("Feature configuration", "net_device").toString();
    cout << "[Feature] device_type: " << device_type << endl;
    cout << "[Feature] auto_enble: " << boolalpha << auto_enable << endl;
    cout << "[Feature] net_device: " << net_dev << endl;

    // 读取 Kinect
    bool kinect_rgb   = ini.get("Kinect", "enable_rgb").toBool();
    bool kinect_depth = ini.get("Kinect", "enable_depth").toBool();
    cout << "[Kinect] enable_rgb: " << kinect_rgb << endl;

    // 读取 Nbit
    string nbit_ip   = ini.get("Nbit", "ip").toString();
    int nbit_port    = ini.get("Nbit", "port").toInt();
    cout << "[Nbit] IP: " << nbit_ip << ", Port: " << nbit_port << endl;

    // 读取串口设备（LpmsIG1）
    string imu_port  = ini.get("LpmsIG1", "serial_port").toString();
    int imu_baud     = ini.get("LpmsIG1", "baudRate").toInt();
    cout << "[LpmsIG1] serial: " << imu_port << ", baud: " << imu_baud << endl;

    // 读取雷达
    string lidar_type = ini.get("RS_HELIOS_16p", "lidar_type").toString();
    cout << "[RS_HELIOS_16p] type: " << lidar_type << endl;

    // 读取摄像头
    string video_path = ini.get("USB", "video").toString();
    cout << "[USB] video: " << video_path << endl;

    // 5. 修改配置测试
    cout << "\n========================================" << endl;
    cout << "           修改配置测试" << endl;
    cout << "========================================" << endl;

    // 修改几个关键配置
    ini.set("Feature configuration", "sensor_state", "inserted");
    ini.set("Feature configuration", "auto_enble", "false");
    ini.set("Nbit", "port", "8888");
    ini.set("Kinect", "enable_rgb", "false");
    cout << "修改完成！" << endl;

    // 6. 保存到新文件
    ini.save("config_new.ini");
    cout << "已保存修改到: config_new.ini" << endl;

    // 7. 判断节点是否存在
    cout << "\n========================================" << endl;
    cout << "           判断节点存在" << endl;
    cout << "========================================" << endl;
    cout << "有无 [Kinect]        : " << ini.has("Kinect") << endl;
    cout << "有无 [USB] video     : " << ini.has("USB", "video") << endl;

    // 8. 删除测试
    cout << "\n========================================" << endl;
    cout << "           删除测试" << endl;
    cout << "========================================" << endl;
    ini.remove("Feature configuration", "net_device");
    cout << "已删除 [Feature] net_device" << endl;

    // 显示最终结果
    ini.show();

    return 0;
}