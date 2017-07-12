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

int main(void)
{
        const struct CMUnitTest tests[] = {
                cmocka_unit_test(t_create_script_engine),
                cmocka_unit_test(t_create_script_slave),
        };

        return cmocka_run_group_tests(tests, common_setup, common_teardown);
}
