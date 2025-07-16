PATCHES_ROOT          := $(shell realpath .)
PATCHES_ALL_MAKEFRAGS := $(foreach file,$(wildcard */*/work.mk),$(PATCHES_ROOT)/$(file))
PATCHES_ALL_DIRS      := $(dir $(PATCHES_ALL_MAKEFRAGS))
PATCHES_WORK_DIR      := $(PATCHES_ROOT)/WORK

COLOR_BLUE := \033[34m
COLOR_RESET := \033[0m

# 1: directory (example: path_to/linux/6.13/)
define patches_gen_rules

# Get words n-1 (package) and n (version) from the given path
tmp_list_$(1) := $$(subst /, ,$(1))
tmp_pkg_$(1)  := $$(word $$(words $$(tmp_list_$(1))), 1st $$(tmp_list_$(1)))
tmp_ver_$(1)  := $$(word $$(words $$(tmp_list_$(1))), $$(tmp_list_$(1)))
# Get tmp_branch and tmp_url
$(eval include $(1)/work.mk)
# Output
tmp_dest_$(1) := $$(PATCHES_WORK_DIR)/$$(tmp_pkg_$(1))/$$(tmp_ver_$(1))

patches_clone_$$(tmp_pkg_$(1))_$$(tmp_ver_$(1)):
	mkdir -p  $$(tmp_dest_$(1))
	git clone $$(tmp_url_$(1))  --branch=$$(tmp_branch_$(1)) --depth=1 $$(tmp_dest_$(1))
	# Todo: check if commit does match, otherwize, unshallow and checkout
	cd $$(tmp_dest_$(1)) && git am $(1)/*.patch
	@echo ---------------
	@echo -e "$(COLOR_BLUE)Done! If you want to use this do:"
	@echo -e "export `echo $$(tmp_pkg_$(1)) | tr 'a-z' 'A-Z'`_OVERRIDE_SRCDIR=$$(tmp_dest_$(1))$(COLOR_RESET)"

endef # patches_gen_rules

$(foreach dir,$(PATCHES_ALL_DIRS),$(eval $(call patches_gen_rules,$(dir))))

patches-help:
	@echo "Try running "make patches_clone_uboot_2021.07""
