EGLIBC_OVERLAYS ?= $(shell ls glibc-linuxthreads* glibc-ports* glibc-libidn*)
MIN_KERNEL_SUPPORTED := 6.0.0
libc = libc0.1

# Linuxthreads Config
threads = yes
libc_add-ons = ports linuxthreads $(add-ons)
libc_extra_config_options = $(extra_config_options) --disable-compatible-utmp

ifndef KFREEBSD_SOURCE
  ifeq ($(DEB_HOST_GNU_TYPE),$(DEB_BUILD_GNU_TYPE))
    KFREEBSD_HEADERS := /usr/include
  else
    KFREEBSD_HEADERS := /usr/$(DEB_HOST_GNU_TYPE)/include
  endif
else
  KFREEBSD_HEADERS := $(KFREEBSD_SOURCE)/sys
endif

# Minimum Kernel supported
with_headers = --with-headers=$(shell pwd)/debian/include --enable-kernel=$(call xx,MIN_KERNEL_SUPPORTED)

KERNEL_HEADER_DIR = $(stamp)mkincludedir
$(stamp)mkincludedir:
	rm -rf debian/include
	mkdir debian/include
	ln -s $(KFREEBSD_HEADERS)/bsm debian/include
	ln -s $(KFREEBSD_HEADERS)/net debian/include
	ln -s $(KFREEBSD_HEADERS)/netatalk debian/include
	ln -s $(KFREEBSD_HEADERS)/netipx debian/include
	ln -s $(KFREEBSD_HEADERS)/nfs debian/include
	ln -s $(KFREEBSD_HEADERS)/osreldate.h debian/include
	ln -s $(KFREEBSD_HEADERS)/sys debian/include
	ln -s $(KFREEBSD_HEADERS)/vm debian/include

        # Link all machine directories.  We can't just link machine
        # because of explicit references to <machine-amd64/*> and
	# <machine-i386/*>.
	find $(KFREEBSD_HEADERS) -maxdepth 1 -xtype d -name machine\* \
		-exec ln -s '{}' debian/include ';'

	# To make configure happy if libc0.1-dev is not installed.
	touch debian/include/assert.h

	touch $@

# Also to make configure happy.
export CPPFLAGS = -isystem $(shell pwd)/debian/include

# This round of ugliness decomposes the FreeBSD kernel version number
# into an integer so it can be easily compared and then does so.
CURRENT_KERNEL_VERSION=$(shell uname -r)
define kernel_check
(minimum=$$((`echo $(1) | sed 's/\([0-9]*\)\.\([0-9]*\)\.\([0-9]*\)/\1 \* 10000 + \2 \* 100 + \3/'`)); \
current=$$((`echo $(CURRENT_KERNEL_VERSION) | sed 's/\([0-9]*\)\.\([0-9]*\).*/\1 \* 10000 + \2 \* 100/'`)); \
if [ $$current -lt $$minimum ]; then \
  false; \
fi)
endef

