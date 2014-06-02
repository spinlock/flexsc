#ifndef __FLEXSC_FLEXSC_LIST_H__
#define __FLEXSC_FLEXSC_LIST_H__

#include "define.h"

struct list_entry {
    struct list_entry *prev, *next;
};

typedef struct list_entry list_entry_t;

static __always_inline void
__list_init(list_entry_t *elm) {
    elm->prev = elm->next = elm;
}

static __always_inline list_entry_t *
list_init(list_entry_t *elm) {
    __list_init(elm);
    return elm;
}

static __always_inline void
__list_add(list_entry_t *elm, list_entry_t *prev, list_entry_t *next) {
    prev->next = next->prev = elm;
    elm->next = next;
    elm->prev = prev;
}

static __always_inline void
list_add_before(list_entry_t *listelm, list_entry_t *elm) {
    __list_add(elm, listelm->prev, listelm);
}

static __always_inline void
list_add_after(list_entry_t *listelm, list_entry_t *elm) {
    __list_add(elm, listelm, listelm->next);
}

static __always_inline void
__list_del(list_entry_t *prev, list_entry_t *next) {
    prev->next = next;
    next->prev = prev;
}

static __always_inline void
list_del_init(list_entry_t *listelm) {
    __list_del(listelm->prev, listelm->next);
    __list_init(listelm);
}

static __always_inline int
list_empty(list_entry_t *list) {
    return list->next == list;
}

static __always_inline list_entry_t *
list_next(list_entry_t *listelm) {
    return listelm->next;
}

static __always_inline list_entry_t *
list_prev(list_entry_t *listelm) {
    return listelm->prev;
}

#endif /* !__FLEXSC_FLEXSC_LIST_H__ */
