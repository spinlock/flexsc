#ifndef __STDLIB_FLEXSC_H__
#define __STDLIB_FLEXSC_H__

#include <sys/types.h>

void flexsc_init(const char *filename);

int flexsc_fiber(void *(*__fn)(void *arg), void *arg, int detached, unsigned int *fidp);
int flexsc_wait(unsigned int fid, void **retp);
void flexsc_exit(void *ret);
void flexsc_schedule(void);

#endif /* !__STDLIB_FLEXSC_H__ */
