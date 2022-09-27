human_arch	= ARMv8
build_arch	= arm64
header_arch	= arm64
defconfig	= defconfig
flavours	= iot
build_image	= Image.gz
kernel_file	= arch/$(build_arch)/boot/Image.gz
install_file	= vmlinuz
no_dumpfile = true
uefi_signed     = true

vdso		= vdso_install

do_cloud_tools	= false
do_doc_package	= false
do_extras_package = false
do_libc_dev_package =false
o_source_package = false
do_tools_usbip  = true
do_tools_cpupower = true
do_tools_perf   = true
do_tools_perf_jvmti = true
do_tools_bpftool = true

do_dtbs		= true
