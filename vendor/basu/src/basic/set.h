/* SPDX-License-Identifier: LGPL-2.1+ */
#pragma once

Set *internal_set_new(const struct hash_ops *hash_ops HASHMAP_DEBUG_PARAMS);
#define set_new(ops) internal_set_new(ops HASHMAP_DEBUG_SRC_ARGS)

static inline Set *set_free(Set *s) {
        internal_hashmap_free(HASHMAP_BASE(s));
        return NULL;
}

static inline Set *set_free_free(Set *s) {
        internal_hashmap_free_free(HASHMAP_BASE(s));
        return NULL;
}

/* no set_free_free_free */

int set_put(Set *s, const void *key);
/* no set_update */
/* no set_replace */
static inline void *set_get(Set *s, void *key) {
        return internal_hashmap_get(HASHMAP_BASE(s), key);
}
/* no set_get2 */

static inline bool set_contains(Set *s, const void *key) {
        return internal_hashmap_contains(HASHMAP_BASE(s), key);
}

/* no set_remove2 */
/* no set_remove_value */
/* no set_remove_and_replace */

static inline unsigned set_size(Set *s) {
        return internal_hashmap_size(HASHMAP_BASE(s));
}

static inline bool set_isempty(Set *s) {
        return set_size(s) == 0;
}

bool set_iterate(Set *s, Iterator *i, void **value);

static inline void *set_steal_first(Set *s) {
        return internal_hashmap_first_key_and_value(HASHMAP_BASE(s), true, NULL);
}

#define set_clear_with_destructor(_s, _f)               \
        ({                                              \
                void *_item;                            \
                while ((_item = set_steal_first(_s)))   \
                        _f(_item);                      \
        })
#define set_free_with_destructor(_s, _f)                \
        ({                                              \
                set_clear_with_destructor(_s, _f);      \
                set_free(_s);                           \
        })

/* no set_steal_first_key */
/* no set_first_key */

/* no set_next */

static inline char **set_get_strv(Set *s) {
        return internal_hashmap_get_strv(HASHMAP_BASE(s));
}

int set_consume(Set *s, void *value);
int set_put_strdup(Set *s, const char *p);

#define SET_FOREACH(e, s, i) \
        for ((i) = ITERATOR_FIRST; set_iterate((s), &(i), (void**)&(e)); )

DEFINE_TRIVIAL_CLEANUP_FUNC(Set*, set_free);
DEFINE_TRIVIAL_CLEANUP_FUNC(Set*, set_free_free);

#define _cleanup_set_free_ _cleanup_(set_freep)
#define _cleanup_set_free_free_ _cleanup_(set_free_freep)
