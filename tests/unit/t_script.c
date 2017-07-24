#include <stdarg.h>
#include <stddef.h>
#include <stdlib.h>
#include <setjmp.h>
#include <cmocka.h>

#include "lib.h"
#include "logging.h"
#include "script.h"

static int common_setup(void **state)
{
        struct callbacks *cb;

        cb = calloc(1, sizeof(*cb));
        assert_non_null(cb);
        logging_init(cb);
        *state = cb;

        return 0;
}

static int common_teardown(void **state)
{
        struct callbacks *cb = *state;

        logging_exit(cb);
        free(*state);

        return 0;
}

static void t_create_script_engine(void **state)
{
        struct script_engine *se = NULL;
        struct callbacks *cb = *state;
        int r;

        r = script_engine_create(&se, cb);
        assert_return_code(r, -r);
        assert_non_null(se);

        se = script_engine_destroy(se);
}

static void t_create_script_slave(void **state)
{
        struct script_engine *se = NULL;
        struct script_slave *ss = NULL;
        struct callbacks *cb = *state;
        int r;

        r = script_engine_create(&se, cb);
        assert_return_code(r, -r);
        assert_non_null(se);

        r = script_slave_create(&ss, se);
        assert_return_code(r, -r);
        assert_non_null(ss);

        ss = script_slave_destroy(ss);
        se = script_engine_destroy(se);
}

static int engine_setup(void **state)
{
        struct script_engine *se = NULL;
        struct callbacks *cb = *state;
        int r;

        r = script_engine_create(&se, cb);
        assert_return_code(r, -r);
        assert_non_null(se);

        *state = se;

        return 0;
}

static int engine_teardown(void **state)
{
        struct script_engine *se = *state;

        se = script_engine_destroy(se);

        return 0;
}

static void t_hooks_run_without_errors(void **state)
{
        struct script_engine *se = *state;
        const char *test_script[] = {
                "client_init()",
                "client_exit()",
                "client_read()",
                "client_write()",
                "client_error()",
                "server_init()",
                "server_exit()",
                "server_read()",
                "server_write()",
                "server_error()",
                "is_client()",
                "is_server()",
                "tid_iter()",
                NULL,
        };
        const char **ts;
        int r;

        for (ts = test_script; *ts; ts++) {
                r = script_engine_run_string(se, *ts);
                assert_return_code(r, -r);
        }
}

static void t_script_slave_init(void **state)
{
        struct script_engine *se = *state;
        struct script_slave *ss = NULL;
        int r;

        r = script_slave_create(&ss, se);
        assert_return_code(r, -r);
        assert_non_null(se);

        r = script_engine_run_string(se, "client_init( function () return 42 end )");
        assert_return_code(r, -r);

        r = script_slave_init(ss, -1, NULL);
        assert_int_equal(r, 42);

        ss = script_slave_destroy(ss);
}

#define engine_unit_test(f) cmocka_unit_test_setup_teardown((f), engine_setup, engine_teardown)

int main(void)
{
        const struct CMUnitTest tests[] = {
                cmocka_unit_test(t_create_script_engine),
                cmocka_unit_test(t_create_script_slave),
                engine_unit_test(t_hooks_run_without_errors),
                engine_unit_test(t_script_slave_init),
        };

        return cmocka_run_group_tests(tests, common_setup, common_teardown);
}
