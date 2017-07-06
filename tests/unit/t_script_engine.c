#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include "script_engine.h"

static void t_se_create_succeeds(void **state)
{
        struct script_engine *se;
        int rc;

        (void) state;

        rc = script_engine_create(&se);
        assert_return_code(rc, -rc);

        se = script_engine_destroy(se);
}

int main(void)
{
        const struct CMUnitTest tests[] = {
                cmocka_unit_test(t_se_create_succeeds),
        };

        return cmocka_run_group_tests(tests, NULL, NULL);
}
