# SPDX-License-Identifier: GPL-2.0

#ccflags-y := -I$(src)/include -I$(src)/../include -I$(src)/../../include -I$(src)/../../../include -I$(src)/../../../common/drv/include
ccflags-y += -I $(srctree)/$(src)/include -I $(srctree)/$(src)/../include -I $(src)/include -I $(src)/../include

common_dir = common

drv_common-y   := $(common_dir)/kds_core.o \
                  $(common_dir)/cu_hls.o \
                  $(common_dir)/xrt_cu.o \
                  $(common_dir)/xrt_xclbin.o

zocl-y := \
	$(drv_common-y) \
	sched_exec.o \
	zocl_sysfs.o \
	zocl_ioctl.o \
	zocl_ert.o \
	zocl_drv.o \
	zocl_bo.o \
	zocl_dma.o \
	zocl_cu.o \
	zocl_mailbox.o \
	zocl_ov_sysfs.o \
	zocl_ospi_versal.o \
	zocl_xclbin.o \
	zocl_sk.o \
	zocl_kds.o \
	cu.o \
	zocl_generic_cu.o

obj-$(CONFIG_DRM_ZOCL)	+= zocl.o
