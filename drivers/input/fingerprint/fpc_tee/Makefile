ifneq ($(filter y, $(CONFIG_BOARD_APOLLO) $(CONFIG_BOARD_MUNCH)),)
obj-$(CONFIG_FINGERPRINT_FPC_TEE) += fpc1020_tee_old.o
else
obj-$(CONFIG_FINGERPRINT_FPC_TEE) += fpc1020_tee.o
endif
