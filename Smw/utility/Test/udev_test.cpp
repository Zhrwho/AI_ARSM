/** Linux 下基于 libudev 的设备热插拔监听程序  */
/* udev监控的流程 */

#include <cstdio>
#include <cstring>
#include <sys/select.h>
#include <unistd.h>
#include <libudev.h>

static const char* safe(const char *s);

static void print_device_info(struct udev_device *dev, const char *action);

int main() {
    //创建一个udev；
    struct udev *udev = udev_new();
    if (!udev) {
        fprintf(stderr, "udev_new failed\n");
        return 1;
    }

    //监控udev
    struct udev_monitor *mon = udev_monitor_new_from_netlink(udev, "udev");
    if (!mon) {
        fprintf(stderr, "udev_monitor_new_from_netlink failed\n");
        udev_unref(udev);
        return 1;
    }

    // 添加过滤规则
    //监听子系统
    udev_monitor_filter_add_match_subsystem_devtype(mon, "usb", NULL);
    // udev_monitor_filter_add_match_subsystem_devtype(mon, "net", NULL);
    // udev_monitor_filter_add_match_subsystem_devtype(mon, "block", NULL);
    // udev_monitor_filter_add_match_subsystem_devtype(mon, "input", NULL);
    udev_monitor_enable_receiving(mon);

    int fd = udev_monitor_get_fd(mon);
    printf("开始监听设备热插拔事件 (fd=%d)...\n\n", fd);

    while (1) {
        //select一直监听

        //select的fd位图
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(fd, &fds);

        struct timeval tv = { .tv_sec = 5, .tv_usec = 0 };
        //tv超时时间
        int ret = select(fd + 1, &fds, NULL, NULL, &tv);

        if (ret < 0) {
            perror("select");
            break;
        }

        if (ret == 0) {
            // 超时，可以做心跳或其他任务
            continue;
        }
        //看fd
        if (FD_ISSET(fd, &fds)) {
            //monitor监听到的，拿到了插入设备的信息
            struct udev_device *dev = udev_monitor_receive_device(mon);
            if (!dev) continue;
            //从dev里面解析设备的动作
            const char *action = udev_device_get_action(dev);
            //从dev里面解析设备的node
            const char *devnode = udev_device_get_devnode(dev);

            // 只对 bind（设备就绪）和 remove（设备移除）进行检测
            // 过滤掉 devnode 为空的接口级别事件，避免重复
            if (devnode && action) {
                if (strcmp(action, "bind") == 0) {
                    print_device_info(dev, action);
                } else if (strcmp(action, "remove") == 0) {
                    print_device_info(dev, action);
                }
            }
            //释放掉刚刚创建的dev
            udev_device_unref(dev);
        }
    }

    udev_monitor_unref(mon);
    udev_unref(udev);
    return 0;
}

/************************* 打印相关函数 *************************/
/* 解析了其他的信息并打印 */
static const char* safe(const char *s) {
    return s ? s : "N/A";
}

static void print_device_info(struct udev_device *dev, const char *action) {
    const char *subsystem = udev_device_get_subsystem(dev);
    const char *devpath   = udev_device_get_devpath(dev);
    const char *devnode   = udev_device_get_devnode(dev);

    if (strcmp(action, "bind") == 0) {
        printf("===== 设备可用 =====\n");
    } else {
        printf("===== 设备移除 =====\n");
    }

    printf("子系统:   %s\n", safe(subsystem));
    printf("设备节点: %s\n", safe(devnode));
    printf("设备路径: %s\n", safe(devpath));

    // USB 设备信息
    if (strcmp(subsystem, "usb") == 0) {
        printf("厂商ID:   %s\n", safe(udev_device_get_property_value(dev, "ID_VENDOR_ID")));
        printf("产品ID:   %s\n", safe(udev_device_get_property_value(dev, "ID_MODEL_ID")));
        printf("厂商名:   %s\n", safe(udev_device_get_property_value(dev, "ID_VENDOR")));
        printf("产品名:   %s\n", safe(udev_device_get_property_value(dev, "ID_MODEL")));
        printf("序列号:   %s\n", safe(udev_device_get_property_value(dev, "ID_SERIAL")));
        printf("版本:     %s\n", safe(udev_device_get_property_value(dev, "ID_REVISION")));
        printf("USB总线:  %s\n", safe(udev_device_get_property_value(dev, "BUSNUM")));
        printf("设备号:   %s\n", safe(udev_device_get_property_value(dev, "DEVNUM")));
        printf("驱动:     %s\n", safe(udev_device_get_driver(dev)));
    }

    // 网络设备信息
    if (strcmp(subsystem, "net") == 0) {
        printf("接口名:   %s\n", safe(udev_device_get_property_value(dev, "INTERFACE")));
        printf("接口索引: %s\n", safe(udev_device_get_property_value(dev, "IFINDEX")));
        printf("设备类型: %s\n", safe(udev_device_get_property_value(dev, "DEVTYPE")));
        printf("MAC地址:  %s\n", safe(udev_device_get_sysattr_value(dev, "address")));
    }

    // 块设备信息
    if (strcmp(subsystem, "block") == 0) {
        printf("主设备号: %s\n", safe(udev_device_get_property_value(dev, "MAJOR")));
        printf("次设备号: %s\n", safe(udev_device_get_property_value(dev, "MINOR")));
        printf("设备类型: %s\n", safe(udev_device_get_property_value(dev, "DEVTYPE")));
        printf("文件系统: %s\n", safe(udev_device_get_property_value(dev, "ID_FS_TYPE")));
        printf("UUID:     %s\n", safe(udev_device_get_property_value(dev, "ID_FS_UUID")));
        printf("卷标:     %s\n", safe(udev_device_get_property_value(dev, "ID_FS_LABEL")));
        printf("厂商:     %s\n", safe(udev_device_get_property_value(dev, "ID_VENDOR")));
        printf("型号:     %s\n", safe(udev_device_get_property_value(dev, "ID_MODEL")));
        printf("序列号:   %s\n", safe(udev_device_get_property_value(dev, "ID_SERIAL")));
    }

    // 输入设备信息
    if (strcmp(subsystem, "input") == 0) {
        printf("设备名:   %s\n", safe(udev_device_get_property_value(dev, "NAME")));
        printf("物理路径: %s\n", safe(udev_device_get_property_value(dev, "PHYS")));
        printf("产品ID:   %s\n", safe(udev_device_get_property_value(dev, "PRODUCT")));
    }

    printf("====================\n\n");
}
