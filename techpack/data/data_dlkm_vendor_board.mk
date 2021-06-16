#Build rmnet perf & shs
DATA_DLKM_BOARD_PLATFORMS_LIST := msmnile
DATA_DLKM_BOARD_PLATFORMS_LIST += kona
DATA_DLKM_BOARD_PLATFORMS_LIST += lito
ifneq ($(TARGET_BOARD_AUTO),true)
ifeq ($(call is-board-platform-in-list,$(DATA_DLKM_BOARD_PLATFORMS_LIST)),true)
BOARD_VENDOR_KERNEL_MODULES += $(KERNEL_MODULES_OUT)/rmnet_shs.ko
BOARD_VENDOR_KERNEL_MODULES += $(KERNEL_MODULES_OUT)/rmnet_perf.ko
endif
endif

