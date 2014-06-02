#include <execinfo.h>
#include <stdlib.h>
#include <unistd.h>
#include <flexsc.h>
#include "flexsc.h"

#define SIZE        128

void
flexsc_backtrace(void) {
    void *buffer[SIZE];
    char **strings;

    int size = backtrace(buffer, SIZE);
    if (!(size > 0 && size <= SIZE)) {
        flexsc_debug_nolock("backtrace() - %d\n", size);
    }
    if (size > 0) {
        if ((strings = backtrace_symbols(buffer, size)) == NULL) {
            flexsc_debug_nolock("backtrace() - %d, but returns NULL.\n", size);
        }
        else {
            flexsc_debug_nolock("backtrace() %d:\n", size);
            int i;
            for (i = 0; i < size; i ++) {
                flexsc_debug_nolock("[%3d] %s\n", i, strings[i]);
            }
            free(strings);
        }
    }
}
