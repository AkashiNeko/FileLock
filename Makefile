obj-m += filelock.o
ccflags-y := -std=gnu11

# 内核源代码目录
KERNELDIR ?= /lib/modules/$(shell uname -r)/build

# 默认目标
all:
	make -C $(KERNELDIR) M=$(PWD) modules

# 清理生成的文件
clean:
	make -C $(KERNELDIR) M=$(PWD) clean

# 编译模块
modules:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules

# 加载模块
load:
	sudo insmod filelock.ko

# 卸载模块
unload:
	sudo rmmod filelock

# 重新编译模块
rebuild: clean modules load

# 卸载模块并重新加载
reinstall: unload rebuild load