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


enum {
        XLUA_TUPVALUEREF = -1,
};


struct l_table_entry {
        struct l_table_entry *next;
        struct l_object key;
        struct l_object value;
};

struct l_table {
        void *id;
        struct l_table_entry *entries;
};

struct upvalue_cache {
        struct l_upvalue *head;
};


static void l_object_free_data(struct l_object *o);
static void serialize_object(struct callbacks *cb, lua_State *L,
                             struct l_object *object);
static void push_object(struct callbacks *cb, lua_State *L,
                        const struct l_object *object);


static void table_entry_free(struct l_table_entry *e)
{
        l_object_free_data(&e->key);
        l_object_free_data(&e->value);
        free(e);
}

static void table_free_entries(struct l_table_entry *e)
{
        struct l_table_entry *e_next;

        while (e) {
                e_next = e->next;
                table_entry_free(e);
                e = e_next;
        }
}

static void table_free(struct l_table *t)
{
        if (t) {
                table_free_entries(t->entries);
                free(t);
        }
}

static void l_object_free_data(struct l_object *o)
{
        if (!o)
                return;

        switch (o->type) {
        case LUA_TBOOLEAN:
        case LUA_TNUMBER:
        case XLUA_TUPVALUEREF:
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

static struct l_upvalue *l_upvalue_new(void *id, int number)
{
        struct l_upvalue *v;

        v = calloc(1, sizeof(*v));
        assert(v);

        v->id = id;
        v->number = number;

        return v;
}

static void l_upvalue_free(struct l_upvalue *v)
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

        lua_pushnil(L);
        while (lua_next(L, index)) {
                struct l_table_entry *e = calloc(1, sizeof(*e));
                if (!e)
                        LOG_FATAL(cb, "calloc failed");
                e->next = head;
                head = e;

                serialize_object(cb, L, &e->value);
                lua_pop(L, 1);
                serialize_object(cb, L, &e->key);
                /* leave key on stack */
        }

        return head;
}

static struct l_table *serialize_table(struct callbacks *cb, lua_State *L,
                                       int index)
{
        struct l_table *t;

        t = calloc(1, sizeof(*t));
        assert(t);

        t->id = (void *) lua_topointer(L, index);
        t->entries = dump_table_entries(cb, L, index);

        return t;
}

static void serialize_object(struct callbacks *cb, lua_State *L,
                             struct l_object *object)
{
        int index;

        index = lua_gettop(L);
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
                object->table = serialize_table(cb, L, index);
                break;
        case LUA_TFUNCTION:
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
                                    void *id, int number)
{
        struct l_upvalue *v;

        v = l_upvalue_new(id, number);
        serialize_object(cb, L, &v->value);

        return v;
}

static void push_table(struct callbacks *cb, lua_State *L,
                       struct l_table *table)
{
        struct l_table_entry *e;

        lua_newtable(L);
        for (e = table->entries; e; e = e->next) {
                push_object(cb, L, &e->key);
                push_object(cb, L, &e->value);
                lua_rawset(L, -3);
        }
}

static void push_object(struct callbacks *cb, lua_State *L,
                        const struct l_object *object)
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

static void set_upvalue(struct callbacks *cb, lua_State *L,
                        const struct l_upvalue *upvalue)
{
        const char *name;

        push_object(cb, L, &upvalue->value);
        name = lua_setupvalue(L, -2, upvalue->number);
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

/**
 * Looks through the list for an upvalue with the given id. Returns NULL if no
 * match was found.
 */
static struct l_upvalue *find_upvalue_by_id(struct l_upvalue **head, void *id)
{
        struct l_upvalue *v;

        assert(head);

        for (v = *head; v; v = v->next) {
                if (v->id == id)
                        return v;
        }

        return NULL;
}

static void init_upvalueref(struct l_object *obj, void *func_id)
{
        obj->type = XLUA_TUPVALUEREF;
        obj->func_id = func_id;
}

static struct l_upvalue *create_upvalueref(const struct l_upvalue *upvalue,
                                           void *func_id)
{
        struct l_upvalue *v;

        v = l_upvalue_new(upvalue->id, upvalue->number);
        init_upvalueref(&v->value, func_id);

        return v;

}

/**
 * Records where an upvalue was set, i.e. in what function, by adding an upvalue
 * reference (a special upvalue that stores function id) to the list of
 * references.
 */
static void record_upvalueref(struct l_upvalue **head,
                              const struct l_upvalue *upvalue, void *func_id)
{
        struct l_upvalue *v;

        assert(head);

        v = create_upvalueref(upvalue, func_id);
        prepend_upvalue(head, v);
}

struct upvalue_cache *upvalue_cache_new(void)
{
        return calloc(1, sizeof(struct upvalue_cache));
}

void upvalue_cache_free(struct upvalue_cache *c)
{
        if (c) {
                destroy_upvalues(&c->head);
                free(c);
        }
}

static void get_cached_object(lua_State *L, int cache_idx, void *obj_id)
{
        lua_pushlightuserdata(L, obj_id);
        lua_rawget(L, cache_idx);
}

void set_shared_upvalue(struct callbacks *cb, lua_State *L,
                        struct upvalue_cache *upvalue_cache,
                        int cache_idx, void *func_id,
                        const struct l_upvalue *upvalue)
{
        struct l_upvalue **head = &upvalue_cache->head;
        struct l_upvalue *v;

        get_cached_object(L, cache_idx, func_id);
        v = find_upvalue_by_id(head, upvalue->id);
        if (v) {
                /* An already seen upvalue, we're sharing */
                get_cached_object(L, cache_idx, v->value.func_id);
                lua_upvaluejoin(L, -2, upvalue->number, -1, v->number);
                lua_pop(L, 1);
        } else {
                /* Upvalue seen for the first time */
                set_upvalue(cb, L, upvalue);
                record_upvalueref(head, upvalue, func_id);
        }
        lua_pop(L, 1);
}
