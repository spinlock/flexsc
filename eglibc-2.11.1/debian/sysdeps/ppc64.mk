libc_slibdir = /lib
libc_libdir = /usr/lib
libc_rtlddir = /lib64

# /lib64 and /usr/lib64 are provided as symlinks 
define libc6_extra_pkg_install
ln -sf /lib debian/$(curpass)/lib64
ln -sf lib debian/$(curpass)/usr/lib64
endef

# build 32-bit (powerpc) alternative library
EGLIBC_PASSES += powerpc
DEB_ARCH_REGULAR_PACKAGES += libc6-powerpc libc6-dev-powerpc
libc6-powerpc_shlib_dep = libc6-powerpc (>= $(shlib_dep_ver))
powerpc_add-ons = nptl $(add-ons)
powerpc_configure_target = powerpc-linux-gnu
powerpc_CC = $(CC) -m32
powerpc_CXX = $(CXX) -m32
powerpc_includedir = /usr/include/powerpc-linux-gnu
powerpc_extra_config_options := $(extra_config_options) --disable-profile
powerpc_rtlddir = /lib
powerpc_slibdir = /lib32
powerpc_libdir = /usr/lib32

define libc6-dev-powerpc_extra_pkg_install
mkdir -p debian/libc6-dev-powerpc/usr/include/gnu
cp -af debian/tmp-powerpc/usr/include/powerpc-linux-gnu/gnu/stubs-32.h \
        debian/libc6-dev-powerpc/usr/include/gnu
mkdir -p debian/libc6-dev-powerpc/usr/include/powerpc-linux-gnu
endef

# create a symlink for the 32 bit dynamic linker in /lib
define libc6-powerpc_extra_pkg_install
mkdir -p debian/$(curpass)/lib
ln -s /lib32/ld.so.1 debian/$(curpass)/lib
endef
