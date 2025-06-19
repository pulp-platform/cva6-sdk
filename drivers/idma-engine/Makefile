ARCH ?= riscv
CROSS_COMPILE ?= riscv64-unknown-linux-gnu-

obj-m := idma-engine.o
KDIR ?= ../../buildroot/output/build/linux-v5.10.7

all:
	$(MAKE) -C $(KDIR) M=$(PWD)

clean:
	rm -f *.cmd *.ko *.mod.* *.mod *.o .*.cmd modules.order *.symvers
