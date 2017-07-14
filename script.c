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


static int null_cb(lua_State *L);


/*
 * Keys to Lua registry where we store hook functions and context.
 */

static void *SCRIPT_ENGINE_KEY = &SCRIPT_ENGINE_KEY;

static const struct luaL_Reg script_callbacks[] = {
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

static int null_cb(lua_State *L)
{
        return 0;
}
