/* SPDX-License-Identifier: LGPL-2.1+ */
#pragma once

#include <limits.h>
#include <stdbool.h>

#include "hash-funcs.h"
#include "util.h"

/*
 * A hash table implementation. As a minor optimization a NULL hashmap object
 * will be treated as empty hashmap for all read operations. That way it is not
 * necessary to instantiate an object for each Hashmap use.
 *
 * If ENABLE_DEBUG_HASHMAP is defined (by configuring with --enable-debug=hashmap),
 * the implementation will:
 * - store extra data for debugging and statistics (see tools/gdb-sd_dump_hashmaps.py)
 * - perform extra checks for invalid use of iterators
 */

#define HASH_KEY_SIZE 16

/* The base type for all hashmap and set types. Many functions in the
 * implementation take (HashmapBase*) parameters and are run-time polymorphic,
 * though the API is not meant to be polymorphic (do not call functions
 * internal_*() directly). */
typedef struct HashmapBase HashmapBase;

/* Specific hashmap/set types */
typedef struct Hashmap Hashmap;               /* Maps keys to values */
typedef struct OrderedHashmap OrderedHashmap; /* Like Hashmap, but also remembers entry insertion order */
typedef struct Set Set;                       /* Stores just keys */

typedef struct IteratedCache IteratedCache;   /* Caches the iterated order of one of the above */

/* Ideally the Iterator would be an opaque struct, but it is instantiated
 * by hashmap users, so the definition has to be here. Do not use its fields
 * directly. */
typedef struct {
        unsigned idx;         /* index of an entry to be iterated next */
        const void *next_key; /* expected value of that entry's key pointer */
#if ENABLE_DEBUG_HASHMAP
        unsigned put_count;   /* hashmap's put_count recorded at start of iteration */
        unsigned rem_count;   /* hashmap's rem_count in previous iteration */
        unsigned prev_idx;    /* idx in previous iteration */
#endif
} Iterator;

#define _IDX_ITERATOR_FIRST (UINT_MAX - 1)
#define ITERATOR_FIRST ((Iterator) { .idx = _IDX_ITERATOR_FIRST, .next_key = NULL })

/* Macros for type checking */
#define PTR_COMPATIBLE_WITH_HASHMAP_BASE(h) \
        (__builtin_types_compatible_p(typeof(h), HashmapBase*) || \
         __builtin_types_compatible_p(typeof(h), Hashmap*) || \
         __builtin_types_compatible_p(typeof(h), OrderedHashmap*) || \
         __builtin_types_compatible_p(typeof(h), Set*))

#define PTR_COMPATIBLE_WITH_PLAIN_HASHMAP(h) \
        (__builtin_types_compatible_p(typeof(h), Hashmap*) || \
         __builtin_types_compatible_p(typeof(h), OrderedHashmap*)) \

#define HASHMAP_BASE(h) \
        __builtin_choose_expr(PTR_COMPATIBLE_WITH_HASHMAP_BASE(h), \
                (HashmapBase*)(h), \
                (void)0)

#define PLAIN_HASHMAP(h) \
        __builtin_choose_expr(PTR_COMPATIBLE_WITH_PLAIN_HASHMAP(h), \
                (Hashmap*)(h), \
                (void)0)

#if ENABLE_DEBUG_HASHMAP
# define HASHMAP_DEBUG_PARAMS , const char *func, const char *file, int line
# define HASHMAP_DEBUG_SRC_ARGS   , __func__, __FILE__, __LINE__
# define HASHMAP_DEBUG_PASS_ARGS   , func, file, line
#else
# define HASHMAP_DEBUG_PARAMS
# define HASHMAP_DEBUG_SRC_ARGS
# define HASHMAP_DEBUG_PASS_ARGS
#endif

Hashmap *internal_hashmap_new(const struct hash_ops *hash_ops  HASHMAP_DEBUG_PARAMS);
#define hashmap_new(ops) internal_hashmap_new(ops  HASHMAP_DEBUG_SRC_ARGS)

HashmapBase *internal_hashmap_free(HashmapBase *h);
static inline Hashmap *hashmap_free(Hashmap *h) {
        return (void*)internal_hashmap_free(HASHMAP_BASE(h));
}
static inline OrderedHashmap *ordered_hashmap_free(OrderedHashmap *h) {
        return (void*)internal_hashmap_free(HASHMAP_BASE(h));
}

HashmapBase *internal_hashmap_free_free(HashmapBase *h);
static inline Hashmap *hashmap_free_free(Hashmap *h) {
        return (void*)internal_hashmap_free_free(HASHMAP_BASE(h));
}
static inline OrderedHashmap *ordered_hashmap_free_free(OrderedHashmap *h) {
        return (void*)internal_hashmap_free_free(HASHMAP_BASE(h));
}

Hashmap *hashmap_free_free_free(Hashmap *h);
static inline OrderedHashmap *ordered_hashmap_free_free_free(OrderedHashmap *h) {
        return (void*)hashmap_free_free_free(PLAIN_HASHMAP(h));
}

IteratedCache *iterated_cache_free(IteratedCache *cache);

int internal_hashmap_ensure_allocated(Hashmap **h, const struct hash_ops *hash_ops  HASHMAP_DEBUG_PARAMS);
int internal_ordered_hashmap_ensure_allocated(OrderedHashmap **h, const struct hash_ops *hash_ops  HASHMAP_DEBUG_PARAMS);
#define hashmap_ensure_allocated(h, ops) internal_hashmap_ensure_allocated(h, ops  HASHMAP_DEBUG_SRC_ARGS)
#define ordered_hashmap_ensure_allocated(h, ops) internal_ordered_hashmap_ensure_allocated(h, ops  HASHMAP_DEBUG_SRC_ARGS)

int hashmap_put(Hashmap *h, const void *key, void *value);
static inline int ordered_hashmap_put(OrderedHashmap *h, const void *key, void *value) {
        return hashmap_put(PLAIN_HASHMAP(h), key, value);
}

void *internal_hashmap_get(HashmapBase *h, const void *key);
static inline void *hashmap_get(Hashmap *h, const void *key) {
        return internal_hashmap_get(HASHMAP_BASE(h), key);
}

bool internal_hashmap_contains(HashmapBase *h, const void *key);

void *internal_hashmap_remove(HashmapBase *h, const void *key);
static inline void *hashmap_remove(Hashmap *h, const void *key) {
        return internal_hashmap_remove(HASHMAP_BASE(h), key);
}
static inline void *ordered_hashmap_remove(OrderedHashmap *h, const void *key) {
        return internal_hashmap_remove(HASHMAP_BASE(h), key);
}

unsigned internal_hashmap_size(HashmapBase *h) _pure_;
static inline unsigned hashmap_size(Hashmap *h) {
        return internal_hashmap_size(HASHMAP_BASE(h));
}
static inline bool hashmap_isempty(Hashmap *h) {
        return hashmap_size(h) == 0;
}

bool internal_hashmap_iterate(HashmapBase *h, Iterator *i, void **value, const void **key);
static inline bool hashmap_iterate(Hashmap *h, Iterator *i, void **value, const void **key) {
        return internal_hashmap_iterate(HASHMAP_BASE(h), i, value, key);
}
static inline bool ordered_hashmap_iterate(OrderedHashmap *h, Iterator *i, void **value, const void **key) {
        return internal_hashmap_iterate(HASHMAP_BASE(h), i, value, key);
}

void internal_hashmap_clear(HashmapBase *h);

void internal_hashmap_clear_free(HashmapBase *h);

void hashmap_clear_free_free(Hashmap *h);

/*
 * Note about all *_first*() functions
 *
 * For plain Hashmaps and Sets the order of entries is undefined.
 * The functions find whatever entry is first in the implementation
 * internal order.
 *
 * Only for OrderedHashmaps the order is well defined and finding
 * the first entry is O(1).
 */

void *internal_hashmap_first_key_and_value(HashmapBase *h, bool remove, void **ret_key);


static inline void *hashmap_steal_first(Hashmap *h) {
        return internal_hashmap_first_key_and_value(HASHMAP_BASE(h), true, NULL);
}
static inline void *ordered_hashmap_first(OrderedHashmap *h) {
        return internal_hashmap_first_key_and_value(HASHMAP_BASE(h), false, NULL);
}

#define hashmap_clear_with_destructor(_s, _f)                   \
        ({                                                      \
                void *_item;                                    \
                while ((_item = hashmap_steal_first(_s)))       \
                        _f(_item);                              \
        })
#define hashmap_free_with_destructor(_s, _f)                    \
        ({                                                      \
                hashmap_clear_with_destructor(_s, _f);          \
                hashmap_free(_s);                               \
        })

char **internal_hashmap_get_strv(HashmapBase *h);

/*
 * Hashmaps are iterated in unpredictable order.
 * OrderedHashmaps are an exception to this. They are iterated in the order
 * the entries were inserted.
 * It is safe to remove the current entry.
 */
#define HASHMAP_FOREACH(e, h, i) \
        for ((i) = ITERATOR_FIRST; hashmap_iterate((h), &(i), (void**)&(e), NULL); )

#define ORDERED_HASHMAP_FOREACH(e, h, i) \
        for ((i) = ITERATOR_FIRST; ordered_hashmap_iterate((h), &(i), (void**)&(e), NULL); )

#define HASHMAP_FOREACH_KEY(e, k, h, i) \
        for ((i) = ITERATOR_FIRST; hashmap_iterate((h), &(i), (void**)&(e), (const void**) &(k)); )

DEFINE_TRIVIAL_CLEANUP_FUNC(Hashmap*, hashmap_free);
DEFINE_TRIVIAL_CLEANUP_FUNC(Hashmap*, hashmap_free_free);
DEFINE_TRIVIAL_CLEANUP_FUNC(Hashmap*, hashmap_free_free_free);
DEFINE_TRIVIAL_CLEANUP_FUNC(OrderedHashmap*, ordered_hashmap_free);
DEFINE_TRIVIAL_CLEANUP_FUNC(OrderedHashmap*, ordered_hashmap_free_free);
DEFINE_TRIVIAL_CLEANUP_FUNC(OrderedHashmap*, ordered_hashmap_free_free_free);

#define _cleanup_hashmap_free_ _cleanup_(hashmap_freep)

DEFINE_TRIVIAL_CLEANUP_FUNC(IteratedCache*, iterated_cache_free);
