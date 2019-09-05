ifeq ($(CONFIG_BUILD_ARM64_DT_OVERLAY),y)
        dtbo-$(CONFIG_ARCH_KONA) += \
                kona-cdp-overlay.dtbo \
                kona-cdp-lcd-overlay.dtbo \
                kona-mtp-overlay.dtbo \
                kona-mtp-ws-overlay.dtbo \
                kona-rumi-overlay.dtbo \
                kona-qrd-overlay.dtbo \
                kona-hdk-overlay.dtbo

kona-cdp-overlay.dtbo-base := kona.dtb kona-v2.dtb
kona-cdp-lcd-overlay.dtbo-base := kona.dtb kona-v2.dtb
kona-mtp-overlay.dtbo-base := kona.dtb kona-v2.dtb
kona-mtp-ws-overlay.dtbo-base := kona.dtb kona-v2.dtb
kona-rumi-overlay.dtbo-base := kona.dtb kona-v2.dtb
kona-qrd-overlay.dtbo-base := kona.dtb kona-v2.dtb
kona-hdk-overlay.dtbo-base := kona.dtb kona-v2.dtb
else
dtb-$(CONFIG_ARCH_KONA) += kona-rumi.dtb \
        kona-mtp.dtb \
        kona-mtp-ws.dtb \
        kona-cdp.dtb \
        kona-cdp-lcd.dtb \
        kona-qrd.dtb \
        kona-v2-rumi.dtb \
        kona-v2-mtp.dtb \
        kona-v2-mtp-ws.dtb \
        kona-v2-cdp.dtb \
        kona-v2-qrd.dtb \
        kona-hdk.dtb
endif

ifeq ($(CONFIG_BUILD_ARM64_DT_OVERLAY),y)
dtbo-$(CONFIG_ARCH_LITO) += lito-rumi-overlay.dtbo \
                lito-mtp-overlay.dtbo \
                lito-cdp-overlay.dtbo \
                lito-atp-overlay.dtbo \
                lito-qrd-overlay.dtbo

lito-rumi-overlay.dtbo-base := lito.dtb lito-v2.dtb
lito-mtp-overlay.dtbo-base := lito.dtb lito-v2.dtb
lito-cdp-overlay.dtbo-base := lito.dtb lito-v2.dtb
lito-atp-overlay.dtbo-base := lito.dtb lito-v2.dtb
lito-qrd-overlay.dtbo-base := lito.dtb lito-v2.dtb
else
dtb-$(CONFIG_ARCH_LITO) += lito-rumi.dtb \
                lito-mtp.dtb \
                lito-cdp.dtb \
                lito-atp.dtb \
                lito-qrd.dtb \
                lito-v2-mtp.dtb \
                lito-v2-cdp.dtb \
                lito-v2-atp.dtb \
                lito-v2-qrd.dtb
endif


ifeq ($(CONFIG_BUILD_ARM64_DT_OVERLAY),y)
        dtbo-$(CONFIG_ARCH_BENGAL) += \
                bengal-rumi-overlay.dtbo

bengal-rumi-overlay.dtbo-base := bengal.dtb
else
dtb-$(CONFIG_ARCH_BENGAL) += bengal-rumi.dtb
endif

always		:= $(dtb-y)
subdir-y	:= $(dts-dirs)
clean-files    := *.dtb *.dtbo
