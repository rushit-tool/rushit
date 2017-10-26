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

#include "serialize.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "lib.h"
#include "logging.h"


static void l_object_free_data(struct l_object *o);
static void serialize_object(struct callbacks *cb, lua_State *L, int index,
                             struct l_object *object);


static void table_free(struct l_table_entry *table)
{
        struct l_table_entry *next;

        while (table) {
                next = table->next;

                l_object_free_data(&table->key);
                l_object_free_data(&table->value);
                free(table);

                table = next;
        }
}

static void l_object_free_data(struct l_object *o)
{
        if (!o)
                return;

        switch (o->type) {
        case LUA_TBOOLEAN:
        case LUA_TNUMBER:
                /* nothing to do */
                break;
        case LUA_TSTRING:
                free(o->string);
                break;
        case LUA_TFUNCTION:
                byte_array_free(o->function);
                break;
        case LUA_TTABLE:
                table_free(o->table);
                break;
        default:
                assert(false);
        }
}

static struct l_upvalue *l_upvalue_new(int index)
{
        struct l_upvalue *v;

        v = calloc(1, sizeof(*v));
        assert(v);

        v->index = index;

        return v;
}

void l_upvalue_free(struct l_upvalue *v)
{
        if (!v)
                return;

        l_object_free_data(&v->value);
        free(v);
}

static int string_writer(lua_State *L, const void *str, size_t len, void *buf)
{
        UNUSED(L);
        luaL_addlstring(buf, str, len);
        return 0;
}

struct byte_array *dump_function_bytecode(struct callbacks *cb, lua_State *L,
                                          int index)
{
        struct byte_array *code;
        const char *buf;
        size_t len = 0;
        luaL_Buffer B;
        int err;

        luaL_buffinit(L, &B);
        err = lua_dump(L, string_writer, &B);
        if (err)
                LOG_FATAL(cb, "lua_dump: %s", lua_tostring(L, -1));
        luaL_pushresult(&B);
        buf = lua_tolstring(L, index, &len);
        if (!buf || !len)
                LOG_FATAL(cb, "lua_dump returned an empty buffer");

        code = byte_array_new((uint8_t *) buf, len);
        lua_pop(L, 1);

        return code;
}

static struct l_table_entry *dump_table_entries(struct callbacks *cb,
                                                lua_State *L, int index)
{
        struct l_table_entry *head = NULL;
        int tbl_idx;

        tbl_idx = lua_gettop(L);
        lua_pushnil(L);
        while (lua_next(L, tbl_idx)) {
                struct l_table_entry *e = calloc(1, sizeof(*e));
                if (!e)
                        LOG_FATAL(cb, "calloc failed");
                e->next = head;
                head = e;

                serialize_object(cb, L, -2, &e->key);
                serialize_object(cb, L, -1, &e->value);
                lua_pop(L, 1);
        }

        return head;
}

static void serialize_object(struct callbacks *cb, lua_State *L, int index,
                             struct l_object *object)
{
        object->type = lua_type(L, index);

        switch (object->type) {
        case LUA_TNIL:
                assert(false);
                break;
        case LUA_TNUMBER:
                object->number = lua_tonumber(L, index);
                break;
        case LUA_TBOOLEAN:
                object->boolean = lua_toboolean(L, index);
                break;
        case LUA_TSTRING:
                object->string = strdup(lua_tostring(L, index));
                break;
        case LUA_TTABLE:
                object->table = dump_table_entries(cb, L, index);
                break;
        case LUA_TFUNCTION:
                object->function = dump_function_bytecode(cb, L, index);
                break;
        case LUA_TUSERDATA:
                assert(false); /* XXX: Not implemented */
                break;
        case LUA_TTHREAD:
                assert(false); /* XXX: Not implemented */
                break;
        case LUA_TLIGHTUSERDATA:
                assert(false); /* XXX: Not implemented */
                break;
        default:
                assert(false);
        }
}

struct l_upvalue *serialize_upvalue(struct callbacks *cb, lua_State *L,
                                    int index)
{
        struct l_upvalue *v;

        v = l_upvalue_new(index);
        serialize_object(cb, L, -1, &v->value);

        return v;
}
