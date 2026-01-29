#include "ds18b20_App.h"

// 读取温度实现（适配驱动的原始16位整数输出，用户态浮点换算）
int ds18b20_read_temperature(char *temp_buf, int buf_size)
{
    int fd;
    int ret;
    char raw_buf[TEMP_BUF_SIZE];
    int temp_raw;   // 存储驱动传递的原始16位整数
    float temp_real;// 用户态浮点换算，无任何限制

    // 打开DS18B20设备节点（只读模式）
    fd = open(DS18B20_DEV_PATH, O_RDONLY);
    if (fd < 0) {
        perror("open /dev/ds18b20 failed");
        return -1;
    }

    // 清空缓冲区
    memset(raw_buf, 0, sizeof(raw_buf));
    memset(temp_buf, 0, buf_size);

    // 读取驱动传递的原始整数温度
    ret = read(fd, raw_buf, sizeof(raw_buf) - 1);
    if (ret < 0) {
        perror("read ds18b20 raw data failed");
        close(fd);
        return -1;
    }
    close(fd);

    // 字符串转整数（原始16位值）
    temp_raw = atoi(raw_buf);
    // 核心换算：原始值 × 0.0625 = 实际温度（DS18B20官方换算公式）
    // 负温度会自动通过补码实现，无需额外处理
    temp_real = temp_raw * 0.0625f;
    // 格式化温度字符串，保留2位小数（和原输出一致）
    snprintf(temp_buf, buf_size, "%.2f℃", temp_real);

    return 0;
}

// 主函数（每秒读取一次温度，无修改）
int main(int argc, char *argv[])
{
    char temp_buf[TEMP_BUF_SIZE];
    int ret;

    printf("DS18B20 Temperature Sensor Test (RK3568 GPIO3B6)\n");
    printf("Reading temperature every 1 second...\n\n");

    while (1) {
        ret = ds18b20_read_temperature(temp_buf, TEMP_BUF_SIZE);
        if (ret == 0) {
            printf("Current Temperature: %s\n", temp_buf);
        } else {
            printf("Read temperature failed! Retry...\n");
        }
        sleep(1);
    }

    return 0;
}