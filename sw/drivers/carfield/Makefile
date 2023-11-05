# Copyright 2020 ETH Zurich and University of Bologna.
# Solderpad Hardware License, Version 0.51, see LICENSE for details.
# SPDX-License-Identifier: SHL-0.51
#
# Cyril Koenig <cykoenig@iis.ee.ethz.ch>

BR_OUTPUT_DIR ?= $(realpath ../../../buildroot/output/)
CROSS_COMPILE ?= $(BR_OUTPUT_DIR)/host/bin/riscv64-buildroot-linux-gnu-
KERNEL_DIR    ?= $(BR_OUTPUT_DIR)/build/linux-5.16.9
ARCH          := riscv

obj-m := carfield.o

all: modules
build: modules

modules:
	make ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) -C $(KERNEL_DIR) M=$(shell pwd) modules

dis: modules
	$(CROSS_COMPILE)objdump -DS carfield.ko > carfield.ko.dump

clean:
	rm -f Module.symvers
	rm -f modules.order
	rm -f *.mod
	rm -rf *.mod.c
	rm -f *.o
	rm -f *.ko
	rm -f *.dump
	rm -f *.cmd
	rm -f .*.cmd

.PHONY: all deploy dis clean build
