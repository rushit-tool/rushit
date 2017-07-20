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


/*
 * Lua to C callbacks
 */

static int client_init_cb(lua_State *L)
{
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
        _auto_free_ struct script_engine *se = NULL;
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
        assert(se);

        lua_close(se->L);
        se->L = NULL;

        free(se);
        return NULL;
}

int script_engine_run_string(struct script_engine *se, const char *script)
{
        int err;

        assert(se);

        err = luaL_dostring(se->L, script);
        if (err) {
                LOG_ERROR(se->cb, "luaL_dostring: %s", lua_tostring(se->L, -1));
                return -err; /* TODO: remap Lua error codes? */
        }

        return 0;
}

/**
 * Create an instance of a slave script engine
 */
int script_slave_create(struct script_slave **ssp, struct script_engine *se)
{
        _auto_free_ struct script_slave *ss = NULL;
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

void script_slave_init(struct script_slave *ss, int sockfd, struct addrinfo *ai)
{
        /* TODO: Get init hook from master and run it */
}
