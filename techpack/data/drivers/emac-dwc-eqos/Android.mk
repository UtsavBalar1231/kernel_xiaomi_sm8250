#This makefile is only to compile EMAC for AUTO platform

ifeq ($(TARGET_BOARD_AUTO),true)
LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

# This makefile is only for DLKM
ifneq ($(findstring vendor,$(LOCAL_PATH)),)

ifneq ($(findstring opensource,$(LOCAL_PATH)),)
EMAC_BLD_DIR := ../../vendor/qcom/opensource/data-kernel/drivers/emac-dwc-eqos
endif # opensource

LOCAL_MODULE_PATH := $(KERNEL_MODULES_OUT)

DLKM_DIR := ./device/qcom/common/dlkm
KBUILD_OPTIONS := $(EMAC_BLD_DIR)
KBUILD_OPTIONS += DCONFIG_PTPSUPPORT_OBJ=1
KBUILD_OPTIONS += DCONFIG_DEBUGFS_OBJ=1
#KBUILD_OPTIONS += DDWC_ETH_QOS_TEST=1

LOCAL_MODULE      := emac_dwc_eqos.ko
LOCAL_MODULE_TAGS := optional
include $(DLKM_DIR)/AndroidKernelModule.mk

include $(CLEAR_VARS)
LOCAL_MODULE       := emac_perf_settings.sh
LOCAL_MODULE_TAGS  := optional
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_PATH  := $(TARGET_OUT_DATA)/emac
LOCAL_SRC_FILES    := emac_perf_settings.sh
include $(BUILD_PREBUILT)

endif
endif


