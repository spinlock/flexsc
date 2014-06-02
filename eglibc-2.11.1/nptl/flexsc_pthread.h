#ifndef __NPTL_FLEXSC_PTHREAD_H__
#define __NPTL_FLEXSC_PTHREAD_H__

#include "flexsc.h"
#include <flexsc/assert.h>
#include <pthread.h>

int flexsc_pthread_create(pthread_t *newthread,
                          const pthread_attr_t *attr,
                          void *(*start_routine)(void *),
                          void *arg);

int flexsc_pthread_join(pthread_t threadid,
                        void **thread_return);

int flexsc_pthread_yield(void);

pthread_t flexsc_pthread_self(void);

int flexsc_pthread_detach(pthread_t th);

int flexsc_pthread_kill(pthread_t threadid, int signo);

void flexsc_pthread_exit(void *value);

struct pthread_key_data *flexsc_specific_1stblock(unsigned int idx, int **specified_usedpp);
struct pthread_key_data *flexsc_specific_mtxblock(unsigned int idx1st, unsigned int idx2nd, int **specified_usedpp);

#endif /* !__NPTL_FLEXSC_PTHREAD_H__ */
