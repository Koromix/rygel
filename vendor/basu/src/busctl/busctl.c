/* SPDX-License-Identifier: LGPL-2.1+ */

#include <getopt.h>

#include "sd-bus.h"

#include "alloc-util.h"
#include "bus-dump.h"
#include "bus-internal.h"
#include "bus-signature.h"
#include "bus-type.h"
#include "busctl-introspect.h"
#include "escape.h"
#include "fd-util.h"
#include "fileio.h"
#include "json.h"
#include "locale-util.h"
#include "parse-util.h"
#include "path-util.h"
#include "set.h"
#include "strv.h"
#include "terminal-util.h"
#include "user-util.h"
#include "verbs.h"

typedef enum BusTransport {
        BUS_TRANSPORT_LOCAL,
        _BUS_TRANSPORT_MAX,
        _BUS_TRANSPORT_INVALID = -1
} BusTransport;

static enum {
        JSON_OFF,
        JSON_SHORT,
        JSON_PRETTY,
} arg_json = JSON_OFF;
static bool arg_legend = true;
static const char *arg_address = NULL;
static bool arg_unique = false;
static bool arg_acquired = false;
static bool arg_activatable = false;
static bool arg_show_machine = false;
static char **arg_matches = NULL;
static BusTransport arg_transport = BUS_TRANSPORT_LOCAL;
static bool arg_user = false;
static bool arg_list = false;
static bool arg_quiet = false;
static bool arg_verbose = false;
static bool arg_expect_reply = true;
static bool arg_auto_start = true;
static bool arg_allow_interactive_authorization = true;
static bool arg_augment_creds = true;
static bool arg_watch_bind = false;
static usec_t arg_timeout = 0;

#define NAME_IS_ACQUIRED INT_TO_PTR(1)
#define NAME_IS_ACTIVATABLE INT_TO_PTR(2)

static int bus_log_parse_error(int r) {
        return log_error_errno(r, "Failed to parse bus message: %m");
}

static int bus_log_create_error(int r) {
        return log_error_errno(r, "Failed to create bus message: %m");
}

static int acquire_bus(bool set_monitor, sd_bus **ret) {
        _cleanup_(sd_bus_unrefp) sd_bus *bus = NULL;
        int r;

        r = sd_bus_new(&bus);
        if (r < 0)
                return log_error_errno(r, "Failed to allocate bus: %m");

        if (set_monitor) {
                r = sd_bus_set_monitor(bus, true);
                if (r < 0)
                        return log_error_errno(r, "Failed to set monitor mode: %m");

                r = sd_bus_negotiate_creds(bus, true, _SD_BUS_CREDS_ALL);
                if (r < 0)
                        return log_error_errno(r, "Failed to enable credentials: %m");

                r = sd_bus_negotiate_timestamp(bus, true);
                if (r < 0)
                        return log_error_errno(r, "Failed to enable timestamps: %m");

                r = sd_bus_negotiate_fds(bus, true);
                if (r < 0)
                        return log_error_errno(r, "Failed to enable fds: %m");
        }

        r = sd_bus_set_bus_client(bus, true);
        if (r < 0)
                return log_error_errno(r, "Failed to set bus client: %m");

        r = sd_bus_set_watch_bind(bus, arg_watch_bind);
        if (r < 0)
                return log_error_errno(r, "Failed to set watch-bind setting to '%s': %m", yes_no(arg_watch_bind));

        if (arg_address)
                r = sd_bus_set_address(bus, arg_address);
        else {
                switch (arg_transport) {

                case BUS_TRANSPORT_LOCAL:
                        if (arg_user) {
                                bus->is_user = true;
                                r = bus_set_address_user(bus);
                        } else {
                                bus->is_system = true;
                                r = bus_set_address_system(bus);
                        }
                        break;

                default:
                        assert_not_reached("Hmm, unknown transport type.");
                }
        }
        if (r < 0)
                return log_error_errno(r, "Failed to set address: %m");

        r = sd_bus_start(bus);
        if (r < 0)
                return log_error_errno(r, "Failed to connect to bus: %m");

        *ret = TAKE_PTR(bus);

        return 0;
}

static int list_bus_names(int argc, char **argv, void *userdata) {
        _cleanup_(sd_bus_flush_close_unrefp) sd_bus *bus = NULL;
        _cleanup_strv_free_ char **acquired = NULL, **activatable = NULL;
        _cleanup_free_ char **merged = NULL;
        _cleanup_hashmap_free_ Hashmap *names = NULL;
        char **i;
        int r;
        size_t max_i = 0;
        unsigned n = 0;
        void *v;
        char *k;
        Iterator iterator;

        if (!arg_unique && !arg_acquired && !arg_activatable)
                arg_unique = arg_acquired = arg_activatable = true;

        r = acquire_bus(false, &bus);
        if (r < 0)
                return r;

        r = sd_bus_list_names(bus, (arg_acquired || arg_unique) ? &acquired : NULL, arg_activatable ? &activatable : NULL);
        if (r < 0)
                return log_error_errno(r, "Failed to list names: %m");

        names = hashmap_new(&string_hash_ops);
        if (!names)
                return log_oom();

        STRV_FOREACH(i, acquired) {
                max_i = MAX(max_i, strlen(*i));

                r = hashmap_put(names, *i, NAME_IS_ACQUIRED);
                if (r < 0)
                        return log_error_errno(r, "Failed to add to hashmap: %m");
        }

        STRV_FOREACH(i, activatable) {
                max_i = MAX(max_i, strlen(*i));

                r = hashmap_put(names, *i, NAME_IS_ACTIVATABLE);
                if (r < 0 && r != -EEXIST)
                        return log_error_errno(r, "Failed to add to hashmap: %m");
        }

        merged = new(char*, hashmap_size(names) + 1);
        if (!merged)
                return log_oom();

        HASHMAP_FOREACH_KEY(v, k, names, iterator)
                merged[n++] = k;

        merged[n] = NULL;
        strv_sort(merged);

        if (arg_legend) {
                printf("%-*s %*s %-*s %-*s %-*s %-*s %-*s %-*s",
                       (int) max_i, "NAME", 10, "PID", 15, "PROCESS", 16, "USER", 13, "CONNECTION", 25, "UNIT", 10, "SESSION", 19, "DESCRIPTION");

                if (arg_show_machine)
                        puts(" MACHINE");
                else
                        putchar('\n');
        }

        STRV_FOREACH(i, merged) {
                _cleanup_(sd_bus_creds_unrefp) sd_bus_creds *creds = NULL;
                sd_id128_t mid;

                if (hashmap_get(names, *i) == NAME_IS_ACTIVATABLE) {
                        /* Activatable */

                        printf("%-*s", (int) max_i, *i);
                        printf("          - -               -                (activatable) -                         -         ");
                        if (arg_show_machine)
                                puts(" -");
                        else
                                putchar('\n');
                        continue;

                }

                if (!arg_unique && (*i)[0] == ':')
                        continue;

                if (!arg_acquired && (*i)[0] != ':')
                        continue;

                printf("%-*s", (int) max_i, *i);

                r = sd_bus_get_name_creds(
                                bus, *i,
                                (arg_augment_creds ? SD_BUS_CREDS_AUGMENT : 0) |
                                SD_BUS_CREDS_EUID|SD_BUS_CREDS_PID|SD_BUS_CREDS_COMM|
                                SD_BUS_CREDS_UNIQUE_NAME|SD_BUS_CREDS_UNIT|SD_BUS_CREDS_SESSION|
                                SD_BUS_CREDS_DESCRIPTION, &creds);
                if (r >= 0) {
                        const char *unique, *session, *unit, *cn;
                        pid_t pid;
                        uid_t uid;

                        r = sd_bus_creds_get_pid(creds, &pid);
                        if (r >= 0) {
                                const char *comm = NULL;

                                sd_bus_creds_get_comm(creds, &comm);

                                printf(" %10lu %-15s", (unsigned long) pid, strna(comm));
                        } else
                                fputs("          - -              ", stdout);

                        r = sd_bus_creds_get_euid(creds, &uid);
                        if (r >= 0) {
                                _cleanup_free_ char *u = NULL;

                                u = uid_to_name(uid);
                                if (!u)
                                        return log_oom();

                                if (strlen(u) > 16)
                                        u[16] = 0;

                                printf(" %-16s", u);
                        } else
                                fputs(" -               ", stdout);

                        r = sd_bus_creds_get_unique_name(creds, &unique);
                        if (r >= 0)
                                printf(" %-13s", unique);
                        else
                                fputs(" -            ", stdout);

                        r = sd_bus_creds_get_unit(creds, &unit);
                        if (r >= 0) {
                                _cleanup_free_ char *e;

                                e = ellipsize(unit, 25, 100);
                                if (!e)
                                        return log_oom();

                                printf(" %-25s", e);
                        } else
                                fputs(" -                        ", stdout);

                        r = sd_bus_creds_get_session(creds, &session);
                        if (r >= 0)
                                printf(" %-10s", session);
                        else
                                fputs(" -         ", stdout);

                        r = sd_bus_creds_get_description(creds, &cn);
                        if (r >= 0)
                                printf(" %-19s", cn);
                        else
                                fputs(" -                  ", stdout);

                } else
                        printf("          - -               -                -             -                         -          -                  ");

                if (arg_show_machine) {
                        r = sd_bus_get_name_machine_id(bus, *i, &mid);
                        if (r >= 0) {
                                char m[SD_ID128_STRING_MAX];
                                printf(" %s\n", sd_id128_to_string(mid, m));
                        } else
                                puts(" -");
                } else
                        putchar('\n');
        }

        return 0;
}

static void print_subtree(const char *prefix, const char *path, char **l) {
        const char *vertical, *space;
        char **n;

        /* We assume the list is sorted. Let's first skip over the
         * entry we are looking at. */
        for (;;) {
                if (!*l)
                        return;

                if (!streq(*l, path))
                        break;

                l++;
        }

        vertical = strjoina(prefix, special_glyph(TREE_VERTICAL));
        space = strjoina(prefix, special_glyph(TREE_SPACE));

        for (;;) {
                bool has_more = false;

                if (!*l || !path_startswith(*l, path))
                        break;

                n = l + 1;
                for (;;) {
                        if (!*n || !path_startswith(*n, path))
                                break;

                        if (!path_startswith(*n, *l)) {
                                has_more = true;
                                break;
                        }

                        n++;
                }

                printf("%s%s%s\n", prefix, special_glyph(has_more ? TREE_BRANCH : TREE_RIGHT), *l);

                print_subtree(has_more ? vertical : space, *l, l);
                l = n;
        }
}

static void print_tree(const char *prefix, char **l) {

        prefix = strempty(prefix);

        if (arg_list) {
                char **i;

                STRV_FOREACH(i, l)
                        printf("%s%s\n", prefix, *i);
                return;
        }

        if (strv_isempty(l)) {
                printf("No objects discovered.\n");
                return;
        }

        if (streq(l[0], "/") && !l[1]) {
                printf("Only root object discovered.\n");
                return;
        }

        print_subtree(prefix, "/", l);
}

static int on_path(const char *path, void *userdata) {
        Set *paths = userdata;
        int r;

        assert(paths);

        r = set_put_strdup(paths, path);
        if (r < 0)
                return log_oom();

        return 0;
}

static int find_nodes(sd_bus *bus, const char *service, const char *path, Set *paths, bool many) {
        static const XMLIntrospectOps ops = {
                .on_path = on_path,
        };

        _cleanup_(sd_bus_message_unrefp) sd_bus_message *reply = NULL;
        _cleanup_(sd_bus_error_free) sd_bus_error error = SD_BUS_ERROR_NULL;
        const char *xml;
        int r;

        r = sd_bus_call_method(bus, service, path, "org.freedesktop.DBus.Introspectable", "Introspect", &error, &reply, "");
        if (r < 0) {
                if (many)
                        printf("Failed to introspect object %s of service %s: %s\n", path, service, bus_error_message(&error, r));
                else
                        log_error_errno(r, "Failed to introspect object %s of service %s: %s", path, service, bus_error_message(&error, r));
                return r;
        }

        r = sd_bus_message_read(reply, "s", &xml);
        if (r < 0)
                return bus_log_parse_error(r);

        return parse_xml_introspect(path, xml, &ops, paths);
}

static int tree_one(sd_bus *bus, const char *service, const char *prefix, bool many) {
        _cleanup_set_free_free_ Set *paths = NULL, *done = NULL, *failed = NULL;
        _cleanup_free_ char **l = NULL;
        char *m;
        int r;

        paths = set_new(&string_hash_ops);
        if (!paths)
                return log_oom();

        done = set_new(&string_hash_ops);
        if (!done)
                return log_oom();

        failed = set_new(&string_hash_ops);
        if (!failed)
                return log_oom();

        m = strdup("/");
        if (!m)
                return log_oom();

        r = set_put(paths, m);
        if (r < 0) {
                free(m);
                return log_oom();
        }

        for (;;) {
                _cleanup_free_ char *p = NULL;
                int q;

                p = set_steal_first(paths);
                if (!p)
                        break;

                if (set_contains(done, p) ||
                    set_contains(failed, p))
                        continue;

                q = find_nodes(bus, service, p, paths, many);
                if (q < 0) {
                        if (r >= 0)
                                r = q;

                        q = set_put(failed, p);
                } else
                        q = set_put(done, p);

                if (q < 0)
                        return log_oom();

                assert(q != 0);
                p = NULL;
        }

        l = set_get_strv(done);
        if (!l)
                return log_oom();

        strv_sort(l);
        print_tree(prefix, l);

        fflush(stdout);

        return r;
}

static int tree(int argc, char **argv, void *userdata) {
        _cleanup_(sd_bus_flush_close_unrefp) sd_bus *bus = NULL;
        char **i;
        int r = 0;

        if (!arg_unique && !arg_acquired)
                arg_acquired = true;

        r = acquire_bus(false, &bus);
        if (r < 0)
                return r;

        if (argc <= 1) {
                _cleanup_strv_free_ char **names = NULL;
                bool not_first = false;

                r = sd_bus_list_names(bus, &names, NULL);
                if (r < 0)
                        return log_error_errno(r, "Failed to get name list: %m");

                STRV_FOREACH(i, names) {
                        int q;

                        if (!arg_unique && (*i)[0] == ':')
                                continue;

                        if (!arg_acquired && (*i)[0] == ':')
                                continue;

                        if (not_first)
                                printf("\n");

                        printf("Service %s%s%s:\n", ansi_highlight(), *i, ansi_normal());

                        q = tree_one(bus, *i, NULL, true);
                        if (q < 0 && r >= 0)
                                r = q;

                        not_first = true;
                }
        } else {
                STRV_FOREACH(i, argv+1) {
                        int q;

                        if (i > argv+1)
                                printf("\n");

                        if (argv[2]) {
                                printf("Service %s%s%s:\n", ansi_highlight(), *i, ansi_normal());
                        }

                        q = tree_one(bus, *i, NULL, !!argv[2]);
                        if (q < 0 && r >= 0)
                                r = q;
                }
        }

        return r;
}

static int format_cmdline(sd_bus_message *m, FILE *f, bool needs_space) {
        int r;

        for (;;) {
                const char *contents = NULL;
                char type;
                union {
                        uint8_t u8;
                        uint16_t u16;
                        int16_t s16;
                        uint32_t u32;
                        int32_t s32;
                        uint64_t u64;
                        int64_t s64;
                        double d64;
                        const char *string;
                        int i;
                } basic;

                r = sd_bus_message_peek_type(m, &type, &contents);
                if (r < 0)
                        return r;
                if (r == 0)
                        return needs_space;

                if (bus_type_is_container(type) > 0) {

                        r = sd_bus_message_enter_container(m, type, contents);
                        if (r < 0)
                                return r;

                        if (type == SD_BUS_TYPE_ARRAY) {
                                unsigned n = 0;

                                /* count array entries */
                                for (;;) {

                                        r = sd_bus_message_skip(m, contents);
                                        if (r < 0)
                                                return r;
                                        if (r == 0)
                                                break;

                                        n++;
                                }

                                r = sd_bus_message_rewind(m, false);
                                if (r < 0)
                                        return r;

                                if (needs_space)
                                        fputc(' ', f);

                                fprintf(f, "%u", n);
                                needs_space = true;

                        } else if (type == SD_BUS_TYPE_VARIANT) {

                                if (needs_space)
                                        fputc(' ', f);

                                fprintf(f, "%s", contents);
                                needs_space = true;
                        }

                        r = format_cmdline(m, f, needs_space);
                        if (r < 0)
                                return r;

                        needs_space = r > 0;

                        r = sd_bus_message_exit_container(m);
                        if (r < 0)
                                return r;

                        continue;
                }

                r = sd_bus_message_read_basic(m, type, &basic);
                if (r < 0)
                        return r;

                if (needs_space)
                        fputc(' ', f);

                switch (type) {
                case SD_BUS_TYPE_BYTE:
                        fprintf(f, "%u", basic.u8);
                        break;

                case SD_BUS_TYPE_BOOLEAN:
                        fputs(true_false(basic.i), f);
                        break;

                case SD_BUS_TYPE_INT16:
                        fprintf(f, "%i", basic.s16);
                        break;

                case SD_BUS_TYPE_UINT16:
                        fprintf(f, "%u", basic.u16);
                        break;

                case SD_BUS_TYPE_INT32:
                        fprintf(f, "%i", basic.s32);
                        break;

                case SD_BUS_TYPE_UINT32:
                        fprintf(f, "%u", basic.u32);
                        break;

                case SD_BUS_TYPE_INT64:
                        fprintf(f, "%" PRIi64, basic.s64);
                        break;

                case SD_BUS_TYPE_UINT64:
                        fprintf(f, "%" PRIu64, basic.u64);
                        break;

                case SD_BUS_TYPE_DOUBLE:
                        fprintf(f, "%g", basic.d64);
                        break;

                case SD_BUS_TYPE_STRING:
                case SD_BUS_TYPE_OBJECT_PATH:
                case SD_BUS_TYPE_SIGNATURE: {
                        _cleanup_free_ char *b = NULL;

                        b = cescape(basic.string);
                        if (!b)
                                return -ENOMEM;

                        fprintf(f, "\"%s\"", b);
                        break;
                }

                case SD_BUS_TYPE_UNIX_FD:
                        fprintf(f, "%i", basic.i);
                        break;

                default:
                        assert_not_reached("Unknown basic type.");
                }

                needs_space = true;
        }
}

typedef struct Member {
        const char *type;
        char *interface;
        char *name;
        char *signature;
        char *result;
        char *value;
        bool writable;
        uint64_t flags;
} Member;

static void member_hash_func(const void *p, struct siphash *state) {
        const Member *m = p;
        uint64_t arity = 1;

        assert(m);
        assert(m->type);

        string_hash_func(m->type, state);

        arity += !!m->name + !!m->interface;

        uint64_hash_func(&arity, state);

        if (m->name)
                string_hash_func(m->name, state);

        if (m->interface)
                string_hash_func(m->interface, state);
}

static int member_compare_func(const Member *x, const Member *y) {
        int d;

        assert(x);
        assert(y);
        assert(x->type);
        assert(y->type);

        d = strcmp_ptr(x->interface, y->interface);
        if (d != 0)
                return d;

        d = strcmp(x->type, y->type);
        if (d != 0)
                return d;

        return strcmp_ptr(x->name, y->name);
}

static int member_compare_funcp(Member * const *a, Member * const *b) {
        return member_compare_func(*a, *b);
}

static void member_free(Member *m) {
        if (!m)
                return;

        free(m->interface);
        free(m->name);
        free(m->signature);
        free(m->result);
        free(m->value);
        free(m);
}

DEFINE_TRIVIAL_CLEANUP_FUNC(Member*, member_free);

static void member_set_free(Set *s) {
        set_free_with_destructor(s, member_free);
}

DEFINE_TRIVIAL_CLEANUP_FUNC(Set*, member_set_free);

static int on_interface(const char *interface, uint64_t flags, void *userdata) {
        _cleanup_(member_freep) Member *m;
        Set *members = userdata;
        int r;

        assert(interface);
        assert(members);

        m = new0(Member, 1);
        if (!m)
                return log_oom();

        m->type = "interface";
        m->flags = flags;

        r = free_and_strdup(&m->interface, interface);
        if (r < 0)
                return log_oom();

        r = set_put(members, m);
        if (r <= 0) {
                log_error("Duplicate interface");
                return -EINVAL;
        }

        m = NULL;
        return 0;
}

static int on_method(const char *interface, const char *name, const char *signature, const char *result, uint64_t flags, void *userdata) {
        _cleanup_(member_freep) Member *m;
        Set *members = userdata;
        int r;

        assert(interface);
        assert(name);

        m = new0(Member, 1);
        if (!m)
                return log_oom();

        m->type = "method";
        m->flags = flags;

        r = free_and_strdup(&m->interface, interface);
        if (r < 0)
                return log_oom();

        r = free_and_strdup(&m->name, name);
        if (r < 0)
                return log_oom();

        r = free_and_strdup(&m->signature, signature);
        if (r < 0)
                return log_oom();

        r = free_and_strdup(&m->result, result);
        if (r < 0)
                return log_oom();

        r = set_put(members, m);
        if (r <= 0) {
                log_error("Duplicate method");
                return -EINVAL;
        }

        m = NULL;
        return 0;
}

static int on_signal(const char *interface, const char *name, const char *signature, uint64_t flags, void *userdata) {
        _cleanup_(member_freep) Member *m;
        Set *members = userdata;
        int r;

        assert(interface);
        assert(name);

        m = new0(Member, 1);
        if (!m)
                return log_oom();

        m->type = "signal";
        m->flags = flags;

        r = free_and_strdup(&m->interface, interface);
        if (r < 0)
                return log_oom();

        r = free_and_strdup(&m->name, name);
        if (r < 0)
                return log_oom();

        r = free_and_strdup(&m->signature, signature);
        if (r < 0)
                return log_oom();

        r = set_put(members, m);
        if (r <= 0) {
                log_error("Duplicate signal");
                return -EINVAL;
        }

        m = NULL;
        return 0;
}

static int on_property(const char *interface, const char *name, const char *signature, bool writable, uint64_t flags, void *userdata) {
        _cleanup_(member_freep) Member *m;
        Set *members = userdata;
        int r;

        assert(interface);
        assert(name);

        m = new0(Member, 1);
        if (!m)
                return log_oom();

        m->type = "property";
        m->flags = flags;
        m->writable = writable;

        r = free_and_strdup(&m->interface, interface);
        if (r < 0)
                return log_oom();

        r = free_and_strdup(&m->name, name);
        if (r < 0)
                return log_oom();

        r = free_and_strdup(&m->signature, signature);
        if (r < 0)
                return log_oom();

        r = set_put(members, m);
        if (r <= 0) {
                log_error("Duplicate property");
                return -EINVAL;
        }

        m = NULL;
        return 0;
}

static int introspect(int argc, char **argv, void *userdata) {
        static const struct hash_ops member_hash_ops = {
                .hash = member_hash_func,
                .compare = (__compar_fn_t) member_compare_func,
        };

        static const XMLIntrospectOps ops = {
                .on_interface = on_interface,
                .on_method = on_method,
                .on_signal = on_signal,
                .on_property = on_property,
        };

        _cleanup_(sd_bus_flush_close_unrefp) sd_bus *bus = NULL;
        _cleanup_(sd_bus_message_unrefp) sd_bus_message *reply_xml = NULL;
        _cleanup_(sd_bus_error_free) sd_bus_error error = SD_BUS_ERROR_NULL;
        _cleanup_(member_set_freep) Set *members = NULL;
        unsigned name_width, type_width, signature_width, result_width, j, k = 0;
        Member *m, **sorted = NULL;
        Iterator i;
        const char *xml;
        int r;

        r = acquire_bus(false, &bus);
        if (r < 0)
                return r;

        members = set_new(&member_hash_ops);
        if (!members)
                return log_oom();

        r = sd_bus_call_method(bus, argv[1], argv[2], "org.freedesktop.DBus.Introspectable", "Introspect", &error, &reply_xml, "");
        if (r < 0)
                return log_error_errno(r, "Failed to introspect object %s of service %s: %s", argv[2], argv[1], bus_error_message(&error, r));

        r = sd_bus_message_read(reply_xml, "s", &xml);
        if (r < 0)
                return bus_log_parse_error(r);

        /* First, get list of all properties */
        r = parse_xml_introspect(argv[2], xml, &ops, members);
        if (r < 0)
                return r;

        /* Second, find the current values for them */
        SET_FOREACH(m, members, i) {
                _cleanup_(sd_bus_message_unrefp) sd_bus_message *reply = NULL;

                if (!streq(m->type, "property"))
                        continue;

                if (m->value)
                        continue;

                if (argv[3] && !streq(argv[3], m->interface))
                        continue;

                r = sd_bus_call_method(bus, argv[1], argv[2], "org.freedesktop.DBus.Properties", "GetAll", &error, &reply, "s", m->interface);
                if (r < 0)
                        return log_error_errno(r, "%s", bus_error_message(&error, r));

                r = sd_bus_message_enter_container(reply, 'a', "{sv}");
                if (r < 0)
                        return bus_log_parse_error(r);

                for (;;) {
                        Member *z;
                        _cleanup_free_ char *buf = NULL;
                        _cleanup_fclose_ FILE *mf = NULL;
                        size_t sz = 0;
                        const char *name;

                        r = sd_bus_message_enter_container(reply, 'e', "sv");
                        if (r < 0)
                                return bus_log_parse_error(r);

                        if (r == 0)
                                break;

                        r = sd_bus_message_read(reply, "s", &name);
                        if (r < 0)
                                return bus_log_parse_error(r);

                        r = sd_bus_message_enter_container(reply, 'v', NULL);
                        if (r < 0)
                                return bus_log_parse_error(r);

                        mf = open_memstream(&buf, &sz);
                        if (!mf)
                                return log_oom();

                        r = format_cmdline(reply, mf, false);
                        if (r < 0)
                                return bus_log_parse_error(r);

                        mf = safe_fclose(mf);

                        z = set_get(members, &((Member) {
                                                .type = "property",
                                                .interface = m->interface,
                                                .name = (char*) name }));
                        if (z)
                                free_and_replace(z->value, buf);

                        r = sd_bus_message_exit_container(reply);
                        if (r < 0)
                                return bus_log_parse_error(r);

                        r = sd_bus_message_exit_container(reply);
                        if (r < 0)
                                return bus_log_parse_error(r);
                }

                r = sd_bus_message_exit_container(reply);
                if (r < 0)
                        return bus_log_parse_error(r);
        }

        name_width = STRLEN("NAME");
        type_width = STRLEN("TYPE");
        signature_width = STRLEN("SIGNATURE");
        result_width = STRLEN("RESULT/VALUE");

        sorted = newa(Member*, set_size(members));

        SET_FOREACH(m, members, i) {

                if (argv[3] && !streq(argv[3], m->interface))
                        continue;

                if (m->interface)
                        name_width = MAX(name_width, strlen(m->interface));
                if (m->name)
                        name_width = MAX(name_width, strlen(m->name) + 1);
                if (m->type)
                        type_width = MAX(type_width, strlen(m->type));
                if (m->signature)
                        signature_width = MAX(signature_width, strlen(m->signature));
                if (m->result)
                        result_width = MAX(result_width, strlen(m->result));
                if (m->value)
                        result_width = MAX(result_width, strlen(m->value));

                sorted[k++] = m;
        }

        if (result_width > 40)
                result_width = 40;

        typesafe_qsort(sorted, k, member_compare_funcp);

        if (arg_legend) {
                printf("%-*s %-*s %-*s %-*s %s\n",
                       (int) name_width, "NAME",
                       (int) type_width, "TYPE",
                       (int) signature_width, "SIGNATURE",
                       (int) result_width, "RESULT/VALUE",
                       "FLAGS");
        }

        for (j = 0; j < k; j++) {
                _cleanup_free_ char *ellipsized = NULL;
                const char *rv;
                bool is_interface;

                m = sorted[j];

                if (argv[3] && !streq(argv[3], m->interface))
                        continue;

                is_interface = streq(m->type, "interface");

                if (argv[3] && is_interface)
                        continue;

                if (m->value) {
                        ellipsized = ellipsize(m->value, result_width, 100);
                        if (!ellipsized)
                                return log_oom();

                        rv = ellipsized;
                } else
                        rv = empty_to_dash(m->result);

                printf("%s%s%-*s%s %-*s %-*s %-*s%s%s%s%s%s%s\n",
                       is_interface ? ansi_highlight() : "",
                       is_interface ? "" : ".",
                       - !is_interface + (int) name_width, empty_to_dash(streq_ptr(m->type, "interface") ? m->interface : m->name),
                       is_interface ? ansi_normal() : "",
                       (int) type_width, empty_to_dash(m->type),
                       (int) signature_width, empty_to_dash(m->signature),
                       (int) result_width, rv,
                       (m->flags & SD_BUS_VTABLE_DEPRECATED) ? " deprecated" : (m->flags || m->writable ? "" : " -"),
                       (m->flags & SD_BUS_VTABLE_METHOD_NO_REPLY) ? " no-reply" : "",
                       (m->flags & SD_BUS_VTABLE_PROPERTY_CONST) ? " const" : "",
                       (m->flags & SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE) ? " emits-change" : "",
                       (m->flags & SD_BUS_VTABLE_PROPERTY_EMITS_INVALIDATION) ? " emits-invalidation" : "",
                       m->writable ? " writable" : "");
        }

        return 0;
}

static int message_dump(sd_bus_message *m, FILE *f) {
        return bus_message_dump(m, f, BUS_MESSAGE_DUMP_WITH_HEADER);
}

static int monitor(int argc, char **argv, int (*dump)(sd_bus_message *m, FILE *f)) {
        _cleanup_(sd_bus_flush_close_unrefp) sd_bus *bus = NULL;
        _cleanup_(sd_bus_message_unrefp) sd_bus_message *message = NULL;
        _cleanup_(sd_bus_error_free) sd_bus_error error = SD_BUS_ERROR_NULL;
        char **i;
        uint32_t flags = 0;
        const char *unique_name;
        bool is_monitor = false;
        int r;

        r = acquire_bus(true, &bus);
        if (r < 0)
                return r;

        /* upgrade connection; it's not used for anything else after this call */
        r = sd_bus_message_new_method_call(bus, &message, "org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus.Monitoring", "BecomeMonitor");
        if (r < 0)
                return bus_log_create_error(r);

        r = sd_bus_message_open_container(message, 'a', "s");
        if (r < 0)
                return bus_log_create_error(r);

        STRV_FOREACH(i, argv+1) {
                _cleanup_free_ char *m = NULL;

                if (!service_name_is_valid(*i)) {
                        log_error("Invalid service name '%s'", *i);
                        return -EINVAL;
                }

                m = strjoin("sender='", *i, "'");
                if (!m)
                        return log_oom();

                r = sd_bus_message_append_basic(message, 's', m);
                if (r < 0)
                        return bus_log_create_error(r);

                free(m);
                m = strjoin("destination='", *i, "'");
                if (!m)
                        return log_oom();

                r = sd_bus_message_append_basic(message, 's', m);
                if (r < 0)
                        return bus_log_create_error(r);
        }

        STRV_FOREACH(i, arg_matches) {
                r = sd_bus_message_append_basic(message, 's', *i);
                if (r < 0)
                        return bus_log_create_error(r);
        }

        r = sd_bus_message_close_container(message);
        if (r < 0)
                return bus_log_create_error(r);

        r = sd_bus_message_append_basic(message, 'u', &flags);
        if (r < 0)
                return bus_log_create_error(r);

        r = sd_bus_call(bus, message, arg_timeout, &error, NULL);
        if (r < 0)
                return log_error_errno(r, "%s", bus_error_message(&error, r));

        r = sd_bus_get_unique_name(bus, &unique_name);
        if (r < 0)
                return log_error_errno(r, "Failed to get unique name: %m");

        log_info("Monitoring bus message stream.");

        for (;;) {
                _cleanup_(sd_bus_message_unrefp) sd_bus_message *m = NULL;

                r = sd_bus_process(bus, &m);
                if (r < 0)
                        return log_error_errno(r, "Failed to process bus: %m");

                if (!is_monitor) {
                        const char *name;

                        /* wait until we lose our unique name */
                        if (sd_bus_message_is_signal(m, "org.freedesktop.DBus", "NameLost") <= 0)
                                continue;

                        r = sd_bus_message_read(m, "s", &name);
                        if (r < 0)
                                return log_error_errno(r, "Failed to read lost name: %m");

                        if (streq(name, unique_name))
                                is_monitor = true;

                        continue;
                }

                if (m) {
                        dump(m, stdout);
                        fflush(stdout);

                        if (sd_bus_message_is_signal(m, "org.freedesktop.DBus.Local", "Disconnected") > 0) {
                                log_info("Connection terminated, exiting.");
                                return 0;
                        }

                        continue;
                }

                if (r > 0)
                        continue;

                r = sd_bus_wait(bus, (uint64_t) -1);
                if (r < 0)
                        return log_error_errno(r, "Failed to wait for bus: %m");
        }
}

static int verb_monitor(int argc, char **argv, void *userdata) {
        return monitor(argc, argv, message_dump);
}

static int status(int argc, char **argv, void *userdata) {
        _cleanup_(sd_bus_flush_close_unrefp) sd_bus *bus = NULL;
        _cleanup_(sd_bus_creds_unrefp) sd_bus_creds *creds = NULL;
        pid_t pid;
        int r;

        r = acquire_bus(false, &bus);
        if (r < 0)
                return r;

        if (!isempty(argv[1])) {
                r = parse_pid(argv[1], &pid);
                if (r < 0)
                        r = sd_bus_get_name_creds(
                                        bus,
                                        argv[1],
                                        (arg_augment_creds ? SD_BUS_CREDS_AUGMENT : 0) | _SD_BUS_CREDS_ALL,
                                        &creds);
                else
                        r = sd_bus_creds_new_from_pid(
                                        &creds,
                                        pid,
                                        _SD_BUS_CREDS_ALL);
        } else {
                const char *scope, *address;
                sd_id128_t bus_id;

                r = sd_bus_get_address(bus, &address);
                if (r >= 0)
                        printf("BusAddress=%s%s%s\n", ansi_highlight(), address, ansi_normal());

                r = sd_bus_get_scope(bus, &scope);
                if (r >= 0)
                        printf("BusScope=%s%s%s\n", ansi_highlight(), scope, ansi_normal());

                r = sd_bus_get_bus_id(bus, &bus_id);
                if (r >= 0)
                        printf("BusID=%s" SD_ID128_FORMAT_STR "%s\n", ansi_highlight(), SD_ID128_FORMAT_VAL(bus_id), ansi_normal());

                r = sd_bus_get_owner_creds(
                                bus,
                                (arg_augment_creds ? SD_BUS_CREDS_AUGMENT : 0) | _SD_BUS_CREDS_ALL,
                                &creds);
        }

        if (r < 0)
                return log_error_errno(r, "Failed to get credentials: %m");

        bus_creds_dump(creds, NULL, false);
        return 0;
}

static int message_append_cmdline(sd_bus_message *m, const char *signature, char ***x) {
        char **p;
        int r;

        assert(m);
        assert(signature);
        assert(x);

        p = *x;

        for (;;) {
                const char *v;
                char t;

                t = *signature;
                v = *p;

                if (t == 0)
                        break;
                if (!v) {
                        log_error("Too few parameters for signature.");
                        return -EINVAL;
                }

                signature++;
                p++;

                switch (t) {

                case SD_BUS_TYPE_BOOLEAN:

                        r = parse_boolean(v);
                        if (r < 0)
                                return log_error_errno(r, "Failed to parse '%s' as boolean: %m", v);

                        r = sd_bus_message_append_basic(m, t, &r);
                        break;

                case SD_BUS_TYPE_BYTE: {
                        uint8_t z;

                        r = safe_atou8(v, &z);
                        if (r < 0)
                                return log_error_errno(r, "Failed to parse '%s' as byte (unsigned 8bit integer): %m", v);

                        r = sd_bus_message_append_basic(m, t, &z);
                        break;
                }

                case SD_BUS_TYPE_INT16: {
                        int16_t z;

                        r = safe_atoi16(v, &z);
                        if (r < 0)
                                return log_error_errno(r, "Failed to parse '%s' as signed 16bit integer: %m", v);

                        r = sd_bus_message_append_basic(m, t, &z);
                        break;
                }

                case SD_BUS_TYPE_UINT16: {
                        uint16_t z;

                        r = safe_atou16(v, &z);
                        if (r < 0)
                                return log_error_errno(r, "Failed to parse '%s' as unsigned 16bit integer: %m", v);

                        r = sd_bus_message_append_basic(m, t, &z);
                        break;
                }

                case SD_BUS_TYPE_INT32: {
                        int32_t z;

                        r = safe_atoi32(v, &z);
                        if (r < 0)
                                return log_error_errno(r, "Failed to parse '%s' as signed 32bit integer: %m", v);

                        r = sd_bus_message_append_basic(m, t, &z);
                        break;
                }

                case SD_BUS_TYPE_UINT32: {
                        uint32_t z;

                        r = safe_atou32(v, &z);
                        if (r < 0)
                                return log_error_errno(r, "Failed to parse '%s' as unsigned 32bit integer: %m", v);

                        r = sd_bus_message_append_basic(m, t, &z);
                        break;
                }

                case SD_BUS_TYPE_INT64: {
                        int64_t z;

                        r = safe_atoi64(v, &z);
                        if (r < 0)
                                return log_error_errno(r, "Failed to parse '%s' as signed 64bit integer: %m", v);

                        r = sd_bus_message_append_basic(m, t, &z);
                        break;
                }

                case SD_BUS_TYPE_UINT64: {
                        uint64_t z;

                        r = safe_atou64(v, &z);
                        if (r < 0)
                                return log_error_errno(r, "Failed to parse '%s' as unsigned 64bit integer: %m", v);

                        r = sd_bus_message_append_basic(m, t, &z);
                        break;
                }

                case SD_BUS_TYPE_DOUBLE: {
                        double z;

                        r = safe_atod(v, &z);
                        if (r < 0)
                                return log_error_errno(r, "Failed to parse '%s' as double precision floating point: %m", v);

                        r = sd_bus_message_append_basic(m, t, &z);
                        break;
                }

                case SD_BUS_TYPE_STRING:
                case SD_BUS_TYPE_OBJECT_PATH:
                case SD_BUS_TYPE_SIGNATURE:

                        r = sd_bus_message_append_basic(m, t, v);
                        break;

                case SD_BUS_TYPE_ARRAY: {
                        uint32_t n;
                        size_t k;

                        r = safe_atou32(v, &n);
                        if (r < 0)
                                return log_error_errno(r, "Failed to parse '%s' number of array entries: %m", v);

                        r = signature_element_length(signature, &k);
                        if (r < 0)
                                return log_error_errno(r, "Invalid array signature: %m");

                        {
                                unsigned i;
                                char s[k + 1];
                                memcpy(s, signature, k);
                                s[k] = 0;

                                r = sd_bus_message_open_container(m, SD_BUS_TYPE_ARRAY, s);
                                if (r < 0)
                                        return bus_log_create_error(r);

                                for (i = 0; i < n; i++) {
                                        r = message_append_cmdline(m, s, &p);
                                        if (r < 0)
                                                return r;
                                }
                        }

                        signature += k;

                        r = sd_bus_message_close_container(m);
                        break;
                }

                case SD_BUS_TYPE_VARIANT:
                        r = sd_bus_message_open_container(m, SD_BUS_TYPE_VARIANT, v);
                        if (r < 0)
                                return bus_log_create_error(r);

                        r = message_append_cmdline(m, v, &p);
                        if (r < 0)
                                return r;

                        r = sd_bus_message_close_container(m);
                        break;

                case SD_BUS_TYPE_STRUCT_BEGIN:
                case SD_BUS_TYPE_DICT_ENTRY_BEGIN: {
                        size_t k;

                        signature--;
                        p--;

                        r = signature_element_length(signature, &k);
                        if (r < 0)
                                return log_error_errno(r, "Invalid struct/dict entry signature: %m");

                        {
                                char s[k-1];
                                memcpy(s, signature + 1, k - 2);
                                s[k - 2] = 0;

                                r = sd_bus_message_open_container(m, t == SD_BUS_TYPE_STRUCT_BEGIN ? SD_BUS_TYPE_STRUCT : SD_BUS_TYPE_DICT_ENTRY, s);
                                if (r < 0)
                                        return bus_log_create_error(r);

                                r = message_append_cmdline(m, s, &p);
                                if (r < 0)
                                        return r;
                        }

                        signature += k;

                        r = sd_bus_message_close_container(m);
                        break;
                }

                case SD_BUS_TYPE_UNIX_FD:
                        log_error("UNIX file descriptor not supported as type.");
                        return -EINVAL;

                default:
                        log_error("Unknown signature type %c.", t);
                        return -EINVAL;
                }

                if (r < 0)
                        return bus_log_create_error(r);
        }

        *x = p;
        return 0;
}

static int json_transform_one(sd_bus_message *m, JsonVariant **ret);

static int json_transform_array_or_struct(sd_bus_message *m, JsonVariant **ret) {
        size_t n_elements = 0, n_allocated = 0;
        JsonVariant **elements = NULL;
        int r;

        assert(m);
        assert(ret);

        for (;;) {
                r = sd_bus_message_at_end(m, false);
                if (r < 0) {
                        bus_log_parse_error(r);
                        goto finish;
                }
                if (r > 0)
                        break;

                if (!GREEDY_REALLOC(elements, n_allocated, n_elements + 1)) {
                        r = log_oom();
                        goto finish;
                }

                r = json_transform_one(m, elements + n_elements);
                if (r < 0)
                        goto finish;

                n_elements++;
        }

        r = json_variant_new_array(ret, elements, n_elements);

finish:
        json_variant_unref_many(elements, n_elements);
        free(elements);

        return r;
}

static int json_transform_variant(sd_bus_message *m, const char *contents, JsonVariant **ret) {
        _cleanup_(json_variant_unrefp) JsonVariant *value = NULL;
        int r;

        assert(m);
        assert(contents);
        assert(ret);

        r = json_transform_one(m, &value);
        if (r < 0)
                return r;

        r = json_build(ret, JSON_BUILD_OBJECT(JSON_BUILD_PAIR("type", JSON_BUILD_STRING(contents)),
                                              JSON_BUILD_PAIR("data", JSON_BUILD_VARIANT(value))));
        if (r < 0)
                return log_oom();

        return r;
}

static int json_transform_dict_array(sd_bus_message *m, JsonVariant **ret) {
        size_t n_elements = 0, n_allocated = 0;
        JsonVariant **elements = NULL;
        int r;

        assert(m);
        assert(ret);

        for (;;) {
                const char *contents;
                char type;

                r = sd_bus_message_at_end(m, false);
                if (r < 0) {
                        bus_log_parse_error(r);
                        goto finish;
                }
                if (r > 0)
                        break;

                r = sd_bus_message_peek_type(m, &type, &contents);
                if (r < 0)
                        return r;

                assert(type == 'e');

                if (!GREEDY_REALLOC(elements, n_allocated, n_elements + 2)) {
                        r = log_oom();
                        goto finish;
                }

                r = sd_bus_message_enter_container(m, type, contents);
                if (r < 0) {
                        bus_log_parse_error(r);
                        goto finish;
                }

                r = json_transform_one(m, elements + n_elements);
                if (r < 0)
                        goto finish;

                n_elements++;

                r = json_transform_one(m, elements + n_elements);
                if (r < 0)
                        goto finish;

                n_elements++;

                r = sd_bus_message_exit_container(m);
                if (r < 0) {
                        bus_log_parse_error(r);
                        goto finish;
                }
        }

        r = json_variant_new_object(ret, elements, n_elements);

finish:
        json_variant_unref_many(elements, n_elements);
        free(elements);

        return r;
}

static int json_transform_one(sd_bus_message *m, JsonVariant **ret) {
        _cleanup_(json_variant_unrefp) JsonVariant *v = NULL;
        const char *contents;
        char type;
        int r;

        assert(m);
        assert(ret);

        r = sd_bus_message_peek_type(m, &type, &contents);
        if (r < 0)
                return bus_log_parse_error(r);

        switch (type) {

        case SD_BUS_TYPE_BYTE: {
                uint8_t b;

                r = sd_bus_message_read_basic(m, type, &b);
                if (r < 0)
                        return bus_log_parse_error(r);

                r = json_variant_new_unsigned(&v, b);
                if (r < 0)
                        return log_error_errno(r, "Failed to transform byte: %m");

                break;
        }

        case SD_BUS_TYPE_BOOLEAN: {
                int b;

                r = sd_bus_message_read_basic(m, type, &b);
                if (r < 0)
                        return bus_log_parse_error(r);

                r = json_variant_new_boolean(&v, b);
                if (r < 0)
                        return log_error_errno(r, "Failed to transform boolean: %m");

                break;
        }

        case SD_BUS_TYPE_INT16: {
                int16_t b;

                r = sd_bus_message_read_basic(m, type, &b);
                if (r < 0)
                        return bus_log_parse_error(r);

                r = json_variant_new_integer(&v, b);
                if (r < 0)
                        return log_error_errno(r, "Failed to transform int16: %m");

                break;
        }

        case SD_BUS_TYPE_UINT16: {
                uint16_t b;

                r = sd_bus_message_read_basic(m, type, &b);
                if (r < 0)
                        return bus_log_parse_error(r);

                r = json_variant_new_unsigned(&v, b);
                if (r < 0)
                        return log_error_errno(r, "Failed to transform uint16: %m");

                break;
        }

        case SD_BUS_TYPE_INT32: {
                int32_t b;

                r = sd_bus_message_read_basic(m, type, &b);
                if (r < 0)
                        return bus_log_parse_error(r);

                r = json_variant_new_integer(&v, b);
                if (r < 0)
                        return log_error_errno(r, "Failed to transform int32: %m");

                break;
        }

        case SD_BUS_TYPE_UINT32: {
                uint32_t b;

                r = sd_bus_message_read_basic(m, type, &b);
                if (r < 0)
                        return bus_log_parse_error(r);

                r = json_variant_new_unsigned(&v, b);
                if (r < 0)
                        return log_error_errno(r, "Failed to transform uint32: %m");

                break;
        }

        case SD_BUS_TYPE_INT64: {
                int64_t b;

                r = sd_bus_message_read_basic(m, type, &b);
                if (r < 0)
                        return bus_log_parse_error(r);

                r = json_variant_new_integer(&v, b);
                if (r < 0)
                        return log_error_errno(r, "Failed to transform int64: %m");

                break;
        }

        case SD_BUS_TYPE_UINT64: {
                uint64_t b;

                r = sd_bus_message_read_basic(m, type, &b);
                if (r < 0)
                        return bus_log_parse_error(r);

                r = json_variant_new_unsigned(&v, b);
                if (r < 0)
                        return log_error_errno(r, "Failed to transform uint64: %m");

                break;
        }

        case SD_BUS_TYPE_DOUBLE: {
                double d;

                r = sd_bus_message_read_basic(m, type, &d);
                if (r < 0)
                        return bus_log_parse_error(r);

                r = json_variant_new_real(&v, d);
                if (r < 0)
                        return log_error_errno(r, "Failed to transform double: %m");

                break;
        }

        case SD_BUS_TYPE_STRING:
        case SD_BUS_TYPE_OBJECT_PATH:
        case SD_BUS_TYPE_SIGNATURE: {
                const char *s;

                r = sd_bus_message_read_basic(m, type, &s);
                if (r < 0)
                        return bus_log_parse_error(r);

                r = json_variant_new_string(&v, s);
                if (r < 0)
                        return log_error_errno(r, "Failed to transform double: %m");

                break;
        }

        case SD_BUS_TYPE_UNIX_FD:
                r = sd_bus_message_read_basic(m, type, NULL);
                if (r < 0)
                        return bus_log_parse_error(r);

                r = json_variant_new_null(&v);
                if (r < 0)
                        return log_error_errno(r, "Failed to transform fd: %m");

                break;

        case SD_BUS_TYPE_ARRAY:
        case SD_BUS_TYPE_VARIANT:
        case SD_BUS_TYPE_STRUCT:
                r = sd_bus_message_enter_container(m, type, contents);
                if (r < 0)
                        return bus_log_parse_error(r);

                if (type == SD_BUS_TYPE_VARIANT)
                        r = json_transform_variant(m, contents, &v);
                else if (type == SD_BUS_TYPE_ARRAY && contents[0] == '{')
                        r = json_transform_dict_array(m, &v);
                else
                        r = json_transform_array_or_struct(m, &v);
                if (r < 0)
                        return r;

                r = sd_bus_message_exit_container(m);
                if (r < 0)
                        return bus_log_parse_error(r);

                break;

        default:
                assert_not_reached("Unexpected element type");
        }

        *ret = TAKE_PTR(v);
        return 0;
}

static int json_transform_message(sd_bus_message *m, JsonVariant **ret) {
        _cleanup_(json_variant_unrefp) JsonVariant *v = NULL;
        const char *type;
        int r;

        assert(m);
        assert(ret);

        assert_se(type = sd_bus_message_get_signature(m, false));

        r = json_transform_array_or_struct(m, &v);
        if (r < 0)
                return r;

        r = json_build(ret, JSON_BUILD_OBJECT(JSON_BUILD_PAIR("type",  JSON_BUILD_STRING(type)),
                                              JSON_BUILD_PAIR("data", JSON_BUILD_VARIANT(v))));
        if (r < 0)
                return log_oom();

        return 0;
}

static void json_dump_with_flags(JsonVariant *v, FILE *f) {

        json_variant_dump(v,
                          (arg_json == JSON_PRETTY ? JSON_FORMAT_PRETTY : JSON_FORMAT_NEWLINE) |
                          colors_enabled() * JSON_FORMAT_COLOR,
                          f, NULL);
}

static int call(int argc, char **argv, void *userdata) {
        _cleanup_(sd_bus_flush_close_unrefp) sd_bus *bus = NULL;
        _cleanup_(sd_bus_error_free) sd_bus_error error = SD_BUS_ERROR_NULL;
        _cleanup_(sd_bus_message_unrefp) sd_bus_message *m = NULL, *reply = NULL;
        int r;

        r = acquire_bus(false, &bus);
        if (r < 0)
                return r;

        r = sd_bus_message_new_method_call(bus, &m, argv[1], argv[2], argv[3], argv[4]);
        if (r < 0)
                return bus_log_create_error(r);

        r = sd_bus_message_set_expect_reply(m, arg_expect_reply);
        if (r < 0)
                return bus_log_create_error(r);

        r = sd_bus_message_set_auto_start(m, arg_auto_start);
        if (r < 0)
                return bus_log_create_error(r);

        r = sd_bus_message_set_allow_interactive_authorization(m, arg_allow_interactive_authorization);
        if (r < 0)
                return bus_log_create_error(r);

        if (!isempty(argv[5])) {
                char **p;

                p = argv+6;

                r = message_append_cmdline(m, argv[5], &p);
                if (r < 0)
                        return r;

                if (*p) {
                        log_error("Too many parameters for signature.");
                        return -EINVAL;
                }
        }

        if (!arg_expect_reply) {
                r = sd_bus_send(bus, m, NULL);
                if (r < 0)
                        return log_error_errno(r, "Failed to send message: %m");

                return 0;
        }

        r = sd_bus_call(bus, m, arg_timeout, &error, &reply);
        if (r < 0)
                return log_error_errno(r, "%s", bus_error_message(&error, r));

        r = sd_bus_message_is_empty(reply);
        if (r < 0)
                return bus_log_parse_error(r);

        if (r == 0 && !arg_quiet) {

                if (arg_json != JSON_OFF) {
                        _cleanup_(json_variant_unrefp) JsonVariant *v = NULL;

                        r = json_transform_message(reply, &v);
                        if (r < 0)
                                return r;

                        json_dump_with_flags(v, stdout);

                } else if (arg_verbose) {

                        r = bus_message_dump(reply, stdout, 0);
                        if (r < 0)
                                return r;
                } else {

                        fputs(sd_bus_message_get_signature(reply, true), stdout);
                        fputc(' ', stdout);

                        r = format_cmdline(reply, stdout, false);
                        if (r < 0)
                                return bus_log_parse_error(r);

                        fputc('\n', stdout);
                }
        }

        return 0;
}

static int get_property(int argc, char **argv, void *userdata) {
        _cleanup_(sd_bus_flush_close_unrefp) sd_bus *bus = NULL;
        _cleanup_(sd_bus_error_free) sd_bus_error error = SD_BUS_ERROR_NULL;
        char **i;
        int r;

        r = acquire_bus(false, &bus);
        if (r < 0)
                return r;

        STRV_FOREACH(i, argv + 4) {
                _cleanup_(sd_bus_message_unrefp) sd_bus_message *reply = NULL;
                const char *contents = NULL;
                char type;

                r = sd_bus_call_method(bus, argv[1], argv[2], "org.freedesktop.DBus.Properties", "Get", &error, &reply, "ss", argv[3], *i);
                if (r < 0)
                        return log_error_errno(r, "%s", bus_error_message(&error, r));

                r = sd_bus_message_peek_type(reply, &type, &contents);
                if (r < 0)
                        return bus_log_parse_error(r);

                r = sd_bus_message_enter_container(reply, 'v', contents);
                if (r < 0)
                        return bus_log_parse_error(r);

                if (arg_json != JSON_OFF) {
                        _cleanup_(json_variant_unrefp) JsonVariant *v = NULL;

                        r = json_transform_variant(reply, contents, &v);
                        if (r < 0)
                                return r;

                        json_dump_with_flags(v, stdout);

                } else if (arg_verbose)  {

                        r = bus_message_dump(reply, stdout, BUS_MESSAGE_DUMP_SUBTREE_ONLY);
                        if (r < 0)
                                return r;
                } else {
                        fputs(contents, stdout);
                        fputc(' ', stdout);

                        r = format_cmdline(reply, stdout, false);
                        if (r < 0)
                                return bus_log_parse_error(r);

                        fputc('\n', stdout);
                }

                r = sd_bus_message_exit_container(reply);
                if (r < 0)
                        return bus_log_parse_error(r);
        }

        return 0;
}

static int set_property(int argc, char **argv, void *userdata) {
        _cleanup_(sd_bus_flush_close_unrefp) sd_bus *bus = NULL;
        _cleanup_(sd_bus_message_unrefp) sd_bus_message *m = NULL;
        _cleanup_(sd_bus_error_free) sd_bus_error error = SD_BUS_ERROR_NULL;
        char **p;
        int r;

        r = acquire_bus(false, &bus);
        if (r < 0)
                return r;

        r = sd_bus_message_new_method_call(bus, &m, argv[1], argv[2], "org.freedesktop.DBus.Properties", "Set");
        if (r < 0)
                return bus_log_create_error(r);

        r = sd_bus_message_append(m, "ss", argv[3], argv[4]);
        if (r < 0)
                return bus_log_create_error(r);

        r = sd_bus_message_open_container(m, 'v', argv[5]);
        if (r < 0)
                return bus_log_create_error(r);

        p = argv + 6;
        r = message_append_cmdline(m, argv[5], &p);
        if (r < 0)
                return r;

        r = sd_bus_message_close_container(m);
        if (r < 0)
                return bus_log_create_error(r);

        if (*p) {
                log_error("Too many parameters for signature.");
                return -EINVAL;
        }

        r = sd_bus_call(bus, m, arg_timeout, &error, NULL);
        if (r < 0)
                return log_error_errno(r, "%s", bus_error_message(&error, r));

        return 0;
}

static int help(char *program_name) {
        printf("%s [OPTIONS...] {COMMAND} ...\n\n"
               "Introspect the bus.\n\n"
               "  -h --help               Show this help\n"
               "     --version            Show package version\n"
               "     --no-legend          Do not show the headers and footers\n"
               "     --system             Connect to system bus\n"
               "     --user               Connect to user bus\n"
               "     --address=ADDRESS    Connect to bus specified by address\n"
               "     --show-machine       Show machine ID column in list\n"
               "     --unique             Only show unique names\n"
               "     --acquired           Only show acquired names\n"
               "     --activatable        Only show activatable names\n"
               "     --match=MATCH        Only show matching messages\n"
               "     --list               Don't show tree, but simple object path list\n"
               "  -q --quiet              Don't show method call reply\n"
               "     --verbose            Show result values in long format\n"
               "     --json=MODE          Output as JSON\n"
               "  -j                      Same as --json=pretty on tty, --json=short otherwise\n"
               "     --expect-reply=BOOL  Expect a method call reply\n"
               "     --auto-start=BOOL    Auto-start destination service\n"
               "     --allow-interactive-authorization=BOOL\n"
               "                          Allow interactive authorization for operation\n"
               "     --timeout=SECS       Maximum time to wait for method call completion\n"
               "     --augment-creds=BOOL Extend credential data with data read from /proc/$PID\n"
               "     --watch-bind=BOOL    Wait for bus AF_UNIX socket to be bound in the file\n"
               "                          system\n\n"
               "Commands:\n"
               "  list                    List bus names\n"
               "  status [SERVICE]        Show bus service, process or bus owner credentials\n"
               "  monitor [SERVICE...]    Show bus traffic\n"
               "  tree [SERVICE...]       Show object tree of service\n"
               "  introspect SERVICE OBJECT [INTERFACE]\n"
               "  call SERVICE OBJECT INTERFACE METHOD [SIGNATURE [ARGUMENT...]]\n"
               "                          Call a method\n"
               "  get-property SERVICE OBJECT INTERFACE PROPERTY...\n"
               "                          Get property value\n"
               "  set-property SERVICE OBJECT INTERFACE PROPERTY SIGNATURE ARGUMENT...\n"
               "                          Set property value\n"
               "  help                    Show this help\n"
               "\nSee the %s for details.\n"
               , program_name
               , "busctl(1) man page"
        );

        return 0;
}

static int verb_help(int argc, char **argv, void *userdata) {
        return help(argv[0]);
}

static int parse_argv(int argc, char *argv[]) {

        enum {
                ARG_VERSION = 0x100,
                ARG_NO_LEGEND,
                ARG_SYSTEM,
                ARG_USER,
                ARG_ADDRESS,
                ARG_MATCH,
                ARG_SHOW_MACHINE,
                ARG_UNIQUE,
                ARG_ACQUIRED,
                ARG_ACTIVATABLE,
                ARG_LIST,
                ARG_VERBOSE,
                ARG_EXPECT_REPLY,
                ARG_AUTO_START,
                ARG_ALLOW_INTERACTIVE_AUTHORIZATION,
                ARG_TIMEOUT,
                ARG_AUGMENT_CREDS,
                ARG_WATCH_BIND,
                ARG_JSON,
        };

        static const struct option options[] = {
                { "help",                            no_argument,       NULL, 'h'                                 },
                { "version",                         no_argument,       NULL, ARG_VERSION                         },
                { "no-legend",                       no_argument,       NULL, ARG_NO_LEGEND                       },
                { "system",                          no_argument,       NULL, ARG_SYSTEM                          },
                { "user",                            no_argument,       NULL, ARG_USER                            },
                { "address",                         required_argument, NULL, ARG_ADDRESS                         },
                { "show-machine",                    no_argument,       NULL, ARG_SHOW_MACHINE                    },
                { "unique",                          no_argument,       NULL, ARG_UNIQUE                          },
                { "acquired",                        no_argument,       NULL, ARG_ACQUIRED                        },
                { "activatable",                     no_argument,       NULL, ARG_ACTIVATABLE                     },
                { "match",                           required_argument, NULL, ARG_MATCH                           },
                { "list",                            no_argument,       NULL, ARG_LIST                            },
                { "quiet",                           no_argument,       NULL, 'q'                                 },
                { "verbose",                         no_argument,       NULL, ARG_VERBOSE                         },
                { "expect-reply",                    required_argument, NULL, ARG_EXPECT_REPLY                    },
                { "auto-start",                      required_argument, NULL, ARG_AUTO_START                      },
                { "allow-interactive-authorization", required_argument, NULL, ARG_ALLOW_INTERACTIVE_AUTHORIZATION },
                { "timeout",                         required_argument, NULL, ARG_TIMEOUT                         },
                { "augment-creds",                   required_argument, NULL, ARG_AUGMENT_CREDS                   },
                { "watch-bind",                      required_argument, NULL, ARG_WATCH_BIND                      },
                { "json",                            required_argument, NULL, ARG_JSON                            },
                {},
        };

        int c, r;

        assert(argc >= 0);
        assert(argv);

        while ((c = getopt_long(argc, argv, "hH:M:qj", options, NULL)) >= 0)

                switch (c) {

                case 'h':
                        return help(argv[0]);

                case ARG_VERSION:
                        return version();

                case ARG_NO_LEGEND:
                        arg_legend = false;
                        break;

                case ARG_USER:
                        arg_user = true;
                        break;

                case ARG_SYSTEM:
                        arg_user = false;
                        break;

                case ARG_ADDRESS:
                        arg_address = optarg;
                        break;

                case ARG_SHOW_MACHINE:
                        arg_show_machine = true;
                        break;

                case ARG_UNIQUE:
                        arg_unique = true;
                        break;

                case ARG_ACQUIRED:
                        arg_acquired = true;
                        break;

                case ARG_ACTIVATABLE:
                        arg_activatable = true;
                        break;

                case ARG_MATCH:
                        if (strv_extend(&arg_matches, optarg) < 0)
                                return log_oom();
                        break;

                case ARG_LIST:
                        arg_list = true;
                        break;

                case 'q':
                        arg_quiet = true;
                        break;

                case ARG_VERBOSE:
                        arg_verbose = true;
                        break;

                case ARG_EXPECT_REPLY:
                        r = parse_boolean(optarg);
                        if (r < 0)
                                return log_error_errno(r, "Failed to parse --expect-reply= parameter '%s': %m", optarg);

                        arg_expect_reply = r;
                        break;

                case ARG_AUTO_START:
                        r = parse_boolean(optarg);
                        if (r < 0)
                                return log_error_errno(r, "Failed to parse --auto-start= parameter '%s': %m", optarg);

                        arg_auto_start = r;
                        break;

                case ARG_ALLOW_INTERACTIVE_AUTHORIZATION:
                        r = parse_boolean(optarg);
                        if (r < 0)
                                return log_error_errno(r, "Failed to parse --allow-interactive-authorization= parameter '%s': %m", optarg);

                        arg_allow_interactive_authorization = r;
                        break;

                case ARG_TIMEOUT:
                        r = parse_sec(optarg, &arg_timeout);
                        if (r < 0)
                                return log_error_errno(r, "Failed to parse --timeout= parameter '%s': %m", optarg);

                        break;

                case ARG_AUGMENT_CREDS:
                        r = parse_boolean(optarg);
                        if (r < 0)
                                return log_error_errno(r, "Failed to parse --augment-creds= parameter '%s': %m", optarg);

                        arg_augment_creds = r;
                        break;

                case ARG_WATCH_BIND:
                        r = parse_boolean(optarg);
                        if (r < 0)
                                return log_error_errno(r, "Failed to parse --watch-bind= parameter '%s': %m", optarg);

                        arg_watch_bind = r;
                        break;

                case 'j':
                        if (on_tty())
                                arg_json = JSON_PRETTY;
                        else
                                arg_json = JSON_SHORT;
                        break;

                case ARG_JSON:
                        if (streq(optarg, "short"))
                                arg_json = JSON_SHORT;
                        else if (streq(optarg, "pretty"))
                                arg_json = JSON_PRETTY;
                        else if (streq(optarg, "help")) {
                                fputs("short\n"
                                      "pretty\n", stdout);
                                return 0;
                        } else {
                                log_error("Unknown JSON out mode: %s", optarg);
                                return -EINVAL;
                        }

                        break;

                case '?':
                        return -EINVAL;

                default:
                        assert_not_reached("Unhandled option");
                }

        return 1;
}

static int busctl_main(int argc, char *argv[]) {

        static const Verb verbs[] = {
                { "list",         VERB_ANY, 1,        VERB_DEFAULT, list_bus_names },
                { "status",       VERB_ANY, 2,        0,            status         },
                { "monitor",      VERB_ANY, VERB_ANY, 0,            verb_monitor   },
                { "tree",         VERB_ANY, VERB_ANY, 0,            tree           },
                { "introspect",   3,        4,        0,            introspect     },
                { "call",         5,        VERB_ANY, 0,            call           },
                { "get-property", 5,        VERB_ANY, 0,            get_property   },
                { "set-property", 6,        VERB_ANY, 0,            set_property   },
                { "help",         VERB_ANY, VERB_ANY, 0,            verb_help      },
                {}
        };

        return dispatch_verb(argc, argv, verbs, NULL);
}

int main(int argc, char *argv[]) {
        int r;

        log_parse_environment();

        r = parse_argv(argc, argv);
        if (r <= 0)
                goto finish;

        r = busctl_main(argc, argv);

finish:

        arg_matches = strv_free(arg_matches);

        return r < 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
