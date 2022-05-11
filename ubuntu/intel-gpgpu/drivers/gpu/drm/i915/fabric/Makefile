#
# INTEL CONFIDENTIAL
#
# Copyright (C) 2020-2022 Intel Corporation
#
# This software and the related documents are Intel copyrighted materials,
# and your use of them is governed by the express license under which they
# were provided to you ("LICENSE"). Unless the LICENSE provides otherwise,
# you may not use, modify, copy, publish, distribute, disclose or transmit
# this software or the related documents without Intel's prior written
# permission.
#
# This software and the related documents are provided as is, with no
# express or implied warranties, other than those that are expressly
# stated in the License.
#
# SPDX-License-Identifier: MIT
#
# Copyright(c) 2020 - 2022 Intel Corporation.
#
# Intel Connectivity fabric driver
#
#

obj-$(CPTCFG_DRM_I915_INTEL_FABRIC) += iaf.o

iaf-y := diagnostics.o \
	 fw.o \
	 main.o \
	 mbdb.o \
	 netlink.o \
	 ops.o \
	 parallel.o \
	 port.o \
	 routing_debug.o \
	 routing_engine.o \
	 routing_event.o \
	 routing_io.o \
	 routing_logic.o \
	 routing_p2p.o \
	 routing_pause.o \
	 routing_topology.o \
	 statedump.o \
	 sysfs.o \
	 trace.o \

iaf-$(CPTCFG_IAF_ANTI_ROLLBACK) += mei_iaf_user.o

iaf-$(CPTCFG_IAF_DEBUG_MBOX_ACCESS) += mbox.o

#export CPTCFG_IAF_DEBUG_SELFTESTS=y
subdir-ccflags-$(CPTCFG_IAF_DEBUG_SELFTESTS) += -DCPTCFG_IAF_DEBUG_SELFTESTS
iaf-$(CPTCFG_IAF_DEBUG_SELFTESTS) += selftests/selftest.o\
	    selftests/routing_selftest.o\
	    selftests/routing_mock.o\

CFLAGS_trace.o = -I$(src)
