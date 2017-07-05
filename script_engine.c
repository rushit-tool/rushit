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

#include "script_engine.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"


/**
 * Script API callback from Lua to C
 */
struct sapi_callback {
        const char *name;
        lua_CFunction func;
};


static int null_cb(lua_State *L);


/*
 * Keys to Lua registry where we store hook functions and context.
 */

static void *SCRIPT_ENGINE_KEY = &SCRIPT_ENGINE_KEY;

static const struct sapi_callback script_callbacks[] = {
        { "client_error", null_cb },
        { "client_exit",  null_cb },
        { "client_init",  null_cb },
        { "client_read",  null_cb },
        { "client_write", null_cb },
        { "is_client",    null_cb },
        { "is_server",    null_cb },
        { "server_error", null_cb },
        { "server_exit",  null_cb },
        { "server_init",  null_cb },
        { "server_read",  null_cb },
        { "server_write", null_cb },
        { "tid_iter",     null_cb },
        { NULL, NULL },
};


/**
 * Initialize script engine state
 */
int se_create(struct script_engine *se)
{
        const struct sapi_callback *cb;
        lua_State *L;

        assert(se);

        /* Init new Lua state */
        L = luaL_newstate();
        if (!L)
                return -ENOMEM;
        luaL_openlibs(L);

        /* Store context for C callbacks */
        lua_pushlightuserdata(L, SCRIPT_ENGINE_KEY);
        lua_pushlightuserdata(L, se);
        lua_settable(L, LUA_REGISTRYINDEX);

        /* Register script API callbacks */
        for (cb = script_callbacks; cb->name; cb++)
                lua_register(L, cb->name, cb->func);

        memset(se, 0, sizeof(*se));
        se->L = L;

        return 0;
}

/**
 * Destroy script engine state
 */
void se_destroy(struct script_engine *se)
{
        assert(se);

        lua_close(se->L);
}

static int null_cb(lua_State *L)
{
        return 0;
}
