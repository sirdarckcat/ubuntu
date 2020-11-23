human_arch	= 64 bit x86
build_arch	= x86
header_arch	= $(build_arch)
defconfig	= defconfig
flavours	= gkeop
build_image	= bzImage
kernel_file	= arch/$(build_arch)/boot/bzImage
install_file	= vmlinuz
loader		= grub
vdso		= vdso_install
no_dumpfile	= true
uefi_signed     = true

do_tools_usbip        = true
do_tools_cpupower     = true
do_tools_perf         = true
do_tools_perf_jvmti   = true
do_tools_bpftool      = true
do_tools_x86	      = true
do_tools_hyperv	      = true
do_tools_host         = false
do_tools_common       = false
do_tools_acpidbg      = true

do_zfs		      = true
do_dkms_nvidia        = false
do_dkms_nvidia_server = false
do_dkms_vbox          = true
do_dkms_wireguard     = true

do_libc_dev_package   = false
do_doc_package        = false
do_extras_package     = true
disable_d_i           = true

do_enforce_all        = true
