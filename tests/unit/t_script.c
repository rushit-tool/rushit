#include <netdb.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <setjmp.h>
#include <cmocka.h>
#include <unistd.h>

#include "common.h"
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
        bool is_client = false;
        int r;

        r = script_engine_create(&se, cb, is_client);
        assert_return_code(r, -r);
        assert_non_null(se);

        se = script_engine_destroy(se);
}

static void t_create_script_slave(void **state)
{
        struct script_engine *se = NULL;
        struct script_slave *ss = NULL;
        struct callbacks *cb = *state;
        bool is_client = false;
        int r;

        r = script_engine_create(&se, cb, is_client);
        assert_return_code(r, -r);
        assert_non_null(se);

        r = script_slave_create(&ss, se);
        assert_return_code(r, -r);
        assert_non_null(ss);

        ss = script_slave_destroy(ss);
        se = script_engine_destroy(se);
}

static int client_engine_setup(void **state)
{
        struct script_engine *se = NULL;
        struct callbacks *cb = *state;
        bool is_client = true;
        int r;

        r = script_engine_create(&se, cb, is_client);
        assert_return_code(r, -r);
        assert_non_null(se);

        *state = se;

        return 0;
}

static int client_engine_teardown(void **state)
{
        struct script_engine *se = *state;

        se = script_engine_destroy(se);
        assert_null(se);

        return 0;
}

static int client_slave_setup(void **state)
{
        struct script_engine *se = NULL;
        struct script_slave *ss = NULL;
        int r;

        r = client_engine_setup(state);
        assert_return_code(r, 0);
        se = *state;

        r = script_slave_create(&ss, se);
        assert_return_code(r, -r);
        assert_non_null(ss);

        *state = ss;

        return 0;
}

static int client_slave_teardown(void **state)
{
        struct script_slave *ss = *state;
        struct script_engine *se = ss->se;
        int r;

        ss = script_slave_destroy(ss);
        assert_null(ss);

        *state = se;
        r = client_engine_teardown(state);
        assert_return_code(r, 0);

        return 0;
}

static void t_hooks_run_without_errors(void **state)
{
        struct script_engine *se = *state;
        const char *test_script[] = {
                "client_socket(function () end)",
                "client_close(function () end)",
                "client_sendmsg(function () end)",
                "client_recvmsg(function () end)",
                "client_recverr(function () end)",
                "server_socket(function () end)",
                "server_close(function () end)",
                "server_sendmsg(function () end)",
                "server_recvmsg(function () end)",
                "server_recverr(function () end)",
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

static void wait_func(struct script_engine *unused, void *done_)
{
        bool *done = done_;
        *done = true;
        (void) unused;
}

static void t_wait_func_gets_called(void **state)
{
        struct script_engine *se = *state;
        bool wait_done = false;
        int r;

        r = script_engine_run_string(se, "", wait_func, &wait_done);
        assert_return_code(r, -r);
        assert_true(wait_done);
}

static void toggle_flag(struct script_engine *unused, void *flag_)
{
        bool *flag = flag_;
        *flag = !*flag;
        (void) unused;
}

static void t_run_cb_gets_invoked(void **state)
{
        struct script_engine *se = *state;
        bool done;
        int r;

        /* explicit invocation from script */
        done = false;
        r = script_engine_run_string(se, "run()", toggle_flag, &done);
        assert_return_code(r, -r);
        assert_true(done);

        /* implicit invocation by engine */
        done = false;
        r = script_engine_run_string(se, "", toggle_flag, &done);
        assert_return_code(r, -r);
        assert_true(done);
}

static void t_run_socket_hook_from_string(void **state)
{
        const char *script = "client_socket( function () return 42 end )";
        struct script_slave *ss = *state;
        int r;

        r = script_engine_run_string(ss->se, script, NULL, NULL);
        assert_return_code(r, -r);

        r = script_slave_socket_hook(ss, -1, NULL);
        assert_int_equal(r, 42);
}

static void t_run_socket_hook_from_file(void **state)
{
        const char *script = "client_socket( function () return 42 end )";
        char script_path[] = "/tmp/t_run_socket_hook_from_file.XXXXXX";
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

        r = script_slave_socket_hook(ss, -1, NULL);
        assert_int_equal(r, 42);

        r = unlink(script_path);
        assert_return_code(r, errno);
}

static void t_run_close_hook(void **state)
{
        const char *script = "client_close( function () return 42 end )";
        struct script_slave *ss = *state;
        int r;

        r = script_engine_run_string(ss->se, script, NULL, NULL);
        assert_return_code(r, -r);

        r = script_slave_close_hook(ss, -1, NULL);
        assert_int_equal(r, 42);
}

static void t_run_sendmsg_hook(void **state)
{
        const char *script = "client_sendmsg( function () return 11015 end )";
        struct script_slave *ss = *state;
        int r;

        r = script_engine_run_string(ss->se, script, NULL, NULL);
        assert_return_code(r, -r);

        r = script_slave_sendmsg_hook(ss, -1, NULL, 0);
        assert_int_equal(r, 11015);
}

static void t_run_recvmsg_hook(void **state)
{
        const char *script = "client_recvmsg( function () return 28139 end )";
        struct script_slave *ss = *state;
        int r;

        r = script_engine_run_string(ss->se, script, NULL, NULL);
        assert_return_code(r, -r);

        r = script_slave_recvmsg_hook(ss, -1, NULL, 0);
        assert_int_equal(r, 28139);
}

static void t_run_recverr_hook(void **state)
{
        const char *script = "client_recverr( function () return 7193 end )";
        struct script_slave *ss = *state;
        int r;

        r = script_engine_run_string(ss->se, script, NULL, NULL);
        assert_return_code(r, -r);

        r = script_slave_recverr_hook(ss, -1, NULL, 0);
        assert_int_equal(r, 7193);
}

#define lua_assert(expr, op, val) lua_assert_(#expr, #op, #val)
#define lua_assert_(expr, op, val) \
        "assert(" expr " " op " " val ", \"expected " expr " to be " val ", got \" .. tostring(" expr "));\n"

#define lua_assert_nil(expr) lua_assert(expr, ==, nil)
#define lua_assert_not_nil(expr) lua_assert(expr, ~=, nil)

#define lua_assert_equal(expr, val) lua_assert(expr, ==, val)
#define lua_assert_not_equal(expr, val) lua_assert(expr, ~=, val)

#define lua_assert_true(expr) lua_assert_(#expr, "==", "true")
#define lua_assert_false(expr) lua_assert_(#expr, "==", "false")

static void t_pass_args_to_socket_hook(void **state)
{
        int fd = 1234;
        struct addrinfo ai = {
                .ai_flags = 0,
                .ai_family = AF_INET,
                .ai_socktype = SOCK_STREAM,
                .ai_protocol = IPPROTO_TCP,
                .ai_addrlen = sizeof(struct sockaddr_in),
                .ai_addr = (struct sockaddr *) &(struct sockaddr_in) {
                        .sin_family = AF_INET,
                        .sin_port = htons(1234),
                        .sin_addr = { htonl(0x01020304) },
                },
                .ai_canonname = NULL,
                .ai_next = NULL,
        };
        const char *script =
                "client_socket("
                "  function (fd, ai)"
                "    " lua_assert_equal(fd, 1234)
                "    " lua_assert_equal(ai.ai_flags, 0)
                "    " lua_assert_equal(ai.ai_family, AF_INET)
                "    " lua_assert_equal(ai.ai_socktype, SOCK_STREAM)
                "    " lua_assert_equal(ai.ai_protocol, IPPROTO_TCP)
                "    " lua_assert_equal(ai.ai_addr.sa_family, AF_INET)
                "    " /* TODO: Check contents of sockaddr_in */
                "    " lua_assert_equal(ai.ai_canonname, nil)
                "    " lua_assert_equal(ai.ai_next, nil)
                "    return 0;"
                "  end"
                ")";

        struct script_slave *ss = *state;
        int r;

        r = script_engine_run_string(ss->se, script, NULL, NULL);
        assert_return_code(r, -r);

        r = script_slave_socket_hook(ss, fd, &ai);
        assert_int_equal(r, 0);
}

static void t_pass_args_to_packet_hook(void **state)
{
        int fd = 1234;
        int flags = MSG_PEEK;
        struct sockaddr_in sin = {
                .sin_family = AF_INET,
                .sin_port = htons(1234),
                .sin_addr = { htonl(0x01020304) },
        };
        char buf[] = "Hello";
        struct iovec iov = {
                .iov_base = buf,
                .iov_len = ARRAY_SIZE(buf),
        };
        struct msghdr msg = {
                .msg_name = &sin,
                .msg_namelen = sizeof(sin),
                .msg_iov = &iov,
                .msg_iovlen = 1,
                .msg_control = NULL,
                .msg_controllen = 0,
                .msg_flags = 0,
        };
        const char *script =
                "client_recvmsg("
                "  function (fd, msg, flags)"
                "    " lua_assert_equal(fd, 1234)
                "    " lua_assert_not_equal(msg.msg_iov, nil)
                "    " lua_assert_equal(msg.msg_iovlen, 1)
                "    " /* TODO: Assert more values in msg */
                /* "    print('msg_name', msg.msg_name);" */
                /* "    print('msg_namelen', msg.msg_namelen);" */
                "    " lua_assert_equal(flags, MSG_PEEK)
                "    return 0;"
                "  end"
                ")";

        struct script_slave *ss = *state;
        int r;

        r = script_engine_run_string(ss->se, script, NULL, NULL);
        assert_return_code(r, -r);

        r = script_slave_recvmsg_hook(ss, fd, &msg, flags);
        assert_int_equal(r, 0);
}

static void t_run_hook_with_boolean_upvalue(void **state)
{
        const char *script =
                "local flag = true;"
                "client_socket("
                "  function ()"
                "    " lua_assert_true(flag)
                "    return 0;"
                "  end"
                ");";
        struct script_slave *ss = *state;
        struct script_engine *se = ss->se;
        int r;

        r = script_engine_run_string(se, script, NULL, NULL);
        assert_return_code(r, -r);

        r = script_slave_socket_hook(ss, -1, NULL);
        assert_return_code(r, -r);
}

static void t_run_hook_with_number_upvalue(void **state)
{
        const char *script =
                "local number = 42;"
                "client_socket("
                "  function ()"
                "    " lua_assert_equal(number, 42)
                "    return 0;"
                "  end"
                ");";
        struct script_slave *ss = *state;
        struct script_engine *se = ss->se;
        int r;

        r = script_engine_run_string(se, script, NULL, NULL);
        assert_return_code(r, -r);

        r = script_slave_socket_hook(ss, -1, NULL);
        assert_return_code(r, -r);
}

static void t_run_hook_with_string_upvalue(void **state)
{
        const char *script =
                "local string = 'foo';"
                "client_socket("
                "  function ()"
                "    " lua_assert_equal(string, 'foo')
                "    return 0;"
                "  end"
                ");";
        struct script_slave *ss = *state;
        struct script_engine *se = ss->se;
        int r;

        r = script_engine_run_string(se, script, NULL, NULL);
        assert_return_code(r, -r);

        r = script_slave_socket_hook(ss, -1, NULL);
        assert_return_code(r, -r);
}

static void t_run_hook_with_multiple_primitive_upvalues(void **state)
{
        const char *script =
                "local flag = true;"
                "local number = 42;"
                "local string = 'foo';"
                "client_socket("
                "  function ()"
                "    " lua_assert_true(flag)
                "    " lua_assert_equal(number, 42)
                "    " lua_assert_equal(string, 'foo')
                "    return 0;"
                "  end"
                ")";
        struct script_slave *ss = *state;
        struct script_engine *se = ss->se;
        int r;

        r = script_engine_run_string(se, script, NULL, NULL);
        assert_return_code(r, -r);

        r = script_slave_socket_hook(ss, -1, NULL);
        assert_return_code(r, -r);
}

static void t_run_hook_with_function_upvalue(void **state)
{
        const char *script =
                "local function func () return true; end;"
                "client_socket("
                "  function ()"
                "    " lua_assert_not_nil(func)
                "    " lua_assert_true(func())
                "    return 0;"
                "  end"
                ")";
        struct script_slave *ss = *state;
        struct script_engine *se = ss->se;
        int r;

        r = script_engine_run_string(se, script, NULL, NULL);
        assert_return_code(r, -r);

        r = script_slave_socket_hook(ss, -1, NULL);
        assert_return_code(r, -r);
}

static void t_run_hook_with_table_of_primitive_values_upvalue(void **state)
{
        const char *script =
                "local t = { 's', 42, false };"
                "client_socket("
                "  function ()"
                "    " lua_assert_not_nil(t)
                "    " lua_assert_equal(t[1], 's')
                "    " lua_assert_equal(t[2], 42)
                "    " lua_assert_false(t[3])
                "    return 0;"
                "  end"
                ")";
        struct script_slave *ss = *state;
        struct script_engine *se = ss->se;
        int r;

        r = script_engine_run_string(se, script, NULL, NULL);
        assert_return_code(r, -r);

        r = script_slave_socket_hook(ss, -1, NULL);
        assert_return_code(r, -r);
}

static void t_upvalues_dont_get_reset(void **state)
{
        const char *script =
                "local count = 0;"
                "client_socket("
                "  function ()"
                "    count = count + 1;"
                "    return count;"
                "  end"
                ")";
        struct script_slave *ss = *state;
        struct script_engine *se = ss->se;
        int r;

        r = script_engine_run_string(se, script, NULL, NULL);
        assert_return_code(r, -r);

        r = script_slave_socket_hook(ss, -1, NULL);
        assert_return_code(r, -r);
        assert_int_equal(r, 1);

        r = script_slave_socket_hook(ss, -1, NULL);
        assert_return_code(r, -r);
        assert_int_equal(r, 2);
}

static void t_hooks_share_basic_upvalues(void **state)
{
        const char *script =
                "local flag = false;"
                "local number = 0;"
                "local string = '';"
                "client_socket("
                "  function ()"
                "    flag = true;"
                "    number = 42;"
                "    string = 'fizz';"
                "    return 0;"
                "  end"
                ");"
                "client_close("
                "  function ()"
                "    " lua_assert_true(flag)
                "    " lua_assert_equal(number, 42)
                "    " lua_assert_equal(string, 'fizz')
                "    return 0;"
                "  end"
                ");";
        struct script_slave *ss = *state;
        struct script_engine *se = ss->se;
        int r;

        r = script_engine_run_string(se, script, NULL, NULL);
        assert_return_code(r, -r);

        r = script_slave_socket_hook(ss, -1, NULL);
        assert_return_code(r, -r);

        r = script_slave_close_hook(ss, -1, NULL);
        assert_return_code(r, -r);
}

static void t_hooks_share_table_upvalues(void **state)
{
        const char *script =
                "local t = { false, 0, '' };"
                "client_socket("
                "  function ()"
                "    t[1] = true;"
                "    t[2] = 42;"
                "    t[3] = 'fizz';"
                "    return 0;"
                "  end"
                ");"
                "client_close("
                "  function ()"
                "    " lua_assert_true(t[1])
                "    " lua_assert_equal(t[2], 42)
                "    " lua_assert_equal(t[3], 'fizz')
                "    return 0;"
                "  end"
                ");";
        struct script_slave *ss = *state;
        struct script_engine *se = ss->se;
        int r;

        r = script_engine_run_string(se, script, NULL, NULL);
        assert_return_code(r, -r);

        r = script_slave_socket_hook(ss, -1, NULL);
        assert_return_code(r, -r);

        r = script_slave_close_hook(ss, -1, NULL);
        assert_return_code(r, -r);
}

static void t_hook_can_access_table_referencing_table(void **state)
{
        const char *script =
                "local t1 = { 42 };"
                "local t2 = { t1 };"
                "client_socket("
                "  function ()"
                "    " lua_assert_equal(t2[1][1], 42)
                "    return 0;"
                "  end"
                ");";
        struct script_slave *ss = *state;
        struct script_engine *se = ss->se;
        int r;

        r = script_engine_run_string(se, script, NULL, NULL);
        assert_return_code(r, -r);

        r = script_slave_socket_hook(ss, -1, NULL);
        assert_return_code(r, -r);

}

static void t_hook_can_access_table_referencing_function(void **state)
{
        const char *script =
                "local function f() return 42 end;"
                "local t = { f };"
                "client_socket("
                "  function ()"
                "    " lua_assert_equal(t[1](), 42)
                "    return 0;"
                "  end"
                ");";
        struct script_slave *ss = *state;
        struct script_engine *se = ss->se;
        int r;

        r = script_engine_run_string(se, script, NULL, NULL);
        assert_return_code(r, -r);

        r = script_slave_socket_hook(ss, -1, NULL);
        assert_return_code(r, -r);
}

static void t_tables_that_are_not_upvalues_are_shared(void **state)
{
        /* t1 is an upvalue for client_close hook.
         * t2 is an upvalue for client_socket hook.
         * t2 references t1, so both hooks should see the same value. */
        const char *script =
                "local t1 = { 0 };"
                "local t2 = { t1 };"
                "client_socket("
                "  function ()"
                "    t2[1][1] = 42;"
                "    return 0;"
                "  end"
                ");"
                "client_close("
                "  function ()"
                "    " lua_assert_equal(t1[1], 42)
                "    return 0;"
                "  end"
                ");";
        struct script_slave *ss = *state;
        struct script_engine *se = ss->se;
        int r;

        r = script_engine_run_string(se, script, NULL, NULL);
        assert_return_code(r, -r);

        r = script_slave_socket_hook(ss, -1, NULL);
        assert_return_code(r, -r);

        r = script_slave_close_hook(ss, -1, NULL);
        assert_return_code(r, -r);
}

static void t_hook_can_access_function_with_basic_upvalues(void **state)
{
        const char *script =
                "local b = true;"
                "local n = 42;"
                "local s = 'fizz';"
                "local function f1() return b end;"
                "local function f2() return n end;"
                "local function f3() return s end;"
                "client_socket("
                "  function ()"
                "    " lua_assert_true(f1())
                "    " lua_assert_equal(f2(), 42)
                "    " lua_assert_equal(f3(), 'fizz')
                "    return 0;"
                "  end"
                ");";
        struct script_slave *ss = *state;
        struct script_engine *se = ss->se;
        int r;

        r = script_engine_run_string(se, script, NULL, NULL);
        assert_return_code(r, -r);

        r = script_slave_socket_hook(ss, -1, NULL);
        assert_return_code(r, -r);
}

static void t_hook_can_access_function_with_table_upvalue(void **state)
{
        const char *script =
                "local t = { 42 };"
                "local function f() return t[1] end;"
                "client_socket("
                "  function ()"
                "    " lua_assert_equal(f(), 42)
                "    return 0;"
                "  end"
                ");";
        struct script_slave *ss = *state;
        struct script_engine *se = ss->se;
        int r;

        r = script_engine_run_string(se, script, NULL, NULL);
        assert_return_code(r, -r);

        r = script_slave_socket_hook(ss, -1, NULL);
        assert_return_code(r, -r);
}

static void t_hook_can_access_function_with_function_upvalue(void **state)
{
        const char *script =
                "local function f() return 42 end;"
                "local function g() return f() end;"
                "client_socket("
                "  function ()"
                "    " lua_assert_equal(g(), 42)
                "    return 0;"
                "  end"
                ");";
        struct script_slave *ss = *state;
        struct script_engine *se = ss->se;
        int r;

        r = script_engine_run_string(se, script, NULL, NULL);
        assert_return_code(r, -r);

        r = script_slave_socket_hook(ss, -1, NULL);
        assert_return_code(r, -r);
}

static void t_collected_upvalues_get_unwrapped(void **state)
{
        const char *script =
                "local n = collect(42);"
                "client_socket("
                "  function ()"
                "    " lua_assert_equal(n, 42)
                "    return 0;"
                "  end"
                ");";
        struct script_slave *ss = *state;
        struct script_engine *se = ss->se;
        int r;

        r = script_engine_run_string(se, script, NULL, NULL);
        assert_return_code(r, -r);

        r = script_slave_socket_hook(ss, -1, NULL);
        assert_return_code(r, -r);
}

static void t_collected_table_elements_get_unwrapped(void **state)
{
        const char *script =
                "local t = { collect(42) };"
                "client_socket("
                "  function ()"
                "    " lua_assert_equal(t[1], 42)
                "    return 0;"
                "  end"
                ");";
        struct script_slave *ss = *state;
        struct script_engine *se = ss->se;
        int r;

        r = script_engine_run_string(se, script, NULL, NULL);
        assert_return_code(r, -r);

        r = script_slave_socket_hook(ss, -1, NULL);
        assert_return_code(r, -r);
}

static void dummy_run(struct script_engine *se, void *ss_)
{
        struct script_slave *ss = ss_;
        int r;

        script_engine_push_data(se, ss);

        r = script_slave_socket_hook(ss, -1, NULL);
        assert_return_code(r, -r);

        script_engine_pull_data(se, ss);
}


static void t_collect_upvalue(void **state)
{
        const char *script =
                "local b = collect(false);"
                "local n = collect(0);"
                "local s = collect('');"
                "local t = collect({ b2 = false, n2 = 0, s2 = '' });"
                "local t2 = collect({ { b3 = false }, { n3 = 0 }, { s3 = '' } });"
                "client_socket("
                "  function ()"
                "    b = true;"
                "    n = 42;"
                "    s = 'foo';"
                "    t.b2 = true;"
                "    t.n2 = 24;"
                "    t.s2 = 'bar';"
                "    t2[1].b3 = true;"
                "    t2[2].n3 = 84;"
                "    t2[3].s3 = 'baz';"
                "    return 0;"
                "  end"
                ");"
                "run();"
                "" lua_assert_equal(#b, 1)
                "" lua_assert_true(b[1])
                "" lua_assert_equal(#n, 1)
                "" lua_assert_equal(n[1], 42)
                "" lua_assert_equal(#s, 1)
                "" lua_assert_equal(s[1], 'foo')
                "" lua_assert_equal(#t, 1)
                "" lua_assert_true(t[1].b2)
                "" lua_assert_equal(t[1].n2, 24)
                "" lua_assert_equal(t[1].s2, 'bar')
                "" lua_assert_equal(#t2, 1)
                "" lua_assert_true(t2[1][1].b3)
                "" lua_assert_equal(t2[1][2].n3, 84)
                "" lua_assert_equal(t2[1][3].s3, 'baz')
                "";
        struct script_slave *ss = *state;
        struct script_engine *se = ss->se;
        int r;

        r = script_engine_run_string(se, script, dummy_run, ss);
        assert_return_code(r, -r);
}

static void t_collect_table_element(void **state)
{
        const char *script =
                "local t1 = { collect(0) };"
                "local t2 = { b = collect(false), s = collect('') };"
                "local t3 = { t = collect({}) };"
                "client_socket("
                "  function ()"
                "    t1[1] = 42;"
                "    t2.b = true;"
                "    t2.s = 'foo';"
                "    t3.t[1] = 24;"
                "    return 0;"
                "  end"
                ");"
                "run();"
                lua_assert_not_equal(#t1[1], nil) /* a value */
                lua_assert_equal(#t1[1], 1)       /* 1 entry table */
                lua_assert_equal(t1[1][1], 42)    /* 1st entry is 42 */

                lua_assert_not_equal(#t2.b, nil) /* a value */
                lua_assert_equal(#t2.b, 1)       /* 1 entry table */
                lua_assert_true(t2.b[1])         /* 1st entry is true */

                lua_assert_not_equal(#t2.s, nil) /* a value */
                lua_assert_equal(#t2.s, 1)       /* 1 entry table */
                lua_assert_equal(t2.s[1], 'foo') /* 1st entry is 'foo' */

                lua_assert_not_equal(#t3.t, nil)   /* a value */
                lua_assert_equal(#t3.t, 1)         /* 1 entry table */
                lua_assert_not_equal(t3.t[1], nil) /* 1st entry is a value */
                lua_assert_equal(#t3.t[1], 1)      /* 1st entry is 1 entry table */
                lua_assert_equal(t3.t[1][1], 24)   /* 1st entry of 1st entry is 24 */
                ;
        struct script_slave *ss = *state;
        struct script_engine *se = ss->se;
        int r;

        r = script_engine_run_string(se, script, dummy_run, ss);
        assert_return_code(r, -r);
}

#define clinet_engine_unit_test(f) \
        cmocka_unit_test_setup_teardown((f), client_engine_setup, client_engine_teardown)
#define client_slave_unit_test(f) \
        cmocka_unit_test_setup_teardown((f), client_slave_setup, client_slave_teardown)

int main(void)
{
        const struct CMUnitTest tests[] = {
                cmocka_unit_test(t_create_script_engine),
                cmocka_unit_test(t_create_script_slave),
                clinet_engine_unit_test(t_hooks_run_without_errors),
                clinet_engine_unit_test(t_wait_func_gets_called),
                clinet_engine_unit_test(t_run_cb_gets_invoked),
                client_slave_unit_test(t_run_socket_hook_from_string),
                client_slave_unit_test(t_run_socket_hook_from_file),
                client_slave_unit_test(t_run_close_hook),
                client_slave_unit_test(t_run_sendmsg_hook),
                client_slave_unit_test(t_run_recvmsg_hook),
                client_slave_unit_test(t_run_recverr_hook),
                client_slave_unit_test(t_pass_args_to_socket_hook),
                client_slave_unit_test(t_pass_args_to_packet_hook),
                client_slave_unit_test(t_run_hook_with_boolean_upvalue),
                client_slave_unit_test(t_run_hook_with_number_upvalue),
                client_slave_unit_test(t_run_hook_with_string_upvalue),
                client_slave_unit_test(t_run_hook_with_multiple_primitive_upvalues),
                client_slave_unit_test(t_run_hook_with_function_upvalue),
                client_slave_unit_test(t_run_hook_with_table_of_primitive_values_upvalue),
                client_slave_unit_test(t_upvalues_dont_get_reset),
                client_slave_unit_test(t_hooks_share_basic_upvalues),
                client_slave_unit_test(t_hooks_share_table_upvalues),
                client_slave_unit_test(t_hook_can_access_table_referencing_table),
                client_slave_unit_test(t_hook_can_access_table_referencing_function),
                client_slave_unit_test(t_tables_that_are_not_upvalues_are_shared),
                client_slave_unit_test(t_hook_can_access_function_with_basic_upvalues),
                client_slave_unit_test(t_hook_can_access_function_with_table_upvalue),
                client_slave_unit_test(t_hook_can_access_function_with_function_upvalue),
                client_slave_unit_test(t_collected_upvalues_get_unwrapped),
                client_slave_unit_test(t_collect_upvalue),
                client_slave_unit_test(t_collected_table_elements_get_unwrapped),
                client_slave_unit_test(t_collect_table_element),
        };

        return cmocka_run_group_tests(tests, common_setup, common_teardown);
}
