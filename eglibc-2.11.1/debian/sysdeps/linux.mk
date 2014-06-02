EGLIBC_OVERLAYS ?= $(shell ls glibc-linuxthreads* glibc-ports* glibc-libidn*)
MIN_KERNEL_SUPPORTED := 2.6.15
libc = libc6

# NPTL Config
threads = yes
libc_add-ons = nptl $(add-ons)
libc_extra_config_options = $(extra_config_options)

ifndef LINUX_SOURCE
  ifeq ($(DEB_HOST_GNU_TYPE),$(DEB_BUILD_GNU_TYPE))
    LINUX_HEADERS := /usr/include
  else
    LINUX_HEADERS := /usr/$(DEB_HOST_GNU_TYPE)/include
  endif
else
  LINUX_HEADERS := $(LINUX_SOURCE)/include
endif

# Minimum Kernel supported
with_headers = --with-headers=$(shell pwd)/debian/include --enable-kernel=$(call xx,MIN_KERNEL_SUPPORTED)

KERNEL_HEADER_DIR = $(stamp)mkincludedir
$(stamp)mkincludedir:
	rm -rf debian/include
	mkdir debian/include
	ln -s $(LINUX_HEADERS)/linux debian/include
	# Link all asm directories.  We can't just link asm and asm-generic
	# because of explicit references to <asm-sparc/*> and
	# <asm-sparc64/*>.
	find $(LINUX_HEADERS) -maxdepth 1 -xtype d -name asm\* \
	  -exec ln -s '{}' debian/include ';'

	# To make configure happy if libc6-dev is not installed.
	touch debian/include/assert.h

	touch $@

# Also to make configure happy.
export CPPFLAGS = -isystem $(shell pwd)/debian/include

# This round of ugliness decomposes the Linux kernel version number
# into an integer so it can be easily compared and then does so.
CURRENT_KERNEL_VERSION=$(shell uname -r)
define kernel_check
(minimum=$$((`echo $(1) | sed 's/\([0-9]*\)\.\([0-9]*\)\.\([0-9]*\).*/\1 \* 65536 + \2 \* 256 + \3/'`)); \
current=$$((`echo $(CURRENT_KERNEL_VERSION) | sed 's/\([0-9]*\)\.\([0-9]*\)\.\([0-9]*\).*/\1 \* 65536 + \2 \* 256 + \3/'`)); \
if [ $$current -lt $$minimum ]; then \
  false; \
fi)
endef
