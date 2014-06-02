# We use -mno-tls-direct-seg-refs to not wrap-around segments, as it
# greatly reduce the speed when running under the Xen hypervisor.
# libc_extra_config_options = $(extra_config_options) --without-__thread --disable-sanity-checks
libc_extra_cflags = -mno-tls-direct-seg-refs -march=i686 -mtune=i586 -g -O3
libc_extra_config_options = $(extra_config_options) --enable-multi-arch
