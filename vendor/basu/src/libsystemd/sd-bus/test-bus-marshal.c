/* SPDX-License-Identifier: LGPL-2.1+ */

#include <math.h>
#include <stdlib.h>

#include "sd-bus.h"

#include "alloc-util.h"
#include "bus-dump.h"
#include "bus-internal.h"
#include "bus-label.h"
#include "bus-message.h"
#include "escape.h"
#include "fd-util.h"
#include "log.h"
#include "tests.h"
#include "util.h"
#include "stdio-util.h"

static int bus_path_encode_unique(sd_bus *b, const char *prefix, const char *sender_id, const char *external_id, char **ret_path) {
        _cleanup_free_ char *sender_label = NULL, *external_label = NULL;
        char external_buf[DECIMAL_STR_MAX(uint64_t)], *p;
        int r;

        assert_return(b || (sender_id && external_id), -EINVAL);
        assert_return(object_path_is_valid(prefix), -EINVAL);
        assert_return(ret_path, -EINVAL);

        if (!sender_id) {
                r = sd_bus_get_unique_name(b, &sender_id);
                if (r < 0)
                        return r;
        }

        if (!external_id) {
                xsprintf(external_buf, "%"PRIu64, ++b->cookie);
                external_id = external_buf;
        }

        sender_label = bus_label_escape(sender_id);
        if (!sender_label)
                return -ENOMEM;

        external_label = bus_label_escape(external_id);
        if (!external_label)
                return -ENOMEM;

        p = strjoin(prefix, "/", sender_label, "/", external_label);
        if (!p)
                return -ENOMEM;

        *ret_path = p;
        return 0;
}

static int bus_path_decode_unique(const char *path, const char *prefix, char **ret_sender, char **ret_external) {
        const char *p, *q;
        char *sender, *external;

        assert(object_path_is_valid(path));
        assert(object_path_is_valid(prefix));
        assert(ret_sender);
        assert(ret_external);

        p = object_path_startswith(path, prefix);
        if (!p) {
                *ret_sender = NULL;
                *ret_external = NULL;
                return 0;
        }

        q = strchr(p, '/');
        if (!q) {
                *ret_sender = NULL;
                *ret_external = NULL;
                return 0;
        }

        sender = bus_label_unescape_n(p, q - p);
        external = bus_label_unescape(q + 1);
        if (!sender || !external) {
                free(sender);
                free(external);
                return -ENOMEM;
        }

        *ret_sender = sender;
        *ret_external = external;
        return 1;
}

static void test_bus_path_encode_unique(void) {
        _cleanup_free_ char *a = NULL, *b = NULL, *c = NULL, *d = NULL, *e = NULL;

        assert_se(bus_path_encode_unique(NULL, "/foo/bar", "some.sender", "a.suffix", &a) >= 0 && streq_ptr(a, "/foo/bar/some_2esender/a_2esuffix"));
        assert_se(bus_path_decode_unique(a, "/foo/bar", &b, &c) > 0 && streq_ptr(b, "some.sender") && streq_ptr(c, "a.suffix"));
        assert_se(bus_path_decode_unique(a, "/bar/foo", &d, &d) == 0 && !d);
        assert_se(bus_path_decode_unique("/foo/bar/onlyOneSuffix", "/foo/bar", &d, &d) == 0 && !d);
        assert_se(bus_path_decode_unique("/foo/bar/_/_", "/foo/bar", &d, &e) > 0 && streq_ptr(d, "") && streq_ptr(e, ""));
}

static void test_bus_path_encode(void) {
        _cleanup_free_ char *a = NULL, *b = NULL, *c = NULL, *d = NULL, *e = NULL, *f = NULL;

        assert_se(sd_bus_path_encode("/foo/bar", "waldo", &a) >= 0 && streq(a, "/foo/bar/waldo"));
        assert_se(sd_bus_path_decode(a, "/waldo", &b) == 0 && b == NULL);
        assert_se(sd_bus_path_decode(a, "/foo/bar", &b) > 0 && streq(b, "waldo"));

        assert_se(sd_bus_path_encode("xxxx", "waldo", &c) < 0);
        assert_se(sd_bus_path_encode("/foo/", "waldo", &c) < 0);

        assert_se(sd_bus_path_encode("/foo/bar", "", &c) >= 0 && streq(c, "/foo/bar/_"));
        assert_se(sd_bus_path_decode(c, "/foo/bar", &d) > 0 && streq(d, ""));

        assert_se(sd_bus_path_encode("/foo/bar", "foo.bar", &e) >= 0 && streq(e, "/foo/bar/foo_2ebar"));
        assert_se(sd_bus_path_decode(e, "/foo/bar", &f) > 0 && streq(f, "foo.bar"));
}

static void test_bus_path_encode_many(void) {
        _cleanup_free_ char *a = NULL, *b = NULL, *c = NULL, *d = NULL, *e = NULL, *f = NULL;

        assert_se(sd_bus_path_decode_many("/foo/bar", "/prefix/%", NULL) == 0);
        assert_se(sd_bus_path_decode_many("/prefix/bar", "/prefix/%bar", NULL) == 1);
        assert_se(sd_bus_path_decode_many("/foo/bar", "/prefix/%/suffix", NULL) == 0);
        assert_se(sd_bus_path_decode_many("/prefix/foobar/suffix", "/prefix/%/suffix", &a) == 1 && streq_ptr(a, "foobar"));
        assert_se(sd_bus_path_decode_many("/prefix/one_foo_two/mid/three_bar_four/suffix", "/prefix/one_%_two/mid/three_%_four/suffix", &b, &c) == 1 && streq_ptr(b, "foo") && streq_ptr(c, "bar"));
        assert_se(sd_bus_path_decode_many("/prefix/one_foo_two/mid/three_bar_four/suffix", "/prefix/one_%_two/mid/three_%_four/suffix", NULL, &d) == 1 && streq_ptr(d, "bar"));

        assert_se(sd_bus_path_decode_many("/foo/bar", "/foo/bar/%", NULL) == 0);
        assert_se(sd_bus_path_decode_many("/foo/bar/suffix", "/foo/bar%", NULL) == 0);
        assert_se(sd_bus_path_decode_many("/foo/bar/suffix", "/foo/%/bar", NULL) == 0);
        assert_se(sd_bus_path_decode_many("/foo/bar/suffix", "/foo/%bar", NULL) == 0);
        assert_se(sd_bus_path_decode_many("/foo/bar/suffix", "/foo/bar/suffix") == 1);
        assert_se(sd_bus_path_decode_many("/foo/bar/suffix", "/foo/%%/suffix", NULL, NULL) == 0); /* multiple '%' are treated verbatim */
        assert_se(sd_bus_path_decode_many("/foo/bar/suffix", "/foo/%/suffi", NULL) == 0);
        assert_se(sd_bus_path_decode_many("/foo/bar/suffix", "/foo/%/suffix", &e) == 1 && streq_ptr(e, "bar"));
        assert_se(sd_bus_path_decode_many("/foo/bar/suffix", "/foo/%/%", NULL, NULL) == 1);
        assert_se(sd_bus_path_decode_many("/foo/bar/suffix", "/%/%/%", NULL, NULL, NULL) == 1);
        assert_se(sd_bus_path_decode_many("/foo/bar/suffix", "%/%/%", NULL, NULL, NULL) == 0);
        assert_se(sd_bus_path_decode_many("/foo/bar/suffix", "/%/%", NULL, NULL) == 0);
        assert_se(sd_bus_path_decode_many("/foo/bar/suffix", "/%/%/", NULL, NULL) == 0);
        assert_se(sd_bus_path_decode_many("/foo/bar/suffix", "/%/", NULL) == 0);
        assert_se(sd_bus_path_decode_many("/foo/bar/suffix", "/%", NULL) == 0);
        assert_se(sd_bus_path_decode_many("/foo/bar/suffix", "%", NULL) == 0);

        assert_se(sd_bus_path_encode_many(&f, "/prefix/one_%_two/mid/three_%_four/suffix", "foo", "bar") >= 0 && streq_ptr(f, "/prefix/one_foo_two/mid/three_bar_four/suffix"));
}

static void test_bus_label_escape_one(const char *a, const char *b) {
        _cleanup_free_ char *t = NULL, *x = NULL, *y = NULL;

        assert_se(t = bus_label_escape(a));
        assert_se(streq(t, b));

        assert_se(x = bus_label_unescape(t));
        assert_se(streq(a, x));

        assert_se(y = bus_label_unescape(b));
        assert_se(streq(a, y));
}

static void test_bus_label_escape(void) {
        test_bus_label_escape_one("foo123bar", "foo123bar");
        test_bus_label_escape_one("foo.bar", "foo_2ebar");
        test_bus_label_escape_one("foo_2ebar", "foo_5f2ebar");
        test_bus_label_escape_one("", "_");
        test_bus_label_escape_one("_", "_5f");
        test_bus_label_escape_one("1", "_31");
        test_bus_label_escape_one(":1", "_3a1");
}

int main(int argc, char *argv[]) {
        _cleanup_(sd_bus_message_unrefp) sd_bus_message *m = NULL, *copy = NULL;
        int r, boolean;
        const char *x, *x2, *y, *z, *a, *b, *c, *d, *a_signature;
        uint8_t u, v;
        void *buffer = NULL;
        size_t sz;
        _cleanup_free_ char *h = NULL;
        const int32_t integer_array[] = { -1, -2, 0, 1, 2 }, *return_array;
        char *s;
        _cleanup_free_ char *first = NULL, *second = NULL, *third = NULL;
        _cleanup_fclose_ FILE *ms = NULL;
        size_t first_size = 0, second_size = 0, third_size = 0;
        _cleanup_(sd_bus_unrefp) sd_bus *bus = NULL;
        double dbl;
        uint64_t u64;

        test_setup_logging(LOG_INFO);

        r = sd_bus_default_user(&bus);
        if (r < 0)
                r = sd_bus_default_system(&bus);
        if (r < 0)
                return log_tests_skipped("Failed to connect to bus");

        r = sd_bus_message_new_method_call(bus, &m, "foobar.waldo", "/", "foobar.waldo", "Piep");
        assert_se(r >= 0);

        r = sd_bus_message_append(m, "");
        assert_se(r >= 0);

        r = sd_bus_message_append(m, "s", "a string");
        assert_se(r >= 0);

        r = sd_bus_message_append(m, "s", NULL);
        assert_se(r >= 0);

        r = sd_bus_message_append(m, "asg", 2, "string #1", "string #2", "sba(tt)ss");
        assert_se(r >= 0);

        r = sd_bus_message_append(m, "sass", "foobar", 5, "foo", "bar", "waldo", "piep", "pap", "after");
        assert_se(r >= 0);

        r = sd_bus_message_append(m, "a{yv}", 2, 3, "s", "foo", 5, "s", "waldo");
        assert_se(r >= 0);

        r = sd_bus_message_append(m, "y(ty)y(yt)y", 8, 777ULL, 7, 9, 77, 7777ULL, 10);
        assert_se(r >= 0);

        r = sd_bus_message_append(m, "()");
        assert_se(r == -EINVAL);

        r = sd_bus_message_append(m, "ba(ss)", 255, 3, "aaa", "1", "bbb", "2", "ccc", "3");
        assert_se(r >= 0);

        r = sd_bus_message_open_container(m, 'a', "s");
        assert_se(r >= 0);

        r = sd_bus_message_append_basic(m, 's', "foobar");
        assert_se(r >= 0);

        r = sd_bus_message_append_basic(m, 's', "waldo");
        assert_se(r >= 0);

        r = sd_bus_message_close_container(m);
        assert_se(r >= 0);

        r = sd_bus_message_append_string_space(m, 5, &s);
        assert_se(r >= 0);
        strcpy(s, "hallo");

        r = sd_bus_message_append_array(m, 'i', integer_array, sizeof(integer_array));
        assert_se(r >= 0);

        r = sd_bus_message_append_array(m, 'u', NULL, 0);
        assert_se(r >= 0);

        r = sd_bus_message_append(m, "a(stdo)", 1, "foo", 815ULL, 47.0, "/");
        assert_se(r >= 0);

        r = sd_bus_message_seal(m, 4711, 0);
        assert_se(r >= 0);

        bus_message_dump(m, stdout, BUS_MESSAGE_DUMP_WITH_HEADER);

        ms = open_memstream(&first, &first_size);
        bus_message_dump(m, ms, 0);
        fflush(ms);
        assert_se(!ferror(ms));

        r = bus_message_get_blob(m, &buffer, &sz);
        assert_se(r >= 0);

        h = cescape_length(buffer, sz);
        assert_se(h);
        log_info("message size = %zu, contents =\n%s", sz, h);

        m = sd_bus_message_unref(m);

        r = bus_message_from_malloc(bus, buffer, sz, NULL, 0, NULL, &m);
        assert_se(r >= 0);

        bus_message_dump(m, stdout, BUS_MESSAGE_DUMP_WITH_HEADER);

        fclose(ms);
        ms = open_memstream(&second, &second_size);
        bus_message_dump(m, ms, 0);
        fflush(ms);
        assert_se(!ferror(ms));
        assert_se(first_size == second_size);
        assert_se(memcmp(first, second, first_size) == 0);

        assert_se(sd_bus_message_rewind(m, true) >= 0);

        r = sd_bus_message_read(m, "ssasg", &x, &x2, 2, &y, &z, &a_signature);
        assert_se(r > 0);
        assert_se(streq(x, "a string"));
        assert_se(streq(x2, ""));
        assert_se(streq(y, "string #1"));
        assert_se(streq(z, "string #2"));
        assert_se(streq(a_signature, "sba(tt)ss"));

        r = sd_bus_message_read(m, "sass", &x, 5, &y, &z, &a, &b, &c, &d);
        assert_se(r > 0);
        assert_se(streq(x, "foobar"));
        assert_se(streq(y, "foo"));
        assert_se(streq(z, "bar"));
        assert_se(streq(a, "waldo"));
        assert_se(streq(b, "piep"));
        assert_se(streq(c, "pap"));
        assert_se(streq(d, "after"));

        r = sd_bus_message_read(m, "a{yv}", 2, &u, "s", &x, &v, "s", &y);
        assert_se(r > 0);
        assert_se(u == 3);
        assert_se(streq(x, "foo"));
        assert_se(v == 5);
        assert_se(streq(y, "waldo"));

        r = sd_bus_message_read(m, "y(ty)", &v, &u64, &u);
        assert_se(r > 0);
        assert_se(v == 8);
        assert_se(u64 == 777);
        assert_se(u == 7);

        r = sd_bus_message_read(m, "y(yt)", &v, &u, &u64);
        assert_se(r > 0);
        assert_se(v == 9);
        assert_se(u == 77);
        assert_se(u64 == 7777);

        r = sd_bus_message_read(m, "y", &v);
        assert_se(r > 0);
        assert_se(v == 10);

        r = sd_bus_message_read(m, "()");
        assert_se(r < 0);

        r = sd_bus_message_read(m, "ba(ss)", &boolean, 3, &x, &y, &a, &b, &c, &d);
        assert_se(r > 0);
        assert_se(boolean);
        assert_se(streq(x, "aaa"));
        assert_se(streq(y, "1"));
        assert_se(streq(a, "bbb"));
        assert_se(streq(b, "2"));
        assert_se(streq(c, "ccc"));
        assert_se(streq(d, "3"));

        assert_se(sd_bus_message_verify_type(m, 'a', "s") > 0);

        r = sd_bus_message_read(m, "as", 2, &x, &y);
        assert_se(r > 0);
        assert_se(streq(x, "foobar"));
        assert_se(streq(y, "waldo"));

        r = sd_bus_message_read_basic(m, 's', &s);
        assert_se(r > 0);
        assert_se(streq(s, "hallo"));

        r = sd_bus_message_read_array(m, 'i', (const void**) &return_array, &sz);
        assert_se(r > 0);
        assert_se(sz == sizeof(integer_array));
        assert_se(memcmp(integer_array, return_array, sz) == 0);

        r = sd_bus_message_read_array(m, 'u', (const void**) &return_array, &sz);
        assert_se(r > 0);
        assert_se(sz == 0);

        r = sd_bus_message_read(m, "a(stdo)", 1, &x, &u64, &dbl, &y);
        assert_se(r > 0);
        assert_se(streq(x, "foo"));
        assert_se(u64 == 815ULL);
        assert_se(fabs(dbl - 47.0) < 0.1);
        assert_se(streq(y, "/"));

        r = sd_bus_message_peek_type(m, NULL, NULL);
        assert_se(r == 0);

        r = sd_bus_message_new_method_call(bus, &copy, "foobar.waldo", "/", "foobar.waldo", "Piep");
        assert_se(r >= 0);

        r = sd_bus_message_rewind(m, true);
        assert_se(r >= 0);

        r = sd_bus_message_copy(copy, m, true);
        assert_se(r >= 0);

        r = sd_bus_message_seal(copy, 4712, 0);
        assert_se(r >= 0);

        fclose(ms);
        ms = open_memstream(&third, &third_size);
        bus_message_dump(copy, ms, 0);
        fflush(ms);
        assert_se(!ferror(ms));

        printf("<%.*s>\n", (int) first_size, first);
        printf("<%.*s>\n", (int) third_size, third);

        assert_se(first_size == third_size);
        assert_se(memcmp(first, third, third_size) == 0);

        r = sd_bus_message_rewind(m, true);
        assert_se(r >= 0);

        assert_se(sd_bus_message_verify_type(m, 's', NULL) > 0);

        r = sd_bus_message_skip(m, "ssasg");
        assert_se(r > 0);

        assert_se(sd_bus_message_verify_type(m, 's', NULL) > 0);

        r = sd_bus_message_skip(m, "sass");
        assert_se(r >= 0);

        assert_se(sd_bus_message_verify_type(m, 'a', "{yv}") > 0);

        r = sd_bus_message_skip(m, "a{yv}y(ty)y(yt)y");
        assert_se(r >= 0);

        assert_se(sd_bus_message_verify_type(m, 'b', NULL) > 0);

        r = sd_bus_message_read(m, "b", &boolean);
        assert_se(r > 0);
        assert_se(boolean);

        r = sd_bus_message_enter_container(m, 0, NULL);
        assert_se(r > 0);

        r = sd_bus_message_read(m, "(ss)", &x, &y);
        assert_se(r > 0);

        r = sd_bus_message_read(m, "(ss)", &a, &b);
        assert_se(r > 0);

        r = sd_bus_message_read(m, "(ss)", &c, &d);
        assert_se(r > 0);

        r = sd_bus_message_read(m, "(ss)", &x, &y);
        assert_se(r == 0);

        r = sd_bus_message_exit_container(m);
        assert_se(r >= 0);

        assert_se(streq(x, "aaa"));
        assert_se(streq(y, "1"));
        assert_se(streq(a, "bbb"));
        assert_se(streq(b, "2"));
        assert_se(streq(c, "ccc"));
        assert_se(streq(d, "3"));

        test_bus_label_escape();
        test_bus_path_encode();
        test_bus_path_encode_unique();
        test_bus_path_encode_many();

        return 0;
}
