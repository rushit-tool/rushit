#include <stdarg.h>
#include <stddef.h>
#include <stdlib.h>
#include <setjmp.h>
#include <cmocka.h>
#include <unistd.h>

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

static int slave_setup(void **state)
{
        struct script_engine *se = NULL;
        struct script_slave *ss = NULL;
        int r;

        r = engine_setup(state);
        assert_return_code(r, 0);
        se = *state;

        r = script_slave_create(&ss, se);
        assert_return_code(r, -r);
        assert_non_null(ss);

        *state = ss;

        return 0;
}

static int slave_teardown(void **state)
{
        struct script_slave *ss = *state;
        struct script_engine *se = ss->se;
        int r;

        ss = script_slave_destroy(ss);
        assert_null(ss);

        *state = se;
        r = engine_teardown(state);
        assert_return_code(r, 0);

        return 0;
}

static void t_hooks_run_without_errors(void **state)
{
        struct script_engine *se = *state;
        const char *test_script[] = {
                "client_init(function () end)",
                "client_exit(function () end)",
                "client_read(function () end)",
                "client_write(function () end)",
                "client_error(function () end)",
                "server_init(function () end)",
                "server_exit(function () end)",
                "server_read(function () end)",
                "server_write(function () end)",
                "server_error(function () end)",
                "is_client()",
                "is_server()",
                "tid_iter()",
                NULL,
        };
        const char **ts;
        int r;

        for (ts = test_script; *ts; ts++) {
                r = script_engine_run_string(se, *ts, NULL, NULL);
                assert_return_code(r, -r);
        }
}

static void t_run_init_hook_from_string(void **state)
{
        const char *script = "client_init( function () return 42 end )";
        struct script_slave *ss = *state;
        int r;

        r = script_engine_run_string(ss->se, script, NULL, NULL);
        assert_return_code(r, -r);

        r = script_slave_init(ss, -1, NULL);
        assert_int_equal(r, 42);
}

static void t_run_init_hook_from_file(void **state)
{
        const char *script = "client_init( function () return 42 end )";
        char script_path[] = "/tmp/t_run_init_hook_from_file.XXXXXX";
        struct script_slave *ss = *state;
        int fd;
        ssize_t r;

        fd = mkstemp(script_path);
        assert_return_code(fd, errno);

        r = write(fd, script, strlen(script));
        assert_return_code(r, errno);

        r = close(fd);
        assert_return_code(r, errno);

        r = script_engine_run_file(ss->se, script_path, NULL, NULL);
        assert_return_code(r, -r);

        r = script_slave_init(ss, -1, NULL);
        assert_int_equal(r, 42);

        r = unlink(script_path);
        assert_return_code(r, errno);
}

#define engine_unit_test(f) cmocka_unit_test_setup_teardown((f), engine_setup, engine_teardown)
#define slave_unit_test(f) cmocka_unit_test_setup_teardown((f), slave_setup, slave_teardown)

int main(void)
{
        const struct CMUnitTest tests[] = {
                cmocka_unit_test(t_create_script_engine),
                cmocka_unit_test(t_create_script_slave),
                engine_unit_test(t_hooks_run_without_errors),
                slave_unit_test(t_run_init_hook_from_string),
                slave_unit_test(t_run_init_hook_from_file),
        };

        return cmocka_run_group_tests(tests, common_setup, common_teardown);
}
