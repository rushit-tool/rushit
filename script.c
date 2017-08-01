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


/*
 * Keys to Lua registry where we store hook functions and context.
 */

static void *SCRIPT_ENGINE_KEY = &SCRIPT_ENGINE_KEY;

static const char *hook_names[SCRIPT_HOOK_MAX] = {
        [SCRIPT_HOOK_INIT] = "init",
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

static int client_init_cb(lua_State *L)
{
        struct script_engine *se;
        const char *buf;
        size_t len = 0;
        luaL_Buffer B;
        int err;

        /* Expect a function argument */
        luaL_checktype(L, 1, LUA_TFUNCTION);

        /* Get context */
        lua_pushlightuserdata(L, SCRIPT_ENGINE_KEY);
        lua_gettable(L, LUA_REGISTRYINDEX);
        se = lua_touserdata(L, -1);
        lua_pop(L, 1);

        assert(se);

        /* Dump function bytecode */
        luaL_buffinit(L, &B);
        err = lua_dump(L, string_writer, &B);
        if (err)
                LOG_FATAL(se->cb, "lua_dump: %s", lua_tostring(L, -1));
        luaL_pushresult(&B);
        buf = lua_tolstring(L, -1, &len);
        if (!buf || !len)
                LOG_FATAL(se->cb, "lua_dump returned an empty buffer");

        script_engine_set_hook(se, SCRIPT_HOOK_INIT, buf, len);

        buf = NULL;
        len = 0;
        lua_pop(L, 1);

        /* TODO: Upvalues */
        /* TODO: Globals */

        return 0;
}

static int client_exit_cb(lua_State *L)
{
        return 0;
}

static int client_read_cb(lua_State *L)
{
        return 0;
}

static int client_write_cb(lua_State *L)
{
        return 0;
}

static int client_error_cb(lua_State *L)
{
        return 0;
}

static int server_init_cb(lua_State *L)
{
        return 0;
}

static int server_exit_cb(lua_State *L)
{
        return 0;
}

static int server_read_cb(lua_State *L)
{
        return 0;
}

static int server_write_cb(lua_State *L)
{
        return 0;
}

static int server_error_cb(lua_State *L)
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
        { "client_init",  client_init_cb },
        { "client_exit",  client_exit_cb },
        { "client_read",  client_read_cb },
        { "client_write", client_write_cb },
        { "client_error", client_error_cb },
        { "server_init",  server_init_cb },
        { "server_exit",  server_exit_cb },
        { "server_read",  server_read_cb },
        { "server_write", server_write_cb },
        { "server_error", server_error_cb },
        { "is_client",    is_client_cb },
        { "is_server",    is_server_cb },
        { "tid_iter",     tid_iter_cb },
        { NULL, NULL },
};


/**
 * Create an instance of a script engine
 */
int script_engine_create(struct script_engine **sep, struct callbacks *cb)
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

        assert(hook_idx == SCRIPT_HOOK_INIT);

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
        assert(hook_idx == SCRIPT_HOOK_INIT);

        h = &se->hooks[hook_idx];
        h->name = hook_names[hook_idx];
        if (h->bytecode)
                Lstring_free(h->bytecode);
        h->bytecode = Lstring_new(bytecode, bytecode_len);
}

int script_slave_run_init_hook(struct script_slave *ss, int sockfd, struct addrinfo *ai)
{
        CLEANUP(script_engine_put_hook) struct script_hook *h = NULL;
        int err, res;

        h = script_engine_get_hook(ss->se, SCRIPT_HOOK_INIT);
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
