do_enforce_all = true
do_libc_dev_package	= false
do_doc_package		= false
do_tools_common		= false
do_tools_host		= false

# Set inconsistent toolchain
export GCC_SUFFIX=-10
# Target
export CC=$(CROSS_COMPILE)gcc$(GCC_SUFFIX)
kmake += CC=$(CC)
