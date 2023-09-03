# Copyright 2020 ETH Zurich and University of Bologna.
# Solderpad Hardware License, Version 0.51, see LICENSE for details.
# SPDX-License-Identifier: SHL-0.51
#
# Cyril Koenig <cykoenig@iis.ee.ethz.ch>

BR_OUTPUT_DIR ?= $(realpath ../../../buildroot/output/)
CROSS_COMPILE ?= $(BR_OUTPUT_DIR)/host/bin/riscv64-buildroot-linux-gnu-
KERNEL_DIR    ?= $(BR_OUTPUT_DIR)/build/linux-v5.10.7
ARCH          := riscv

CC            := $(CROSS_COMPILE)gcc

addressability: addressability.o

clean:
	rm -rf *.o addressability