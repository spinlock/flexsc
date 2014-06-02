#ifndef __NPTL_FLEXSC_LIST_H__
#define __NPTL_FLEXSC_LIST_H__

#include "flexsc.h"

typedef struct {
    list_entry_t __list;
    volatile unsigned int __slots;
} flist_t;

static __always_inline flist_t *
flist_init(flist_t *flist) {
    list_init(&(flist->__list));
    flist->__slots = 0;
    return flist;
}

static __always_inline unsigned int
flist_slots(flist_t *flist) {
    return flist->__slots;
}

static __always_inline void
flist_enqueue_slot(flist_t *flist, list_entry_t *le) {
    list_add_before(&(flist->__list), le);
    flist->__slots ++;
}

static __always_inline void
flist_dequeue_slot(flist_t *flist, list_entry_t *le) {
    list_del_init(le);
    flist->__slots --;
}

#endif /* !__NPTL_FLEXSC_LIST_H__ */

