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
#include "script.h"


static void l_object_free_data(struct l_object *o);
static void serialize_object(struct callbacks *cb, lua_State *L, int index,
                             struct l_object *object);
static void push_object(struct callbacks *cb, lua_State *L,
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

static struct l_upvalue *l_upvalue_new(int number)
{
        struct l_upvalue *v;

        v = calloc(1, sizeof(*v));
        assert(v);

        v->number = number;

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

struct byte_array *dump_function_bytecode(struct callbacks *cb, lua_State *L)
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
        buf = lua_tolstring(L, -1, &len);
        if (!buf || !len)
                LOG_FATAL(cb, "lua_dump returned an empty buffer");

        code = byte_array_new((uint8_t *) buf, len);
        lua_pop(L, 1);

        return code;
}

int load_function_bytecode(struct callbacks *cb, lua_State *L,
                           const struct byte_array *bytecode,
                           const char *name)
{
        int err;

        err = luaL_loadbuffer(L, (char *) bytecode->data, bytecode->len, name);
        if (err) {
                LOG_FATAL(cb, "%s: luaL_loadbuffer: %s",
                          name, lua_tostring(L, -1));
                return -errno_lua(err);
        }

        return 0;
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
                assert(index == -1); /* Not supported */
                object->function = dump_function_bytecode(cb, L);
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
                                    int number)
{
        struct l_upvalue *v;

        v = l_upvalue_new(number);
        serialize_object(cb, L, -1, &v->value);

        return v;
}

static void push_table(struct callbacks *cb, lua_State *L,
                       struct l_table_entry *table)
{
        struct l_table_entry *e;

        lua_newtable(L);
        for (e = table; e; e = e->next) {
                push_object(cb, L, &e->key);
                push_object(cb, L, &e->value);
                lua_rawset(L, -3);
        }
}

static void push_object(struct callbacks *cb, lua_State *L,
                        struct l_object *object)
{
        switch (object->type) {
        case LUA_TBOOLEAN:
                lua_pushboolean(L, object->boolean);
                break;
        case LUA_TNUMBER:
                lua_pushnumber(L, object->number);
                break;
        case LUA_TSTRING:
                lua_pushstring(L, object->string);
                break;
        case LUA_TFUNCTION:
                load_function_bytecode(cb, L, object->function, NULL);
                break;
        case LUA_TTABLE:
                push_table(cb, L, object->table);
                break;
        default:
                assert(false);
                break;
        }
}

void set_upvalue(struct callbacks *cb, lua_State *L, int func_index,
                 struct l_upvalue *upvalue)
{
        const char *name;

        push_object(cb, L, &upvalue->value);
        name = lua_setupvalue(L, func_index, upvalue->number);
        assert(name);
}

void destroy_upvalues(struct l_upvalue **head)
{
        struct l_upvalue *v, *v_next;

        assert(head);

        v = *head;
        *head = NULL;

        while (v) {
                v_next = v->next;
                l_upvalue_free(v);
                v = v_next;
        }
}

void prepend_upvalue(struct l_upvalue **head, struct l_upvalue *upvalue)
{
        assert(head);
        assert(upvalue);

        upvalue->next = *head;
        *head = upvalue;
}
