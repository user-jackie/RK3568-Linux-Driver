#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/ide.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/gpio.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <linux/semaphore.h>
#include <linux/timer.h>
#include <linux/irq.h>
#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/fs.h>
#include <linux/fcntl.h>
#include <linux/platform_device.h>

#define DS18B20_NAME "ds18b20"
#define DS18B20_CNT 1

// 全局设备结构体（无任何浮点成员）
struct ds18b20_dev {
    dev_t devid;              // 字符设备号
    struct cdev cdev;         // 字符设备
    struct class *class;      // 设备类
    struct device *device;    // 设备节点
    struct device *dev;       // 平台设备
    int major;              // 主设备号
    int minor;              // 次设备号
    int ds18b20_gpio;        // LED GPIO编号
    struct device_node *nd;   // 设备节点
};
static struct ds18b20_dev ds18b20_dev;

/********************* DS18B20单总线核心时序（无修改，保留原逻辑）*********************/
// 设置GPIO为输出模式
static void ds18b20_gpio_output(void)
{
    gpio_direction_output(ds18b20_dev.ds18b20_gpio, 1);
}

// 设置GPIO为输入模式
static void ds18b20_gpio_input(void)
{
    gpio_direction_input(ds18b20_dev.ds18b20_gpio);
}

// 设置数据口电平
static void ds18b20_set_gpio(int val)
{
    gpio_set_value(ds18b20_dev.ds18b20_gpio, val);
}

// 读取数据口电平
static int ds18b20_get_gpio(void)
{
    return gpio_get_value(ds18b20_dev.ds18b20_gpio);
}

// DS18B20复位（单总线时序核心）
static int ds18b20_reset(void)
{
    int ret;
    // 1. 主机拉低总线480~960us
    ds18b20_gpio_output();
    ds18b20_set_gpio(0);
    udelay(600);
    // 2. 主机释放总线，等待15~60us
    ds18b20_set_gpio(1);
    ds18b20_gpio_input();
    udelay(80);
    // 3. 检测从机应答（拉低总线60~240us）
    ret = ds18b20_get_gpio();
    // 4. 等待总线释放
    udelay(500);
    return ret; // 0:复位成功，1:失败
}

// 单总线读1bit
static u8 ds18b20_read_bit(void)
{
    u8 bit = 0;
    // 主机拉低总线至少1us
    ds18b20_gpio_output();
    ds18b20_set_gpio(0);
    udelay(2);
    // 主机释放总线
    ds18b20_set_gpio(1);
    ds18b20_gpio_input();
    udelay(10);
    // 读取总线电平
    if (ds18b20_get_gpio())
        bit = 1;
    else
        bit = 0;
    // 等待时序完成（至少45us）
    udelay(50);
    return bit;
}

// 单总线读1字节
static u8 ds18b20_read_byte(void)
{
    u8 i, byte = 0;
    for (i = 0; i < 8; i++) {
        byte >>= 1;
        if (ds18b20_read_bit())
            byte |= 0x80;
    }
    return byte;
}

// 单总线写1字节
static void ds18b20_write_byte(u8 byte)
{
    u8 i, bit;
    ds18b20_gpio_output();
    for (i = 0; i < 8; i++) {
        bit = byte & 0x01;
        byte >>= 1;
        // 写0：拉低总线60~120us
        if (bit == 0) {
            ds18b20_set_gpio(0);
            udelay(80);
            ds18b20_set_gpio(1);
            udelay(2);
        } else { // 写1：拉低总线1~15us，然后释放
            ds18b20_set_gpio(0);
            udelay(2);
            ds18b20_set_gpio(1);
            udelay(80);
        }
    }
}

/********************* DS18B20温度读取（纯整数，无浮点！核心修改）*********************/
// 读取DS18B20原始16位整数数据，无任何浮点运算
// temp_raw：输出参数，存储原始16位温度值（DS18B20直接返回的原始数据）
static int ds18b20_read_temp(int *temp_raw)
{
    u8 temp_h, temp_l;
    u16 temp_val;
    // 1. 复位DS18B20，失败直接返回
    if (ds18b20_reset() != 0) {
        printk("DS18B20 reset failed!\n");
        return -EIO;
    }
    // 2. 跳过ROM（单设备挂载专用，多设备需修改为读ROM）
    ds18b20_write_byte(0xCC);
    // 3. 发送温度转换命令，等待转换完成（最大750ms，内核mdelay更安全）
    ds18b20_write_byte(0x44);
    mdelay(750);
    // 4. 再次复位，准备读取原始数据
    if (ds18b20_reset() != 0) {
        printk("DS18B20 reset failed!\n");
        return -EIO;
    }
    // 5. 跳过ROM，发送读数据命令
    ds18b20_write_byte(0xCC);
    ds18b20_write_byte(0xBE);
    // 6. 读取原始温度数据（低字节+高字节，16位原始值）
    temp_l = ds18b20_read_byte();
    temp_h = ds18b20_read_byte();
    temp_val = (temp_h << 8) | temp_l;
    // 7. 直接传递原始16位整数，无任何浮点运算！
    *temp_raw = (int)temp_val;
    return 0;
}

/********************* 字符设备操作接口（无浮点，核心修改）*********************/
static int ds18b20_open(struct inode *inode, struct file *filp)
{
    return 0;
}

static ssize_t ds18b20_read(struct file *filp, char __user *buf, size_t size, loff_t *ppos)
{
    int temp_raw;  // 原始16位整数温度，无浮点
    char temp_buf[32];
    int ret;

    // 读取DS18B20原始整数温度（无任何浮点运算）
    ret = ds18b20_read_temp(&temp_raw);
    if (ret < 0)
        return ret;

    // 仅格式化整数为字符串，内核态安全（无浮点）
    snprintf(temp_buf, sizeof(temp_buf), "%d\n", temp_raw);
    // 拷贝原始整数字符串到用户空间
    ret = copy_to_user(buf, temp_buf, strlen(temp_buf));
    if (ret < 0) {
        printk("copy to user failed!\n");
        return -EFAULT;
    }
    // 防止重复读取
    if (*ppos > 0)
        return 0;
    *ppos += strlen(temp_buf);
    return strlen(temp_buf);
}

static int ds18b20_release(struct inode *inode, struct file *filp)
{
    return 0;
}

// 字符设备操作集
static const struct file_operations ds18b20_fops = {
    .owner = THIS_MODULE,
    .open = ds18b20_open,
    .read = ds18b20_read,
    .release = ds18b20_release,
};

/* 获取并初始化 DS18B20 使用的 GPIO（保留与现有错误路径兼容的接口） */
static int ds18b20_gpio_init(struct device_node *nd)
{
    int ret;

    /* 从设备树读取名为 "ds18b20-gpios" 的属性（第 0 个 GPIO）*/
    ds18b20_dev.ds18b20_gpio = of_get_named_gpio(nd, "ds18b20-gpios", 0);
    if (ds18b20_dev.ds18b20_gpio < 0) {
        pr_err("ds18b20: can't get gpio from DT\n");
        return -EINVAL;
    }
    pr_info("ds18b20: gpio num = %d\n", ds18b20_dev.ds18b20_gpio);

    /* 请求并配置为输出（初始置高） */
    ret = gpio_request(ds18b20_dev.ds18b20_gpio, "ds18b20");
    if (ret) {
        pr_err("ds18b20: gpio_request failed, ret=%d\n", ret);
        return ret;
    }
    ret = gpio_direction_output(ds18b20_dev.ds18b20_gpio, 1);
    if (ret) {
        pr_err("ds18b20: gpio_direction_output failed\n");
        gpio_free(ds18b20_dev.ds18b20_gpio);
        return -EINVAL;
    }

    return 0;
}

static int ds18b20_probe(struct platform_device *pdev)
{
    int ret;
    
    ret = ds18b20_gpio_init(pdev->dev.of_node);
    if(ret < 0)
        return ret;

    // 2. 动态分配字符设备号
    ret = alloc_chrdev_region(&ds18b20_dev.devid, 0, DS18B20_CNT, DS18B20_NAME);
    if (ret < 0) {
        printk("alloc chrdev region failed!\n");
        goto free_gpio;
    }
    // 3. 初始化字符设备
    cdev_init(&ds18b20_dev.cdev, &ds18b20_fops);
    ds18b20_dev.cdev.owner = THIS_MODULE;
    ret = cdev_add(&ds18b20_dev.cdev, ds18b20_dev.devid, 1);
    if (ret < 0) {
        printk("cdev add failed!\n");
        goto unregister_chrdev;
    }
    // 4. 创建设备类和设备节点（自动生成/dev/ds18b20）
    ds18b20_dev.class = class_create(THIS_MODULE, "ds18b20_class");
    if (IS_ERR(ds18b20_dev.class)) {
        printk("class create failed!\n");
        goto del_cdev;
    }
    ds18b20_dev.device = device_create(ds18b20_dev.class, NULL, ds18b20_dev.devid, NULL, DS18B20_NAME);
    if (IS_ERR(ds18b20_dev.device)) {
        printk("device create failed!\n");
        goto destroy_class;
    }
    printk("ds18b20 driver probe success!\n");
    return 0;

destroy_class:
    class_destroy(ds18b20_dev.class);
del_cdev:
    cdev_del(&ds18b20_dev.cdev);
unregister_chrdev:
    unregister_chrdev_region(ds18b20_dev.devid, 1);
free_gpio:
    gpio_free(ds18b20_dev.ds18b20_gpio);
    return -EIO;
}

static int ds18b20_remove(struct platform_device *pdev)
{
    // 释放资源
    device_destroy(ds18b20_dev.class, ds18b20_dev.devid);
    class_destroy(ds18b20_dev.class);
    cdev_del(&ds18b20_dev.cdev);
    unregister_chrdev_region(ds18b20_dev.devid, 1);
    gpio_free(ds18b20_dev.ds18b20_gpio);
    printk("ds18b20 driver removed!\n");
    return 0;
}

static const struct of_device_id rk3568_ds18b20_of_match[] = {
    { .compatible = "rockchip,rk3568-ds18b20" },
    { }
};
MODULE_DEVICE_TABLE(of, rk3568_ds18b20_of_match);

static struct platform_driver rk3568_ds18b20_driver = {
    .driver = {
        .name = "rk3568-ds18b20",
        .of_match_table = rk3568_ds18b20_of_match,
    },
    .probe = ds18b20_probe,
    .remove = ds18b20_remove,
};

static int __init ds18b20_driver_init(void)
{
    return platform_driver_register(&rk3568_ds18b20_driver);
}

static void __exit ds18b20_driver_exit(void)
{
    platform_driver_unregister(&rk3568_ds18b20_driver);
}

module_init(ds18b20_driver_init);
module_exit(ds18b20_driver_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Liaoyuan");
