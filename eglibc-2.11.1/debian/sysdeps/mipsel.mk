libc_add-ons = ports nptl $(add-ons)

# build 32-bit (n32) alternative library
EGLIBC_PASSES += mipsn32
DEB_ARCH_REGULAR_PACKAGES += libc6-mipsn32 libc6-dev-mipsn32
mipsn32_add-ons = ports nptl $(add-ons)
mipsn32_configure_target = mips32el-linux-gnu
mipsn32_CC = $(CC) -mabi=n32
mipsn32_CXX = $(CXX) -mabi=n32
libc6-mipsn32_shlib_dep = libc6-mipsn32 (>= $(shlib_dep_ver))
mipsn32_slibdir = /lib32
mipsn32_libdir = /usr/lib32
mipsn32_extra_config_options := $(extra_config_options) --disable-profile

# build 64-bit alternative library
EGLIBC_PASSES += mips64
DEB_ARCH_REGULAR_PACKAGES += libc6-mips64 libc6-dev-mips64
mips64_add-ons = ports nptl $(add-ons)
mips64_configure_target = mips64el-linux-gnu
mips64_CC = $(CC) -mabi=64
mips64_CXX = $(CXX) -mabi=64
libc6-mips64_shlib_dep = libc6-mips64 (>= $(shlib_dep_ver))
mips64_slibdir = /lib64
mips64_libdir = /usr/lib64
mips64_extra_config_options := $(extra_config_options) --disable-profile

# Need to put a tri-arch aware version of ldd in the base package
define mipsn32_extra_install
cp debian/tmp-mipsn32/usr/bin/ldd debian/tmp-libc/usr/bin
endef
