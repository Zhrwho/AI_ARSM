#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <cstring>
#include <errno.h>

int main()
{
    // 1. 打开串口
    const char* dev = "/dev/ttyUSB0";

    int fd = open(dev, O_RDWR | O_NOCTTY | O_NDELAY);

    if (fd < 0)
    {
        std::cout << "open failed: "
                  << strerror(errno)
                  << std::endl;
        return -1;
    }

    std::cout << "open serial success" << std::endl;

    // 2. 配置串口
    struct termios tty;

    memset(&tty, 0, sizeof(tty));

    if (tcgetattr(fd, &tty) != 0)
    {
        std::cout << "tcgetattr failed" << std::endl;
        return -1;
    }

    // 波特率
    cfsetispeed(&tty, B115200);
    cfsetospeed(&tty, B115200);

    // 8数据位
    tty.c_cflag |= CS8;

    // 无校验
    tty.c_cflag &= ~PARENB;

    // 1停止位
    tty.c_cflag &= ~CSTOPB;

    // 本地连接
    tty.c_cflag |= CLOCAL;
    tty.c_cflag |= CREAD;

    // 原始模式
    tty.c_lflag = 0;
    tty.c_oflag = 0;
    tty.c_iflag = 0;

    // 非阻塞读取
    tty.c_cc[VMIN]  = 0;
    tty.c_cc[VTIME] = 1;

    tcflush(fd, TCIOFLUSH);

    if (tcsetattr(fd, TCSANOW, &tty) != 0)
    {
        std::cout << "tcsetattr failed" << std::endl;
        return -1;
    }

    std::cout << "serial config success" << std::endl;

    // 3. 循环读写
    char recv_buf[1024];

    while (true)
    {
        // 发送数据
        const char send_buf[] = "hello CH340\n";

        int ret = write(fd, send_buf, strlen(send_buf));

        if (ret > 0)
        {
            std::cout << "send: "
                      << send_buf;
        }
        else
        {
            std::cout << "write failed"
                      << std::endl;
        }

        // 清空接收buf
        memset(recv_buf, 0, sizeof(recv_buf));

        // 读取数据
        ret = read(fd, recv_buf, sizeof(recv_buf));

        if (ret > 0)
        {
            std::cout << "recv (" << ret << " bytes): ";

            for (int i = 0; i < ret; i++)
            {
                printf("%02X ", (unsigned char)recv_buf[i]);
            }

            std::cout << std::endl;
        }

        sleep(1);
    }

    close(fd);

    return 0;
}