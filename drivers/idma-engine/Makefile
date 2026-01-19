ARCH ?= riscv
CROSS_COMPILE ?= riscv64-unknown-linux-gnu-

DRIVERS = idma-engine.ko

# Variables for building drivers
obj-m := idma-engine.o
KDIR ?= ../../buildroot/output/build/linux-v5.10.7

.PHONY: all drivers deploy clean
all: drivers
drivers: $(DRIVERS)

%.ko:
	$(MAKE) -C $(KDIR) M=$(PWD) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE)

deploy: all
	@echo "Deploying drivers and applications..."
	@mkdir -p ../../rootfs/root
	@cp $(DRIVERS) ../../rootfs/root/

clean:
	rm -f *.cmd *.ko *.mod.* *.mod *.o .*.cmd modules.order *.symvers
