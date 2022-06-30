# SPDX-License-Identifier: GPL-2.0
#
# Makefile for the Entropy Source and DRNG Manager.
#

obj-y					+= lrng_es_mgr.o lrng_drng_mgr.o \
					   lrng_es_aux.o
obj-$(CONFIG_LRNG_SHA256)		+= lrng_sha256.o
obj-$(CONFIG_LRNG_SHA1)			+= lrng_sha1.o

obj-$(CONFIG_SYSCTL)			+= lrng_proc.o
obj-$(CONFIG_LRNG_SYSCTL)		+= lrng_sysctl.o
obj-$(CONFIG_NUMA)			+= lrng_numa.o

obj-$(CONFIG_LRNG_SWITCH)		+= lrng_switch.o
obj-$(CONFIG_LRNG_HASH_KCAPI)		+= lrng_hash_kcapi.o
obj-$(CONFIG_LRNG_DRNG_CHACHA20)	+= lrng_drng_chacha20.o
obj-$(CONFIG_LRNG_DRBG)			+= lrng_drng_drbg.o
obj-$(CONFIG_LRNG_DRNG_KCAPI)		+= lrng_drng_kcapi.o
obj-$(CONFIG_LRNG_DRNG_ATOMIC)		+= lrng_drng_atomic.o

obj-$(CONFIG_LRNG_TIMER_COMMON)		+= lrng_es_timer_common.o
obj-$(CONFIG_LRNG_IRQ)			+= lrng_es_irq.o
obj-$(CONFIG_LRNG_KERNEL_RNG)		+= lrng_es_krng.o
obj-$(CONFIG_LRNG_SCHED)		+= lrng_es_sched.o
obj-$(CONFIG_LRNG_CPU)			+= lrng_es_cpu.o
obj-$(CONFIG_LRNG_JENT)			+= lrng_es_jent.o

obj-$(CONFIG_LRNG_HEALTH_TESTS)		+= lrng_health.o
obj-$(CONFIG_LRNG_TESTING)		+= lrng_testing.o
obj-$(CONFIG_LRNG_SELFTEST)		+= lrng_selftest.o

obj-$(CONFIG_LRNG_COMMON_DEV_IF)	+= lrng_interface_dev_common.o
obj-$(CONFIG_LRNG_RANDOM_IF)		+= lrng_interface_random_user.o \
					   lrng_interface_random_kernel.o \
					   lrng_interface_aux.o
obj-$(CONFIG_LRNG_KCAPI_IF)		+= lrng_interface_kcapi.o
obj-$(CONFIG_LRNG_DEV_IF)		+= lrng_interface_dev.o
obj-$(CONFIG_LRNG_HWRAND_IF)		+= lrng_interface_hwrand.o
