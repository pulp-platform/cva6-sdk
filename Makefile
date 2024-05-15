# Copyright 2022 ETH Zurich and University of Bologna.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0
#
# Robert Balas <balasr@iis.ee.ethz.ch>

DTC = dtc

%.dtb: %.dts
	$(DTC) -o $@ $^

.PHONY: setup
setup: target/cheshire/cheshire.dtb
	$(MAKE) -C buildroot BR2_EXTERNAL=.. cheshire_defconfig


$(RISCV)/vmlinux: $(buildroot_defconfig) $(linux_defconfig) $(busybox_defconfig) $(CC)
	mkdir -p $(RISCV)
	make -C buildroot $(buildroot-mk)
	cp buildroot/output/images/vmlinux $@

$(RISCV)/Image: $(RISCV)/vmlinux
	$(OBJCOPY) -O binary -R .note -R .comment -S $< $@

$(RISCV)/Image.gz: $(RISCV)/Image
	$(GZIP_BIN) -9 --force $< > $@

# U-Boot-compatible Linux image
$(RISCV)/uImage: $(RISCV)/Image.gz $(MKIMAGE)
	$(MKIMAGE) -A riscv -O linux -T kernel -a $(UIMAGE_LOAD_ADDRESS) -e $(UIMAGE_ENTRY_POINT) -C gzip -n "CV$(XLEN)A6Linux" -d $< $@

$(RISCV)/u-boot.bin: u-boot/u-boot.bin
	mkdir -p $(RISCV)
	cp $< $@
	# Also bring ELF and build annotated dump into install DIR
	cp u-boot/u-boot $(RISCV)/
	$(TOOLCHAIN_PREFIX)objdump -d -S  u-boot/u-boot > $(RISCV)/u-boot.dump


$(MKIMAGE) u-boot/u-boot.bin: $(CC)
	make -C u-boot pulp-platform_cheshire_defconfig
	make -C u-boot CROSS_COMPILE=$(TOOLCHAIN_PREFIX)

# OpenSBI with u-boot as payload
$(RISCV)/fw_payload.bin: $(RISCV)/u-boot.bin
	make -C opensbi FW_PAYLOAD_PATH=$< $(sbi-mk)
	cp opensbi/build/platform/$(PLATFORM)/firmware/fw_payload.elf $(RISCV)/fw_payload.elf
	cp opensbi/build/platform/$(PLATFORM)/firmware/fw_payload.bin $(RISCV)/fw_payload.bin
	# Also bring in dump
	$(TOOLCHAIN_PREFIX)objdump -d -S  opensbi/build/platform/$(PLATFORM)/firmware/fw_payload.elf > $(RISCV)/fw_payload.dump

$(RISCV)/%.dtb: dts/%.dts
	dtc -o $@ $^

# OpenSBI with linux as payload
$(RISCV)/fw_payload_linux.bin: $(RISCV)/Image $(RISCV)/cheshire.dtb
	make -C opensbi FW_PAYLOAD_PATH=$(RISCV)/Image FW_FDT_PATH=$(RISCV)/cheshire.dtb $(sbi-mk)
	cp opensbi/build/platform/$(PLATFORM)/firmware/fw_payload.elf $(RISCV)/fw_payload_linux.elf
	cp opensbi/build/platform/$(PLATFORM)/firmware/fw_payload.bin $(RISCV)/fw_payload_linux.bin
# $(TOOLCHAIN_PREFIX)objdump -d -S  opensbi/build/platform/$(PLATFORM)/firmware/fw_payload.elf > $(RISCV)/fw_payload.dump


# OpenSBI for Spike with Linux as payload
$(RISCV)/spike_fw_payload.elf: PLATFORM=generic
$(RISCV)/spike_fw_payload.elf: $(RISCV)/Image
	make -C opensbi FW_PAYLOAD_PATH=$< $(sbi-mk)
	cp opensbi/build/platform/$(PLATFORM)/firmware/fw_payload.elf $(RISCV)/spike_fw_payload.elf
	cp opensbi/build/platform/$(PLATFORM)/firmware/fw_payload.bin $(RISCV)/spike_fw_payload.bin

# need to run flash-sdcard with sudo -E, be careful to set the correct SDDEVICE
DT_SECTORSTART 		:= 2048
DT_SECTOREND   		:= 264191	# 2048 + 128M
FW_SECTORSTART 		:= 264192
FW_SECTOREND   		:= 526335	# 264192 + 128M
UIMAGE_SECTORSTART 	:= 526336
UIMAGE_SECTOREND	:= 1050623	# 526336 + 256M
ROOT_SECTORSTART	:= 1050624
ROOT_SECTOREND		:= 0

flash-sdcard: $(RISCV)/fw_payload.bin $(RISCV)/uImage format-sd
	dd if=$(RISCV)/fw_payload.bin of=$(SDDEVICE)2 status=progress oflag=sync bs=1M
	dd if=$(RISCV)/uImage         of=$(SDDEVICE)3 status=progress oflag=sync bs=1M
	mkfs.fat -F32 -n "CHESHIRE" $(SDDEVICE)4
	@echo "Don't forget to flash the device tree binary to $(SDDEVICE)1 :)"

format-sd: $(SDDEVICE)
	@test "$(shell whoami)" = "root" || (echo 'This has to be run with sudo or as root, Ex: sudo -E make flash-sdcard SDDEVICE=/dev/sdc' && exit 1)
	@test -n "$(SDDEVICE)" || (echo 'SDDEVICE must be set, Ex: make flash-sdcard SDDEVICE=/dev/sdc' && exit 1)
	sgdisk --clear -g --new=1:$(DT_SECTORSTART):$(DT_SECTOREND) --new=2:$(FW_SECTORSTART):$(FW_SECTOREND) --new=3:$(UIMAGE_SECTORSTART):$(UIMAGE_SECTOREND) --new=4:$(ROOT_SECTORSTART):$(ROOT_SECTOREND) --typecode=1:b000 --typecode=2:3000 --typecode=3:8300 --typecode=4:8200 $(SDDEVICE)

# specific recipes
gcc: $(CC)
vmlinux: $(RISCV)/vmlinux
fw_payload.bin: $(RISCV)/fw_payload.bin
fw_payload_linux.bin: $(RISCV)/fw_payload_linux.bin
uImage: $(RISCV)/uImage
spike_payload: $(RISCV)/spike_fw_payload.elf

images: $(CC) $(RISCV)/fw_payload_linux.bin $(RISCV)/uImage $(RISCV)/Image

clean:
	rm -rf $(RISCV)/vmlinux
	rm -rf $(RISCV)/fw_payload.bin $(RISCV)/uImage $(RISCV)/Image.gz
	make -C u-boot clean
	make -C opensbi distclean

clean-all: clean
	rm -rf $(RISCV) riscv-isa-sim/build riscv-tests/build
	make -C buildroot clean

.PHONY: gcc vmlinux images help fw_payload.bin uImage

help:
	@echo "usage: $(MAKE) [tool/img] ..."
	@echo ""
	@echo "install compiler with"
	@echo "    make gcc"
	@echo ""
	@echo "install [tool] with compiler"
	@echo "    where tool can be any one of:"
	@echo "        gcc isa-sim tests"
	@echo ""
	@echo "build linux images for cva6"
	@echo "        make images"
	@echo "    for specific artefact"
	@echo "        make [vmlinux|uImage|fw_payload.bin]"
	@echo ""
	@echo "flash firmware and linux images to sd card"
	@echo "    has to be run as root or with sudo -E:"
	@echo "        make flash-sdcard SDDEVICE=/dev/sdX"
	@echo ""
	@echo "There are two clean targets:"
	@echo "    Clean only build object"
	@echo "        make clean"
	@echo "    Clean everything (including toolchain etc)"
	@echo "        make clean-all"
