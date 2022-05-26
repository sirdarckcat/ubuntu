#
# Makefile for the Linux kernel ubuntu supplied third-party device drivers.
#

#
# NOTE: to allow drivers to be added and removed without causing merge
# collisions you should add new entries in the middle of the six lines
# of ## at the bottom of the list.  Always add three lines of ## above
# your new entry and maintain the six lines below.
#

##
##
##
##
##
##
obj-$(CONFIG_MALI400) += mali/
##
##
##
obj-y += xilinx-vcu/
##
##
##
obj-y += xilinx-hdmi/
##
##
##
obj-$(CONFIG_NET_VENDOR_S2I) += xilinx-mv-camera-10gige-module/
##
##
##
obj-y += xilinx-mv-camera-sensor-module/
##
##
##
obj-$(CONFIG_UBUNTU_HOST)      += ubuntu-host/
##
##
##
##
##
##

# This is a stupid trick to get kbuild to create ubuntu/built-in.o
obj- += foo.o
