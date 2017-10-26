/*
 * Copyright 2017 Red Hat, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "script.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "common.h"
#include "serialize.h"

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"


enum run_mode { CLIENT, SERVER };

/*
 * Keys to Lua registry where we store hook functions and context.
 */
static void *SCRIPT_ENGINE_KEY = &SCRIPT_ENGINE_KEY;

DEFINE_CLEANUP_FUNC(lua_close, lua_State *);

enum script_hook_error errno_lua(int err)
{
        assert(err == LUA_ERRRUN || err == LUA_ERRSYNTAX ||
               err == LUA_ERRMEM || err == LUA_ERRERR);
        return SCRIPT_HOOK_ERROR_BASE + err;
}

/*
 * Lua to C callbacks
 */

static struct script_hook *script_engine_get_hook(struct script_engine *se,
                                                  enum script_hook_id hid)
{
        assert(0 <= hid && hid < SCRIPT_HOOK_MAX);

        return &se->hooks[hid];
}

static struct script_hook *script_engine_put_hook(struct script_hook *hook)
{
        return NULL;
}
DEFINE_CLEANUP_FUNC(script_engine_put_hook, struct script_hook *);

static void hook_set_bytecode(struct script_hook *h, struct byte_array *bytecode)
{
        assert(h);

        byte_array_free(h->bytecode);
        h->bytecode = bytecode;
}

static int store_hook_bytecode(struct callbacks *cb, lua_State *L,
                               struct script_hook *hook)
{
        struct byte_array *code;

        code = dump_function_bytecode(cb, L, -1);
        hook_set_bytecode(hook, code);

        return 0;
}

static struct script_engine *get_context(lua_State *L)
{
        struct script_engine *se;

        lua_pushlightuserdata(L, SCRIPT_ENGINE_KEY);
        lua_gettable(L, LUA_REGISTRYINDEX);
        se = lua_touserdata(L, -1);
        assert(se);
        lua_pop(L, 1);

        return se;
}

static void hook_set_upvalue(struct script_hook *hook,
                             struct l_upvalue *upvalue)
{
        assert(hook);
        assert(upvalue);

        upvalue->next = hook->upvalues;
        hook->upvalues = upvalue;
}

static void hook_unset_upvalues(struct script_hook *hook)
{
        struct l_upvalue *v, *v_next;

        v = hook->upvalues;
        while (v) {
                v_next = v->next;
                l_upvalue_free(v);
                v = v_next;
        }

        hook->upvalues = NULL;
}

static void store_hook_upvalues(struct callbacks *cb, struct lua_State *L,
                                struct script_hook *hook)
{
        struct l_upvalue *upval;
        const char *name;
        int i, top;

        assert(hook);

        hook_unset_upvalues(hook);

        top = lua_gettop(L);
        for (i = 1; (name = lua_getupvalue(L, top, i)); i++) {
                upval = serialize_upvalue(cb, L, i);
                hook_set_upvalue(hook, upval);
                lua_pop(L, 1);
        }
}

static int store_hook(lua_State *L, enum run_mode run_mode,
                      enum script_hook_id hid)
{
        CLEANUP(script_engine_put_hook) struct script_hook *h = NULL;
        struct script_engine *se;
        int rc = 0;

        /* Expect a function argument */
        luaL_checktype(L, 1, LUA_TFUNCTION);

        se = get_context(L);
        if (se->run_mode == run_mode) {
                h = script_engine_get_hook(se, hid);
                store_hook_upvalues(se->cb, L, h);
                rc = store_hook_bytecode(se->cb, L, h);
        }

        return rc;
}

static int client_socket_cb(lua_State *L)
{
        return store_hook(L, CLIENT, SCRIPT_HOOK_SOCKET);
}

static int client_close_cb(lua_State *L)
{
        return store_hook(L, CLIENT, SCRIPT_HOOK_CLOSE);
}

static int client_sendmsg_cb(lua_State *L)
{
        return store_hook(L, CLIENT, SCRIPT_HOOK_SENDMSG);
}

static int client_recvmsg_cb(lua_State *L)
{
        return store_hook(L, CLIENT, SCRIPT_HOOK_RECVMSG);
}

static int client_recverr_cb(lua_State *L)
{
        return store_hook(L, CLIENT, SCRIPT_HOOK_RECVERR);
}

static int server_socket_cb(lua_State *L)
{
        return store_hook(L, SERVER, SCRIPT_HOOK_SOCKET);
}

static int server_close_cb(lua_State *L)
{
        return store_hook(L, SERVER, SCRIPT_HOOK_CLOSE);
}

static int server_sendmsg_cb(lua_State *L)
{
        return store_hook(L, SERVER, SCRIPT_HOOK_SENDMSG);
}

static int server_recvmsg_cb(lua_State *L)
{
        return store_hook(L, SERVER, SCRIPT_HOOK_RECVMSG);
}

static int server_recverr_cb(lua_State *L)
{
        return store_hook(L, SERVER, SCRIPT_HOOK_RECVERR);
}

static int is_client_cb(lua_State *L)
{
        return 0;
}

static int is_server_cb(lua_State *L)
{
        return 0;
}

static int run_cb(lua_State *L)
{
        struct script_engine *se;

        se = get_context(L);
        if (se->run_func) {
                (*se->run_func)(se->run_data);
                se->run_func = NULL; /* runs only once */
        }

        return 0;
}

static int tid_iter_cb(lua_State *L)
{
        return 0;
}

static const struct luaL_Reg client_callbacks[] = {
        [SCRIPT_HOOK_SOCKET] =  { "client_socket",  client_socket_cb },
        [SCRIPT_HOOK_CLOSE] =   { "client_close",   client_close_cb },
        [SCRIPT_HOOK_SENDMSG] = { "client_sendmsg", client_sendmsg_cb },
        [SCRIPT_HOOK_RECVMSG] = { "client_recvmsg", client_recvmsg_cb },
        [SCRIPT_HOOK_RECVERR] = { "client_recverr", client_recverr_cb },
        { NULL, NULL },
};

static const struct luaL_Reg server_callbacks[] = {
        [SCRIPT_HOOK_SOCKET] =  { "server_socket",  server_socket_cb },
        [SCRIPT_HOOK_CLOSE] =   { "server_close",   server_close_cb },
        [SCRIPT_HOOK_SENDMSG] = { "server_sendmsg", server_sendmsg_cb },
        [SCRIPT_HOOK_RECVMSG] = { "server_recvmsg", server_recvmsg_cb },
        [SCRIPT_HOOK_RECVERR] = { "server_recverr", server_recverr_cb },
        { NULL, NULL },
};

static const struct luaL_Reg common_callbacks[] = {
        { "is_client", is_client_cb },
        { "is_server", is_server_cb },
        { "run",       run_cb },
        { "tid_iter",  tid_iter_cb },
        { NULL, NULL },
};

static const char *get_hook_name(enum run_mode mode, enum script_hook_id hid)
{
        static const struct luaL_Reg *hook_names[2] = {
                [CLIENT] = client_callbacks,
                [SERVER] = server_callbacks,
        };

        return hook_names[mode][hid].name;
}

static void init_hook_names(struct script_engine *se)
{
        int hid;

        for (hid = 0; hid < SCRIPT_HOOK_MAX; hid++)
                se->hooks[hid].name = get_hook_name(se->run_mode, hid);
}

/**
 * Create an instance of a script engine
 */
int script_engine_create(struct script_engine **sep, struct callbacks *cb,
                         bool is_client)
{
        CLEANUP(free) struct script_engine *se = NULL;
        const struct luaL_Reg *f;
        lua_State *L;

        assert(sep);

        se = calloc(1, sizeof(*se));
        if (!se)
                return -ENOMEM;

        /* Init new Lua state */
        L = luaL_newstate();
        if (!L)
                return -ENOMEM;
        luaL_openlibs(L);
        /* Register Lua to C callbacks (script API)
         * TODO: Switch to a single call to luaL_register()? */
        for (f = client_callbacks; f->name; f++)
                lua_register(L, f->name, f->func);
        for (f = server_callbacks; f->name; f++)
                lua_register(L, f->name, f->func);
        for (f = common_callbacks; f->name; f++)
                lua_register(L, f->name, f->func);

        /* Set context for Lua to C callbacks */
        lua_pushlightuserdata(L, SCRIPT_ENGINE_KEY);
        lua_pushlightuserdata(L, se);
        lua_settable(L, LUA_REGISTRYINDEX);

        se->L = L;
        se->cb = cb;
        se->run_mode = is_client ? CLIENT : SERVER;

        init_hook_names(se);

        *sep = se;
        se = NULL;

        return 0;
}

/**
 * Destroy a script engine instance
 */
struct script_engine *script_engine_destroy(struct script_engine *se)
{
        struct script_hook *h;

        assert(se);

        lua_close(se->L);
        se->L = NULL;

        for (h = se->hooks; h < se->hooks + SCRIPT_HOOK_MAX; h++) {
                byte_array_free(h->bytecode);
                hook_unset_upvalues(h);
        }

        free(se);
        return NULL;
}

static int run_script(struct script_engine *se,
                      int (*load_func)(lua_State *, const char *), const char *input,
                      void (*run_func)(void *data), void *run_data)
{
        int err;

        se->run_func = run_func;
        se->run_data = run_data;

        err = (*load_func)(se->L, input);
        if (err) {
                LOG_ERROR(se->cb, "luaL_load...: %s", lua_tostring(se->L, -1));
                return -errno_lua(err);
        }
        err = lua_pcall(se->L, 0, LUA_MULTRET, 0);
        if (err) {
                LOG_ERROR(se->cb, "lua_pcall: %s", lua_tostring(se->L, -1));
                return -errno_lua(err);
        }

        /* If run() hasn't been called from the script, do it now */
        run_cb(se->L);

        /* TODO: Propagate return value. */
        return 0;
}


/**
 * Runs the script passed in a string.
 */
int script_engine_run_string(struct script_engine *se, const char *script,
                             void (*run_func)(void *), void *run_data)
{
        assert(se);
        assert(script);

        return run_script(se, luaL_loadstring, script, run_func, run_data);
}

/**
 * Runs the script from a given file.
 */
int script_engine_run_file(struct script_engine *se, const char *filename,
                            void (*run_func)(void *), void *run_data)
{
        assert(se);
        assert(filename);

        return run_script(se, luaL_loadfile, filename, run_func, run_data);
}

static int load_prelude(struct callbacks *cb, lua_State *L)
{
        int err;

        lua_getglobal(L, "require");
        lua_pushliteral(L, "script_prelude");
        err = lua_pcall(L, 1, 0, 0);
        if (err) {
                LOG_ERROR(cb, "require('script_prelude'): %s",
                          lua_tostring(L, -1));
                return -errno_lua(err);
        }

        return 0;
}

/**
 * Create an instance of a slave script engine
 */
int script_slave_create(struct script_slave **ssp, struct script_engine *se)
{
        CLEANUP(free) struct script_slave *ss = NULL;
        CLEANUP(lua_close) lua_State *L = NULL;
        int err;

        assert(ssp);
        assert(se);

        ss = calloc(1, sizeof(*ss));
        if (!ss)
                return -ENOMEM;

        L = luaL_newstate();
        if (!L)
                return -ENOMEM;
        luaL_openlibs(L);
        err = load_prelude(se->cb, L);
        if (err)
                return err;

        /* TODO: Install hooks */

        ss->se = se;
        ss->cb = se->cb;
        ss->L = L;
        L = NULL;

        *ssp = ss;
        ss = NULL;
        return 0;
}


/**
 * Destroy a slave script engine instance
 */
struct script_slave *script_slave_destroy(struct script_slave *ss)
{
        assert(ss);

        lua_close(ss->L);
        ss->L = NULL;
        ss->se = NULL;

        free(ss);
        return NULL;
}

/**
 * Equivalent of:
 *
 * function (proto, ptr)
 *   return ffi.cast(ffi.typeof(proto), ptr)
 * end
 *
 * TODO: Move to script_prelude.lua?
 */
static int push_cpointer(struct callbacks *cb, lua_State *L, const char *proto, void *ptr)
{
        int err;

        /* Get ffi */
        lua_getglobal(L, "require");
        lua_pushliteral(L, "ffi");
        err = lua_pcall(L, 1, 1, 0);
        if (err) {
                LOG_ERROR(cb, "lua_pcall(require 'ffi'): %s", lua_tostring(L, -1));
                return -errno_lua(err);
        }

        lua_getfield(L, -1, "cast");
        lua_getfield(L, -2, "typeof");

        /* Call ffi.typeof */
        lua_pushstring(L, proto);
        err = lua_pcall(L, 1, 1, 0);
        if (err) {
                lua_pop(L, 3);
                LOG_ERROR(cb, "lua_pcall(ffi.typeof): %s", lua_tostring(L, -1));
                return -errno_lua(err);
        }

        /* Call ffi.cast*/
        lua_pushlightuserdata(L, ptr);
        err = lua_pcall(L, 2, 1, 0);
        if (err) {
                lua_pop(L, 2);
                LOG_ERROR(cb, "lua_pcall(ffi.cast): %s", lua_tostring(L, -1));
                return -errno_lua(err);
        }

        /* Remove ffi module */
        lua_remove(L, -2);
        return  0;
}

/* Load a serialized hook function. Return a key to it in the registry. */
static int load_hook(struct callbacks *cb, lua_State *L,
                     const struct script_hook *hook, void **key)
{
        struct l_upvalue *v;
        int err, hook_idx;
        void *k;

        if (!hook->bytecode)
                return -EHOOKEMPTY;

        err = load_function_bytecode(cb, L, hook->bytecode, hook->name);
        if (err)
                return err;
        hook_idx = lua_gettop(L);

        for (v = hook->upvalues; v; v = v->next)
                push_upvalue(cb, L, hook_idx, v);

        /* TODO: Push globals */

        /* Keep a reference to the hook */
        k = (void *) lua_topointer(L, -1);
        lua_pushlightuserdata(L, k);
        lua_insert(L, -2);
        lua_rawset(L, LUA_REGISTRYINDEX);

        *key = k;
        return 0;
}

static int push_hook(struct script_slave *ss, enum script_hook_id hid)
{
        CLEANUP(script_engine_put_hook) struct script_hook *h = NULL;
        lua_State *L;
        int err;

        assert(ss);

        h = script_engine_get_hook(ss->se, hid);
        L = ss->L;

        if (!ss->hook_key[hid]) {
                err = load_hook(ss->cb, L, h, &ss->hook_key[hid]);
                if (err)
                        return err;
        }

        lua_pushlightuserdata(L, ss->hook_key[hid]);
        lua_gettable(L, LUA_REGISTRYINDEX);

        return 0;
}

static int call_hook(struct script_slave *ss, enum script_hook_id hid, int nargs)
{
        int err, res;

        err = lua_pcall(ss->L, nargs, 1, 0);
        if (err) {
                LOG_FATAL(ss->cb, "%s: lua_pcall: %s",
                          get_hook_name(ss->se->run_mode, hid),
                          lua_tostring(ss->L, -1));
                return -errno_lua(err);
        }

        if (lua_isnumber(ss->L, -1))
                res = lua_tointeger(ss->L, -1);
        else
                res = -EHOOKRETVAL;
        lua_pop(ss->L, 1);

        return res;
}

static int run_socket_hook(struct script_slave *ss, enum script_hook_id hid,
                           int sockfd, struct addrinfo *ai)
{
        int err;

        err = push_hook(ss, hid);
        if (err)
                return err;

        /* Push arguments */
        lua_pushinteger(ss->L, sockfd);
        push_cpointer(ss->cb, ss->L, "struct addrinfo *", ai);

        return call_hook(ss, hid, 2);
}

static int run_packet_hook(struct script_slave *ss, enum script_hook_id hid,
                           int sockfd, struct msghdr *msg, int flags)
{
        int err;

        err = push_hook(ss, hid);
        if (err)
                return err;

        /* Push arguments */
        lua_pushinteger(ss->L, sockfd);
        push_cpointer(ss->cb, ss->L, "struct msghdr *", msg);
        lua_pushinteger(ss->L, flags);

        return call_hook(ss, hid, 3);
}

int script_slave_socket_hook(struct script_slave *ss, int sockfd,
                             struct addrinfo *ai)
{
        return run_socket_hook(ss, SCRIPT_HOOK_SOCKET, sockfd, ai);
}

int script_slave_close_hook(struct script_slave *ss, int sockfd,
                            struct addrinfo *ai)
{
        return run_socket_hook(ss, SCRIPT_HOOK_CLOSE, sockfd, ai);
}

ssize_t script_slave_sendmsg_hook(struct script_slave *ss, int sockfd,
                                  struct msghdr *msg, int flags)
{
        return run_packet_hook(ss, SCRIPT_HOOK_SENDMSG, sockfd, msg, flags);
}

ssize_t script_slave_recvmsg_hook(struct script_slave *ss, int sockfd,
                                  struct msghdr *msg, int flags)
{
        return run_packet_hook(ss, SCRIPT_HOOK_RECVMSG, sockfd, msg, flags);
}

ssize_t script_slave_recverr_hook(struct script_slave *ss, int sockfd,
                                  struct msghdr *msg, int flags)
{
        return run_packet_hook(ss, SCRIPT_HOOK_RECVERR, sockfd, msg, flags);
}

const char *script_strerror(int errnum)
{
        switch (errnum) {
        case EHOOKEMPTY:
                return "No hook to invoke";
        case EHOOKRETVAL:
                return "No return value from hook";
        case EHOOKRUN:
                return "Hook runtime error";
        case EHOOKSYNTAX:
                return "Hook syntax error";
        case EHOOKMEM:
                return "Hook memory allocation error";
        case EHOOKERR:
                return "Hook error handler error";
        default:
                return "Unkown script error";
        };
}
