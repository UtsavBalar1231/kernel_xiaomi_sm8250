#This makefile is only to compile EMAC for AUTO platform

ifeq ($(TARGET_BOARD_AUTO),true)
LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE       := emac_rps_settings.sh
LOCAL_MODULE_TAGS  := optional
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_PATH  := $(TARGET_OUT_VENDOR_EXECUTABLES)
LOCAL_SRC_FILES    := emac_rps_settings.sh
include $(BUILD_PREBUILT)

endif


