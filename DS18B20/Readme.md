# 设备树配置

1.rk3568-pinctrl.dtsi（芯片级引脚复用配置文件）

`ds18b20 {`

​    `ds18b20_gpio: ds18b20-gpio {`

​      `rockchip,pins = <3 RK_PB6 RK_FUNC_GPIO &pcfg_pull_up>; /* 用于设置引脚的电气属性 */`

​    `};`

  `};`

2.rk3568-xxxx.dtsi/.dts（开发板定制化设备树文件，根据开发板不同命名不同）

`ds18b20@0 {`

​    `compatible = "rockchip,rk3568-ds18b20";  */\* 与驱动的compatible严格匹配 \*/*`

​    `status = "okay";             */\* 使能节点 \*/*`

​    `ds18b20-gpios = <&gpio3 RK_PB6 GPIO_ACTIVE_HIGH>; */\* GPIO3B6，高电平有效 \*/*`

​    `pinctrl-names = "default";`

​    `pinctrl-0 = <&ds18b20_gpio>;       */\* 引脚复用配置 \*/*`

  `};`

二者的关系为

![relation1](assets/image-20260130104448452.png)



![relation2](assets/image-20260130104529396.png)

3.Makefile文件中

`KERNELDIR := /home/brojackie/RK3568/rk356x-linux/rk356x-linux/kernel`

和

`CROSS_COMPILE ?= /home/brojackie/RK3568/rk356x-linux/rk356x-linux/prebuilts/gcc/linux-x86/aarch64/gcc-arm-10.3-2021.07-x86_64-aarch64-none-linux-gnu/bin/aarch64-none-linux-gnu-`

根据个人的文件文件位置进行更改



更改完成后使用`make`指令并将编译出的`ds18b20_driver.ko`文件和`ds18b20_App`文件导入开发板的`/lib/modules/5.10.160`(根据自己开发板选择)，然后使用`depmod`命令加载模块依赖，使用`modprobe ds18b20_driver.ko` 加载模块，使用`./ds18b20_App`指令执行app文件，就可以开始读取温度。

正常工作时

![running](assets/image-20260130105651003.png)

如果报错优先检查设备树上引脚是不是被占用，其他情况根据具体报错内容修改，驱动文件应该没有问题。
