ARCH ?= riscv
CROSS_COMPILE ?= riscv64-unknown-linux-gnu-

DRIVERS = idma-proxy.ko
APPS = example-kernel-buffer example-copy-buffer example-user-direct

# Variables for building drivers
obj-m := idma-proxy.o
KDIR ?= ../../buildroot/output/build/linux-v5.10.7

# Variables for building applications
APP_CC = $(CROSS_COMPILE)gcc
APP_COMMON_OBJS = 
APP_SYSROOT_DIR = ../../buildroot/output/host/riscv64-buildroot-linux-gnu/sysroot
APP_CFLAGS = --sysroot=$(APP_SYSROOT_DIR) -I$(APP_SYSROOT_DIR)/usr/include -Wall -O2
APP_LDFLAGS = --sysroot=$(APP_SYSROOT_DIR) -L$(APP_SYSROOT_DIR)/usr/lib

.PHONY: all drivers apps deploy clean
all: drivers apps
drivers: $(DRIVERS)
apps: $(APPS)

%.ko:
	$(MAKE) -C $(KDIR) M=$(PWD) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE)

$(APPS): %: %.o $(APP_COMMON_OBJS)
	$(APP_CC) $(APP_LDFLAGS) -o $@ $^

%.o: %.c
	$(APP_CC) $(APP_CFLAGS) -c $< -o $@

deploy: all
	@echo "Deploying drivers and applications..."
	@mkdir -p ../../rootfs/root
	@cp $(DRIVERS) ../../rootfs/root/
	@cp $(APPS) ../../rootfs/root/

clean:
	rm -f *.cmd *.ko *.mod.* *.mod *.o .*.cmd modules.order *.symvers $(APPS)
