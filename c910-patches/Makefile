# Makefile for C910 SDK Integration
SHELL := /bin/bash
.PHONY: all init-submodules apply-patches force-apply-patches build clean

PATCH_DIR := .
SDK_DIR := ../
STAMP_DIR := .patches_applied

PATCHES := $(wildcard $(PATCH_DIR)/*.patch)
STAMPS := $(patsubst $(PATCH_DIR)/%.patch,$(STAMP_DIR)/%.stamp,$(PATCHES))

all: init-submodules apply-patches

init-submodules:
	@echo "Initializing submodules..."
	git submodule update --init --recursive

$(STAMP_DIR):
	@mkdir -p $@

# Standard patch application with stamps
$(STAMP_DIR)/%.stamp: $(PATCH_DIR)/%.patch $(STAMP_DIR)
	@echo "Checking patch: $<"
	@patch_name=$$(basename "$<"); \
	submodule_dir=$$(echo "$$patch_name" | sed 's/.patch//'); \
	cd $(SDK_DIR)/$$submodule_dir && \
	if git apply --check ../c910-patches/$< 2>/dev/null; then \
		echo "Applying patch: $<"; \
		git apply ../c910-patches/$<; \
		touch ../c910-patches/$@; \
	else \
		echo "Patch already applied or invalid: $<"; \
	fi; \

# Explicit apply-patches target
apply-patches: init-submodules $(STAMPS)
	@echo "Patch application complete"

# Force re-application of all patches
force-apply-patches: clean
	@rm -rf $(STAMP_DIR)
	@$(MAKE) apply-patches

build: apply-patches
	@echo "Building images..."
	$(MAKE) -C $(SDK_DIR) images

clean:
	@echo "Cleaning build artifacts..."
	$(MAKE) -C $(SDK_DIR) clean
	@echo "Removing patch stamps..."
	rm -rf $(STAMP_DIR)
	@echo "Resetting submodules..."
	git submodule foreach --recursive "git reset --hard && git clean -fdx"
	git submodule update --init --recursive

status:
	git submodule status --recursive