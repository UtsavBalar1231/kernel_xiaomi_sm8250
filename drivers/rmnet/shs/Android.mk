ifneq ($(TARGET_PRODUCT),qssi)
RMNET_SHS_DLKM_PLATFORMS_LIST := msmnile
RMNET_SHS_DLKM_PLATFORMS_LIST += kona
RMNET_SHS_DLKM_PLATFORMS_LIST += lito

ifeq ($(call is-board-platform-in-list, $(RMNET_SHS_DLKM_PLATFORMS_LIST)),true)
#Make file to create RMNET_SHS DLKM
LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_CFLAGS := -Wno-macro-redefined -Wno-unused-function -Wall -Werror
LOCAL_CLANG :=true

LOCAL_MODULE_PATH := $(KERNEL_MODULES_OUT)
LOCAL_MODULE := rmnet_shs.ko

LOCAL_SRC_FILES := rmnet_shs_main.c rmnet_shs_config.c rmnet_shs_wq.c rmnet_shs_freq.c rmnet_shs_wq_mem.c rmnet_shs_wq_genl.c

RMNET_SHS_BLD_DIR := ../../vendor/qcom/opensource/data-kernel/drivers/rmnet/shs
DLKM_DIR := ./device/qcom/common/dlkm

KBUILD_OPTIONS := $(RMNET_SHS_BLD_DIR)

$(warning $(DLKM_DIR))
include $(DLKM_DIR)/AndroidKernelModule.mk

endif #End of Check for target
endif #End of Check for qssi target
