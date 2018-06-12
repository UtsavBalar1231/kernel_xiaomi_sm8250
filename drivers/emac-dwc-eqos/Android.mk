#This makefile is only to compile EMAC for AUTO platform

ifeq ($(TARGET_BOARD_AUTO),true)
LOCAL_PATH := $(call my-dir)

# This makefile is only for DLKM
ifneq ($(findstring vendor,$(LOCAL_PATH)),)

ifneq ($(findstring opensource,$(LOCAL_PATH)),)
EMAC_BLD_DIR := $(ANDROID_BUILD_TOP)/vendor/qcom/opensource/data-kernel/drivers/emac-dwc-eqos
endif # opensource

DLKM_DIR := $(TOP)/device/qcom/common/dlkm
KBUILD_OPTIONS := $(EMAC_BLD_DIR)
KBUILD_OPTIONS += DCONFIG_PTPSUPPORT_OBJ=1
KBUILD_OPTIONS += DCONFIG_DEBUGFS_OBJ=1
#KBUILD_OPTIONS += DDWC_ETH_QOS_TEST=1

include $(CLEAR_VARS)
LOCAL_MODULE      := emac_dwc_eqos.ko
LOCAL_MODULE_TAGS := debug
include $(DLKM_DIR)/AndroidKernelModule.mk
endif
endif


