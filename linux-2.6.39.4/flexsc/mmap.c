#include "flexsc.h"

extern asmlinkage long sys_mmap_pgoff(unsigned long addr, unsigned long len,
                                      unsigned long prot, unsigned long flags,
                                      unsigned long fd, unsigned long pgoff);

void *
flexsc_mmap(size_t size, int locked, int *errp) {
    unsigned long prots = PROT_READ | PROT_WRITE;
    unsigned long flags = MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE;
    long addr;
    if (locked) {
        flags |= MAP_LOCKED;
    }
    if ((addr = sys_mmap_pgoff(0, roundup(size, PAGE_SIZE), prots, flags, -1, 0)) <= 0) {
        if (-addr <= 1024) {
            if (addr == 0) {
                addr = -1;
            }
            if (errp != NULL) {
                *errp = (int)addr;
            }
            return NULL;
        }
    }
    return (void *)addr;
}
