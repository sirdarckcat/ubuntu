
export PENSANDO_DRV_VER := 22.11.1-001

subdir-ccflags-y					:= -I$(PWD)/ubuntu/pensando-dsc-drivers/drivers/common -Ddrv_ver=\"$(PENSANDO_DRV_VER)\"
obj-$(CONFIG_IONIC)			+= drivers/linux/eth/ionic/
obj-$(CONFIG_IONIC_MNIC)		+= drivers/linux/eth/ionic/
obj-$(CONFIG_MDEV)			+= drivers/linux/mdev/
obj-$(CONFIG_MNET_UIO_PDRV_GENIRQ)	+= drivers/linux/mnet_uio_pdrv_genirq/

obj-y					+= drivers/linux/pciesvc/
