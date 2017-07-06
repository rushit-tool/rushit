#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include "script_engine.h"

static void t_create_script_engine(void **state)
{
        struct script_engine *se;
        int r;

        (void) state;

        r = script_engine_create(&se);
        assert_return_code(r, -r);
        assert_non_null(se);

        se = script_engine_destroy(se);
}

int main(void)
{
        const struct CMUnitTest tests[] = {
                cmocka_unit_test(t_create_script_engine),
        };

        return cmocka_run_group_tests(tests, NULL, NULL);
}
