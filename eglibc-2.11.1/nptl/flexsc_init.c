#include "flexsc.h"

extern void flexsc_init_callback(flexsc_syscall_t do_syscall);

static int try_set_init(void) {
    static volatile int init = 0;
    int old;
    __asm__ __volatile__ ("leaq %1, %%rdi;"
                          "xchgl %%eax, (%%rdi);"
                          : "=a" (old), "=m" (init)
                          : "0" (1), "m" (init)
                          : "memory", "rdi");
    return old;
}

static struct cpuinfo cpuinfo_list[MAX_NCPUS];

static int cpuinfo_ngroups = 0;

static int
ncpus_detect(void) {
    int max_ncpus = sysconf(_SC_NPROCESSORS_CONF);
    if (!(max_ncpus > 0 && max_ncpus <= MAX_NCPUS)) {
        flexsc_panic("invalid max_ncpus %d\n", max_ncpus);
    }
    return max_ncpus;
}

void
open_cpuinfo(int list[], int size) {
    if (!(size > 0 && size <= MAX_CPUINFO_SIZE)) {
        flexsc_panic("invalid cpuinfo !!");
    }
    if (cpuinfo_ngroups >= MAX_NCPUS) {
        flexsc_panic("too many cpuinfo !!");
    }
    struct cpuinfo *cpuinfo = cpuinfo_list + cpuinfo_ngroups ++;
    memcpy(cpuinfo->list, list, sizeof(cpuinfo->list));
    cpuinfo->size = size;
}

static void
__flexsc_nptl_init(void) {
    if (flexsc_enabled()) {
        flexsc_panic("flexsc already enabled!!.\n");
    }

    if (try_set_init()) {
        flexsc_panic("call flexsc init too many times!!.\n");
    }

    flexsc_fiber_init();

    flexsc_thread_init(cpuinfo_ngroups, cpuinfo_list);
    
    flexsc_init_callback(flexsc_nptl_syscall);
    
    flexsc_assert(flexsc_enabled());
}

extern void flexsc_init_parse(const char *filename);

void
flexsc_nptl_init(const char *filename) {
    if (filename != NULL) {
        flexsc_init_parse(filename);
    }
    else {
        int cpu_mask[] = {1};
    
        int n = sizeof(cpu_mask) / sizeof(cpu_mask[0]);
        flexsc_assert(n > 0 && n <= MAX_NCPUS);
    
        int i;
        for (i = 0; i < n; i ++) {
            if (cpu_mask[i]) {
                struct cpuinfo *cpuinfo = cpuinfo_list + cpuinfo_ngroups ++;
                cpuinfo->list[0] = i, cpuinfo->size = 1;
            }
        }
    }

    flexsc_assert(cpuinfo_ngroups > 0 && cpuinfo_ngroups <= MAX_NCPUS);

    struct cpuinfo *cpuinfo;
    int i, j, cpu, max_ncpus = ncpus_detect();
    for (i = 0, cpuinfo = cpuinfo_list; i < cpuinfo_ngroups; i ++, cpuinfo ++) {
        if (!(cpuinfo->size > 0 && cpuinfo->size <= MAX_CPUINFO_SIZE)) {
            flexsc_panic("invalid cpuinfo size %d", cpuinfo->size);
        }
        for (j = 0; j < cpuinfo->size; j ++) {
            cpu = cpuinfo->list[j];
            if (!(cpu >= 0 && cpu < max_ncpus)) {
                flexsc_panic("invalid cpu id %d\n", cpu);
            }
        }
    }

    for (i = 0, cpuinfo = cpuinfo_list; i < cpuinfo_ngroups; i ++, cpuinfo ++) {
        flexsc_debug_nolock("cpu list: %2d [", i);
        for (j = 0; j < cpuinfo->size; j ++) {
            flexsc_debug_nolock(" %2d", cpuinfo->list[j]);
        }
        flexsc_debug_nolock(" ]\n");
    }

    __flexsc_nptl_init();
}
