ifeq ($(CONFIG_BOARD_LMI),y)
CDEFINES += -DUSE_CONT_F0_CALI
KBUILD_CPPFLAGS += $(CDEFINES)
endif

aw8697-haptic-objs = ringbuffer.o aw8697.o

obj-$(CONFIG_INPUT_AW8697_HAPTIC_NEW)	+= aw8697-haptic.o
