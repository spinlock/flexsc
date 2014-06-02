libc_MIN_KERNEL_SUPPORTED = 2.4.1
libc_add-ons = ports linuxthreads $(add-ons)
libc_extra_config_options = $(extra_config_options) --disable-sanity-checks --without-__thread --without-tls
