
KERNELDIR := /home/brojackie/RK3568/rk356x-linux/rk356x-linux/kernel
CURRENT_PATH := $(shell pwd)
obj-m := ds18b20_driver.o

ARCH ?= arm64
CROSS_COMPILE ?= /home/brojackie/RK3568/rk356x-linux/rk356x-linux/prebuilts/gcc/linux-x86/aarch64/gcc-arm-10.3-2021.07-x86_64-aarch64-none-linux-gnu/bin/aarch64-none-linux-gnu-

# User-space app
APP := ds18b20_App
APP_SRCS := ds18b20_App.c
APP_CFLAGS ?= -O2
APP_CC := $(CROSS_COMPILE)gcc

.PHONY: build kernel_modules app clean

# default build target: build both kernel module and user app
build: kernel_modules app


kernel_modules:
	$(MAKE) -C $(KERNELDIR) M=$(CURRENT_PATH) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) modules


app: $(APP)

$(APP): $(APP_SRCS)
	$(APP_CC) $(APP_CFLAGS) -o $@ $^


clean:
	$(MAKE) -C $(KERNELDIR) M=$(CURRENT_PATH) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) clean
	rm -f $(APP)
