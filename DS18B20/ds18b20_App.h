#ifndef __DS18B20_APP_H
#define __DS18B20_APP_H

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

// DS18B20设备节点（与驱动中创建的一致）
#define DS18B20_DEV_PATH "/dev/ds18b20"
// 温度缓冲区大小
#define TEMP_BUF_SIZE 32

/**
 * @brief  读取DS18B20温度
 * @param  temp_buf: 存储温度的缓冲区（输出）
 * @param  buf_size: 缓冲区大小
 * @retval 0:成功，-1:失败
 */
int ds18b20_read_temperature(char *temp_buf, int buf_size);

#endif /* __DS18B20_APP_H */