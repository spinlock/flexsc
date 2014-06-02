# We use -mno-tls-direct-seg-refs to not wrap-around segments, as it
# greatly reduce the speed when running under the Xen hypervisor.
# libc_extra_config_options = $(extra_config_options) --without-__thread --disable-sanity-checks
libc_extra_cflags = -mno-tls-direct-seg-refs
libc_extra_config_options = $(extra_config_options) --enable-multi-arch

# We use -march=i686 and glibc's i686 routines use cmov, so require it.
# A Debian-local glibc patch adds cmov to the search path.
# The optimized libraries also use NPTL!
EGLIBC_PASSES += i686
DEB_ARCH_REGULAR_PACKAGES += libc6-i686
i686_add-ons = nptl $(add-ons)
i686_configure_target=i686-linux-gnu
i686_extra_cflags = -march=i686 -mtune=generic
i686_rtlddir = /lib
i686_slibdir = /lib/tls/i686/cmov
i686_extra_config_options = $(extra_config_options) --disable-profile --enable-multi-arch

# We use -mno-tls-direct-seg-refs to not wrap-around segments, as it
# greatly increase the speed when running under the 32bit Xen hypervisor.
EGLIBC_PASSES += xen
DEB_ARCH_REGULAR_PACKAGES += libc6-xen
xen_add-ons = nptl $(add-ons)
xen_configure_target=i686-linux-gnu
xen_extra_cflags = -march=i686 -mtune=generic -mno-tls-direct-seg-refs
xen_rtlddir = /lib
xen_slibdir = /lib/tls/i686/nosegneg
xen_extra_config_options = $(extra_config_options) --disable-profile --enable-multi-arch

define libc6-xen_extra_pkg_install
mkdir -p debian/libc6-xen/etc/ld.so.conf.d
echo '# This directive teaches ldconfig to search in nosegneg subdirectories' >  debian/libc6-xen/etc/ld.so.conf.d/xen.conf
echo '# and cache the DSOs there with extra bit 1 set in their hwcap match'   >> debian/libc6-xen/etc/ld.so.conf.d/xen.conf
echo '# fields. In Xen guest kernels, the vDSO tells the dynamic linker to'   >> debian/libc6-xen/etc/ld.so.conf.d/xen.conf
echo '# search in nosegneg subdirectories and to match this extra hwcap bit'  >> debian/libc6-xen/etc/ld.so.conf.d/xen.conf
echo '# in the ld.so.cache file.'                                             >> debian/libc6-xen/etc/ld.so.conf.d/xen.conf
echo 'hwcap 1 nosegneg'                                                       >> debian/libc6-xen/etc/ld.so.conf.d/xen.conf
endef

# build 64-bit (amd64) alternative library
EGLIBC_PASSES += amd64
DEB_ARCH_REGULAR_PACKAGES += libc6-amd64 libc6-dev-amd64
libc6-amd64_shlib_dep = libc6-amd64 (>= $(shlib_dep_ver))
amd64_add-ons = nptl $(add-ons)
amd64_configure_target = x86_64-linux-gnu
# __x86_64__ is defined here because Makeconfig uses -undef and the
# /usr/include/asm wrappers need that symbol.
amd64_CC = $(CC) -m64 -D__x86_64__
amd64_CXX = $(CXX) -m64 -D__x86_64__
amd64_extra_config_options = $(extra_config_options) --disable-profile --enable-multi-arch
amd64_slibdir = /lib64
amd64_libdir = /usr/lib64

define amd64_extra_install
cp debian/tmp-amd64/usr/bin/ldd \
	debian/tmp-libc/usr/bin
cp -af debian/tmp-amd64/usr/include/* \
	debian/tmp-libc/usr/include
rm -f debian/tmp-libc/usr/include/gnu/stubs-64.h
endef

define libc6-dev_extra_pkg_install
mkdir -p debian/libc6-dev/usr/lib/xen
cp -af debian/tmp-xen/usr/lib/*.a \
	debian/libc6-dev/usr/lib/xen
endef

define libc6-dev-amd64_extra_pkg_install
mkdir -p debian/libc6-dev-amd64/usr/include/gnu
cp -af debian/tmp-amd64/usr/include/gnu/stubs-64.h \
	debian/libc6-dev-amd64/usr/include/gnu
mkdir -p debian/libc6-dev-amd64/usr/include/x86_64-linux-gnu
endef

