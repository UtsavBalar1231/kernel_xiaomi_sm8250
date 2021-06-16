export PTPSUPPORT CONFIG_PTPSUPPORT_OBJ DWC_ETH_QOS_CONFIG_PTP

#default values
PTPSUPPORT=y	#ptp is enabled

ifeq ($(RELEASE_PACKAGE),1)
EXTRA_CFLAGS+=-DRELEASE_PACKAGE
endif
LBITS ?= $(shell getconf LONG_BIT)
ifeq ($(LBITS),64)
CCFLAGS += -m64
EXTRA_CFLAGS+=-DSYSTEM_IS_64
else
CCFLAGS += -m32
endif

ifeq "$(PTPSUPPORT)" "y"
CONFIG_PTPSUPPORT_OBJ=y
DWC_ETH_QOS_CONFIG_PTP=-DPTPSUPPORT
EXTRA_CFLAGS+=-DCONFIG_PTPSUPPORT_OBJ
else
CONFIG_PTPSUPPORT_OBJ=y
endif

ifeq ($(CONFIG_IPA_OFFLOAD), 1)
CDEFINES+=-DDWC_ETH_QOS_ENABLE_IPA
emac_dwc_eqos-y += DWC_ETH_QOS_ipa.o
endif

obj-m := emac_dwc_eqos.o

emac_dwc_eqos-y += DWC_ETH_QOS_dev.o \
			DWC_ETH_QOS_drv.o \
			DWC_ETH_QOS_desc.o \
			DWC_ETH_QOS_ethtool.o \
			DWC_ETH_QOS_platform.o \
			DWC_ETH_QOS_mdio.o \
			DWC_ETH_QOS_eee.o \
			DWC_ETH_QOS_rgmii_io_macro.o

emac_dwc_eqos-$(CONFIG_PTPSUPPORT_OBJ) += DWC_ETH_QOS_ptp.o

KERNEL_SRC ?= /lib/modules/$(shell uname -r)/build

all:
	$(MAKE) -C $(KERNEL_SRC) M=$(shell pwd) modules $(KBUILD_OPTIONS)

modules_install:
	$(MAKE) -C $(KERNEL_SRC) M=$(shell pwd) modules_install

clean:
	$(MAKE) -C $(KERNEL_SRC) M=$(PWD) clean
