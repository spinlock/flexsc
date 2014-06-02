libc_rtlddir = /lib64
libc_extra_cflags = -mcpu=ultrasparc

# build a sparcv9b optimized library
EGLIBC_PASSES += sparcv9b
DEB_ARCH_REGULAR_PACKAGES += libc6-sparcv9b
sparcv9b_add-ons = nptl $(add-ons)
sparcv9b_configure_target=sparc64b-linux-gnu
sparcv9b_configure_build=sparc64b-linux-gnu
sparcv9b_extra_cflags = -mcpu=ultrasparc3
sparcv9b_extra_config_options = $(extra_config_options) --disable-profile
sparcv9b_rtlddir = /lib
sparcv9b_slibdir = /lib/ultra3

# /lib64 and /usr/lib64 are provided by glibc instead base-files: #259302.
define libc6_extra_pkg_install
ln -sf lib debian/$(curpass)/lib64
ln -sf lib debian/$(curpass)/usr/lib64
endef
