
obj-$(CONFIG_PROVENCORE_REE) += pnc_ree.o
pnc_ree-objs := main.o session.o shm.o smc.o

ccflags-y := \
	-Wno-declaration-after-statement \
	-Wno-unused-function \
	-Wno-error=date-time

ccflags-y += -I${srctree}/${src}/../../../include

CC_VERSION := $(shell $(CC) -dumpversion)
IS_VER_GE = $(shell printf "$2\n$1" | sort --version-sort --check=quiet && echo YES)

ifeq ($(ARCH), arm64)
ifeq ($(call IS_VER_GE,$(CC_VERSION),10.1), YES)
ccflags-y += -mno-outline-atomics
endif
endif

#ccflags-y := -DDEBUG
