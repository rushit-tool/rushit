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

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"


enum run_mode { CLIENT, SERVER };

/*
 * Keys to Lua registry where we store hook functions and context.
 */
static void *SCRIPT_ENGINE_KEY = &SCRIPT_ENGINE_KEY;

static const char *hook_names[SCRIPT_HOOK_MAX] = {
        [SCRIPT_HOOK_SOCKET] = "socket_hook",
        [SCRIPT_HOOK_CLOSE] = "close_hook",
        [SCRIPT_HOOK_SENDMSG] = "sendmsg_hook",
        [SCRIPT_HOOK_RECVMSG] = "recvmsg_hook",
        [SCRIPT_HOOK_RECVERR] = "recverr_hook",
};

static void script_engine_set_hook(struct script_engine *se, int hook_idx,
                                   const char *bytecode, size_t length);

struct Lstring {
        char *data;
        size_t len;
};

static struct Lstring *Lstring_new(const char *data, size_t len)
{
        struct Lstring *s;

        assert(data);

        s = calloc(1, sizeof(*s) + len + 1);
        assert(s);
        s->data = (void *) (s + 1);
        s->len = len;
        memcpy(s->data, data, len);

        return s;
}

static void Lstring_free(struct Lstring *s)
{
        free(s);
}

/*
 * Lua to C callbacks
 */

static int string_writer(lua_State *L, const void *str, size_t len, void *buf)
{
        UNUSED(L);
        luaL_addlstring(buf, str, len);
        return 0;
}

static int store_hook_bytecode(struct script_engine *se, int hook_idx)
{
        lua_State *L = se->L;
        const char *buf;
        size_t len = 0;
        luaL_Buffer B;
        int err;

        /* Dump function bytecode */
        luaL_buffinit(L, &B);
        err = lua_dump(L, string_writer, &B);
        if (err)
                LOG_FATAL(se->cb, "lua_dump: %s", lua_tostring(L, -1));
        luaL_pushresult(&B);
        buf = lua_tolstring(L, -1, &len);
        if (!buf || !len)
                LOG_FATAL(se->cb, "lua_dump returned an empty buffer");

        script_engine_set_hook(se, hook_idx, buf, len);

        buf = NULL;
        len = 0;
        lua_pop(L, 1);

        /* TODO: Upvalues */
        /* TODO: Globals */

        return 0;
}

static int store_hook(lua_State *L, enum run_mode run_mode, int hook_idx)
{
        struct script_engine *se;
        int rc = 0;

        /* Expect a function argument */
        luaL_checktype(L, 1, LUA_TFUNCTION);

        /* Get context */
        lua_pushlightuserdata(L, SCRIPT_ENGINE_KEY);
        lua_gettable(L, LUA_REGISTRYINDEX);
        se = lua_touserdata(L, -1);
        lua_pop(L, 1);

        assert(se);

        if (se->run_mode == run_mode)
               rc = store_hook_bytecode(se, hook_idx);

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
        return 0;
}

static int server_close_cb(lua_State *L)
{
        return 0;
}

static int server_sendmsg_cb(lua_State *L)
{
        return 0;
}

static int server_recvmsg_cb(lua_State *L)
{
        return 0;
}

static int server_recverr_cb(lua_State *L)
{
        return 0;
}

static int is_client_cb(lua_State *L)
{
        return 0;
}

static int is_server_cb(lua_State *L)
{
        return 0;
}

static int tid_iter_cb(lua_State *L)
{
        return 0;
}

static const struct luaL_Reg script_callbacks[] = {
        { "client_socket",  client_socket_cb },
        { "client_close",  client_close_cb },
        { "client_sendmsg", client_sendmsg_cb },
        { "client_recvmsg", client_recvmsg_cb },
        { "client_recverr", client_recverr_cb },
        { "server_socket",  server_socket_cb },
        { "server_close",  server_close_cb },
        { "server_sendmsg", server_sendmsg_cb },
        { "server_recvmsg", server_recvmsg_cb },
        { "server_recverr", server_recverr_cb },
        { "is_client",    is_client_cb },
        { "is_server",    is_server_cb },
        { "tid_iter",     tid_iter_cb },
        { NULL, NULL },
};


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
        for (f = script_callbacks; f->name; f++)
                lua_register(L, f->name, f->func);

        /* Set context for Lua to C callbacks */
        lua_pushlightuserdata(L, SCRIPT_ENGINE_KEY);
        lua_pushlightuserdata(L, se);
        lua_settable(L, LUA_REGISTRYINDEX);

        se->L = L;
        se->cb = cb;
        se->run_mode = is_client ? CLIENT : SERVER;

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

        for (h = se->hooks; h < se->hooks + SCRIPT_HOOK_MAX; h++)
                Lstring_free(h->bytecode);

        free(se);
        return NULL;
}

static int run_script(struct script_engine *se,
                      int (*load_func)(lua_State *, const char *), const char *input,
                      void (*wait_func)(void *data), void *wait_data)
{
        int err;

        se->wait_func = wait_func;
        se->wait_data = wait_data;

        err = (*load_func)(se->L, input);
        if (err) {
                LOG_ERROR(se->cb, "luaL_load...: %s", lua_tostring(se->L, -1));
                return -err; /* TODO: remap Lua error codes? */
        }
        err = lua_pcall(se->L, 0, LUA_MULTRET, 0);
        if (err) {
                LOG_ERROR(se->cb, "lua_pcall: %s", lua_tostring(se->L, -1));
                return -err; /* TODO: remap Lua error codes? */
        }

        if (wait_func)
                (*wait_func)(wait_data);

        /* TODO: Propagate return value. */
        return 0;
}


/**
 * Runs the script passed in a string.
 */
int script_engine_run_string(struct script_engine *se, const char *script,
                             void (*wait_func)(void *), void *wait_data)
{
        assert(se);
        assert(script);

        return run_script(se, luaL_loadstring, script, wait_func, wait_data);
}

/**
 * Runs the script from a given file.
 */
int script_engine_run_file(struct script_engine *se, const char *filename,
                            void (*wait_func)(void *), void *wait_data)
{
        assert(se);
        assert(filename);

        return run_script(se, luaL_loadfile, filename, wait_func, wait_data);
}

/**
 * Create an instance of a slave script engine
 */
int script_slave_create(struct script_slave **ssp, struct script_engine *se)
{
        CLEANUP(free) struct script_slave *ss = NULL;
        lua_State *L;

        assert(ssp);
        assert(se);

        ss = calloc(1, sizeof(*ss));
        if (!ss)
                return -ENOMEM;

        L = luaL_newstate();
        if (!L)
                return -ENOMEM;
        luaL_openlibs(L);

        /* TODO: Load prelude */

        /* TODO: Install hooks */

        ss->se = se;
        ss->cb = se->cb;
        ss->L = L;

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

static struct script_hook *script_engine_get_hook(struct script_engine *se,
                                                  int hook_idx)
{
        struct script_hook *h;

        assert(0 <= hook_idx && hook_idx < SCRIPT_HOOK_MAX);

        h = &se->hooks[hook_idx];
        return h->bytecode ? h : NULL;
}

static struct script_hook *script_engine_put_hook(struct script_hook *hook)
{
        return NULL;
}
DEFINE_CLEANUP_FUNC(script_engine_put_hook, struct script_hook *);

static void script_engine_set_hook(struct script_engine *se, int hook_idx,
                                   const char *bytecode, size_t bytecode_len)
{
        struct script_hook *h;

        assert(se);
        assert(0 <= hook_idx && hook_idx < SCRIPT_HOOK_MAX);

        h = &se->hooks[hook_idx];
        h->name = hook_names[hook_idx];
        if (h->bytecode)
                Lstring_free(h->bytecode);
        h->bytecode = Lstring_new(bytecode, bytecode_len);
}

static int run_hook(struct script_slave *ss, int hook_idx)
{
        CLEANUP(script_engine_put_hook) struct script_hook *h = NULL;
        int err, res;

        h = script_engine_get_hook(ss->se, hook_idx);
        if (!h)
                return 0;

        err = luaL_loadbuffer(ss->L, h->bytecode->data, h->bytecode->len, h->name);
        if (err) {
                LOG_ERROR(ss->cb, "luaL_loadbuffer: %s", lua_tostring(ss->L, -1));
                return -err;
        }
        /* TODO: Push upvalues */
        /* TODO: Push globals */

        /* TODO: Push arguments */
        err = lua_pcall(ss->L, 0, 1, 0);
        if (err) {
                LOG_ERROR(ss->cb, "lua_pcall: %s", lua_tostring(ss->L, -1));
                return -err;
        }

        if (lua_isnil(ss->L, -1))
                return 0;

        res = luaL_checkint(ss->L, -1);
        lua_pop(ss->L, 1);
        return res;
}

static int run_socket_hook(struct script_slave *ss, int hook_idx,
                           int sockfd, struct addrinfo *ai)
{
        /* TODO: Pass arguments for the hook */
        return run_hook(ss, hook_idx);
}

static int run_packet_hook(struct script_slave *ss, int hook_idx,
                           int sockfd, struct msghdr *msg, int flags)
{
        /* TODO: Pass arguments for the hook */
        return run_hook(ss, hook_idx);
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
