obj-$(CONFIG_EMAC_DWC_EQOS) += DWC_ETH_QOS_dev.o \
		DWC_ETH_QOS_drv.o \
		DWC_ETH_QOS_desc.o \
		DWC_ETH_QOS_ethtool.o \
		DWC_ETH_QOS_mdio.o \
		DWC_ETH_QOS_eee.o \
		DWC_ETH_QOS_platform.o \
		DWC_ETH_QOS_rgmii_io_macro.o \
		DWC_ETH_QOS_poll_support.o

ifeq ($(CONFIG_PTP_1588_CLOCK), y)
EXTRA_CFLAGS+=-DCONFIG_PTPSUPPORT_OBJ
obj-$(CONFIG_EMAC_DWC_EQOS) += DWC_ETH_QOS_ptp.o
endif

ifeq ($(CONFIG_IPA_OFFLOAD), y)
KBUILD_CFLAGS += -DDWC_ETH_QOS_ENABLE_IPA
obj-$(CONFIG_EMAC_DWC_EQOS) += DWC_ETH_QOS_ipa.o
endif

KBUILD_CFLAGS += -DDWC_ETH_QOS_BUILTIN
