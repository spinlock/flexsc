#include <unistd.h>
#include <flexsc.h>
#include "flexsc.h"

extern void __attribute__((__weak__)) flexsc_nptl_init(const char *filename);

void
flexsc_init(const char *filename) {
    static_assert(sizeof(struct flexsc_sysentry) == CACHE_SIZE);
    if (unlikely(!flexsc_nptl_init)) {
        flexsc_panic("cannot find flexsc library.\n");
    }

    flexsc_nptl_init(filename);
}
