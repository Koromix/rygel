/* SPDX-License-Identifier: LGPL-2.1+ */

#include "sd-bus.h"

#include "alloc-util.h"
#include "bus-dump.h"
#include "bus-gvariant.h"
#include "bus-internal.h"
#include "bus-message.h"
#include "macro.h"
#include "tests.h"
#include "util.h"

static void test_bus_gvariant_is_fixed_size(void) {
        log_info("/* %s */", __func__);

        assert_se(bus_gvariant_is_fixed_size("") > 0);
        assert_se(bus_gvariant_is_fixed_size("()") == -EINVAL);
        assert_se(bus_gvariant_is_fixed_size("y") > 0);
        assert_se(bus_gvariant_is_fixed_size("u") > 0);
        assert_se(bus_gvariant_is_fixed_size("b") > 0);
        assert_se(bus_gvariant_is_fixed_size("n") > 0);
        assert_se(bus_gvariant_is_fixed_size("q") > 0);
        assert_se(bus_gvariant_is_fixed_size("i") > 0);
        assert_se(bus_gvariant_is_fixed_size("t") > 0);
        assert_se(bus_gvariant_is_fixed_size("d") > 0);
        assert_se(bus_gvariant_is_fixed_size("s") == 0);
        assert_se(bus_gvariant_is_fixed_size("o") == 0);
        assert_se(bus_gvariant_is_fixed_size("g") == 0);
        assert_se(bus_gvariant_is_fixed_size("h") > 0);
        assert_se(bus_gvariant_is_fixed_size("ay") == 0);
        assert_se(bus_gvariant_is_fixed_size("v") == 0);
        assert_se(bus_gvariant_is_fixed_size("(u)") > 0);
        assert_se(bus_gvariant_is_fixed_size("(uuuuy)") > 0);
        assert_se(bus_gvariant_is_fixed_size("(uusuuy)") == 0);
        assert_se(bus_gvariant_is_fixed_size("a{ss}") == 0);
        assert_se(bus_gvariant_is_fixed_size("((u)yyy(b(iiii)))") > 0);
        assert_se(bus_gvariant_is_fixed_size("((u)yyy(b(iiivi)))") == 0);
}

static void test_bus_gvariant_get_size(void) {
        log_info("/* %s */", __func__);

        assert_se(bus_gvariant_get_size("") == 0);
        assert_se(bus_gvariant_get_size("()") == -EINVAL);
        assert_se(bus_gvariant_get_size("y") == 1);
        assert_se(bus_gvariant_get_size("u") == 4);
        assert_se(bus_gvariant_get_size("b") == 1);
        assert_se(bus_gvariant_get_size("n") == 2);
        assert_se(bus_gvariant_get_size("q") == 2);
        assert_se(bus_gvariant_get_size("i") == 4);
        assert_se(bus_gvariant_get_size("t") == 8);
        assert_se(bus_gvariant_get_size("d") == 8);
        assert_se(bus_gvariant_get_size("s") < 0);
        assert_se(bus_gvariant_get_size("o") < 0);
        assert_se(bus_gvariant_get_size("g") < 0);
        assert_se(bus_gvariant_get_size("h") == 4);
        assert_se(bus_gvariant_get_size("ay") < 0);
        assert_se(bus_gvariant_get_size("v") < 0);
        assert_se(bus_gvariant_get_size("(u)") == 4);
        assert_se(bus_gvariant_get_size("(uuuuy)") == 20);
        assert_se(bus_gvariant_get_size("(uusuuy)") < 0);
        assert_se(bus_gvariant_get_size("a{ss}") < 0);
        assert_se(bus_gvariant_get_size("((u)yyy(b(iiii)))") == 28);
        assert_se(bus_gvariant_get_size("((u)yyy(b(iiivi)))") < 0);
        assert_se(bus_gvariant_get_size("((b)(t))") == 16);
        assert_se(bus_gvariant_get_size("((b)(b)(t))") == 16);
        assert_se(bus_gvariant_get_size("(bt)") == 16);
        assert_se(bus_gvariant_get_size("((t)(b))") == 16);
        assert_se(bus_gvariant_get_size("(tb)") == 16);
        assert_se(bus_gvariant_get_size("((b)(b))") == 2);
        assert_se(bus_gvariant_get_size("((t)(t))") == 16);
}

static void test_bus_gvariant_get_alignment(void) {
        log_info("/* %s */", __func__);

        assert_se(bus_gvariant_get_alignment("") == 1);
        assert_se(bus_gvariant_get_alignment("()") == -EINVAL);
        assert_se(bus_gvariant_get_alignment("y") == 1);
        assert_se(bus_gvariant_get_alignment("b") == 1);
        assert_se(bus_gvariant_get_alignment("u") == 4);
        assert_se(bus_gvariant_get_alignment("s") == 1);
        assert_se(bus_gvariant_get_alignment("o") == 1);
        assert_se(bus_gvariant_get_alignment("g") == 1);
        assert_se(bus_gvariant_get_alignment("v") == 8);
        assert_se(bus_gvariant_get_alignment("h") == 4);
        assert_se(bus_gvariant_get_alignment("i") == 4);
        assert_se(bus_gvariant_get_alignment("t") == 8);
        assert_se(bus_gvariant_get_alignment("x") == 8);
        assert_se(bus_gvariant_get_alignment("q") == 2);
        assert_se(bus_gvariant_get_alignment("n") == 2);
        assert_se(bus_gvariant_get_alignment("d") == 8);
        assert_se(bus_gvariant_get_alignment("ay") == 1);
        assert_se(bus_gvariant_get_alignment("as") == 1);
        assert_se(bus_gvariant_get_alignment("au") == 4);
        assert_se(bus_gvariant_get_alignment("an") == 2);
        assert_se(bus_gvariant_get_alignment("ans") == 2);
        assert_se(bus_gvariant_get_alignment("ant") == 8);
        assert_se(bus_gvariant_get_alignment("(ss)") == 1);
        assert_se(bus_gvariant_get_alignment("(ssu)") == 4);
        assert_se(bus_gvariant_get_alignment("a(ssu)") == 4);
        assert_se(bus_gvariant_get_alignment("(u)") == 4);
        assert_se(bus_gvariant_get_alignment("(uuuuy)") == 4);
        assert_se(bus_gvariant_get_alignment("(uusuuy)") == 4);
        assert_se(bus_gvariant_get_alignment("a{ss}") == 1);
        assert_se(bus_gvariant_get_alignment("((u)yyy(b(iiii)))") == 4);
        assert_se(bus_gvariant_get_alignment("((u)yyy(b(iiivi)))") == 8);
        assert_se(bus_gvariant_get_alignment("((b)(t))") == 8);
        assert_se(bus_gvariant_get_alignment("((b)(b)(t))") == 8);
        assert_se(bus_gvariant_get_alignment("(bt)") == 8);
        assert_se(bus_gvariant_get_alignment("((t)(b))") == 8);
        assert_se(bus_gvariant_get_alignment("(tb)") == 8);
        assert_se(bus_gvariant_get_alignment("((b)(b))") == 1);
        assert_se(bus_gvariant_get_alignment("((t)(t))") == 8);
}

static int test_marshal(void) {
        _cleanup_(sd_bus_message_unrefp) sd_bus_message *m = NULL, *n = NULL;
        _cleanup_(sd_bus_flush_close_unrefp) sd_bus *bus = NULL;
        _cleanup_free_ void *blob = NULL;
        size_t sz;
        int r;

        r = sd_bus_open_user(&bus);
        if (r < 0)
                r = sd_bus_open_system(&bus);
        if (r < 0)
                return log_tests_skipped_errno(r, "Failed to connect to bus");

        bus->message_version = 2; /* dirty hack to enable gvariant */

        r = sd_bus_message_new_method_call(bus, &m, "a.service.name",
                                           "/an/object/path/which/is/really/really/long/so/that/we/hit/the/eight/bit/boundary/by/quite/some/margin/to/test/this/stuff/that/it/really/works",
                                           "an.interface.name", "AMethodName");
        assert_se(r >= 0);

        assert_cc(sizeof(struct bus_header) == 16);

        assert_se(sd_bus_message_append(m,
                                        "a(usv)", 3,
                                        4711, "first-string-parameter", "(st)", "X", (uint64_t) 1111,
                                        4712, "second-string-parameter", "(a(si))", 2, "Y", 5, "Z", 6,
                                        4713, "third-string-parameter", "(uu)", 1, 2) >= 0);

        assert_se(sd_bus_message_seal(m, 4711, 0) >= 0);

        assert_se(bus_message_dump(m, NULL, BUS_MESSAGE_DUMP_WITH_HEADER) >= 0);

        assert_se(bus_message_get_blob(m, &blob, &sz) >= 0);

        assert_se(bus_message_from_malloc(bus, blob, sz, NULL, 0, NULL, &n) >= 0);
        blob = NULL;

        assert_se(bus_message_dump(n, NULL, BUS_MESSAGE_DUMP_WITH_HEADER) >= 0);

        m = sd_bus_message_unref(m);

        assert_se(sd_bus_message_new_method_call(bus, &m, "a.x", "/a/x", "a.x", "Ax") >= 0);

        assert_se(sd_bus_message_append(m, "as", 0) >= 0);

        assert_se(sd_bus_message_seal(m, 4712, 0) >= 0);
        assert_se(bus_message_dump(m, NULL, BUS_MESSAGE_DUMP_WITH_HEADER) >= 0);

        return EXIT_SUCCESS;
}

int main(int argc, char *argv[]) {
        test_setup_logging(LOG_DEBUG);

        test_bus_gvariant_is_fixed_size();
        test_bus_gvariant_get_size();
        test_bus_gvariant_get_alignment();

        return test_marshal();
}
