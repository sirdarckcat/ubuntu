human_arch	= ARMv8
build_arch	= arm64
header_arch	= arm64
defconfig	= defconfig
flavours	= bluefield
build_image_bluefield	= Image
kernel_file_bluefield	= arch/$(build_arch)/boot/Image
install_file	= vmlinuz
no_dumpfile = true
uefi_signed	= true

loader		= grub
vdso		= vdso_install

do_extras_package = true
do_tools_usbip  = true
do_tools_cpupower = true
do_tools_perf   = true
do_tools_perf_jvmti = true
do_tools_bpftool = true
do_enforce_all      = false

do_dtbs		= true
do_zfs		= true
do_dkms_nvidia	= false
do_dkms_nvidia_server = false
do_dkms_vbox	= false
do_dkms_wireguard = false

disable_d_i = true
do_source_package = false
do_common_headers_indep = false
do_libc_dev_package = false
