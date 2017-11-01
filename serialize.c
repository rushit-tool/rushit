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


struct l_table_entry {
        struct l_table_entry *next;
        struct l_object key;
        struct l_object value;
};

struct l_table {
        void *id;
        struct l_table_entry *entries;
};

struct l_function {
        void *id;
        struct byte_array *code;
        struct l_upvalue *upvalues;
};

struct upvalue_mapping {
        struct upvalue_mapping *next;
        void *key;
        void *function_id;
        int upvalue_num;
};

struct object_mapping {
        struct object_mapping *next;
        void *key;
        void *object_id;
};

struct upvalue_cache {
        /* Map of serialized upvalue ids to deserialized (function id, upvalue
         * number) tuples */
        struct upvalue_mapping *upvalue_map;
        /* Map of serialized object ids to deserialized object ids */
        struct object_mapping *object_map;
        /* Lua store for deserialized objects indexed by their id */
        int object_tbl_idx;
};


static void l_object_free_data(struct l_object *o);
static void serialize_object(struct callbacks *cb, lua_State *L,
                             struct l_object *object);
static void push_object(struct callbacks *cb, lua_State *L,
                        struct upvalue_cache *cache,
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

void l_function_free(struct l_function *f)
{
        if (f) {
                byte_array_free(f->code);
                free(f);
        }
}

static int string_writer(lua_State *L, const void *str, size_t len, void *buf)
{
        UNUSED(L);
        luaL_addlstring(buf, str, len);
        return 0;
}

static struct byte_array *dump_function_bytecode(struct callbacks *cb,
                                                 lua_State *L)
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

static int load_function_bytecode(struct callbacks *cb, lua_State *L,
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
                                                lua_State *L)
{
        struct l_table_entry *head = NULL;

        lua_pushnil(L);
        while (lua_next(L, -2)) {
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

static struct l_table *serialize_table(struct callbacks *cb, lua_State *L)
{
        struct l_table *t;

        t = calloc(1, sizeof(*t));
        assert(t);

        t->id = (void *) lua_topointer(L, -1);
        t->entries = dump_table_entries(cb, L);

        return t;
}

struct l_function *serialize_function(struct callbacks *cb, lua_State *L)
{
        struct l_function *f;

        f = calloc(1, sizeof(*f));
        assert(f);

        f->id = (void *) lua_topointer(L, -1);
        f->code = dump_function_bytecode(cb, L);

        return f;
}

static void serialize_object(struct callbacks *cb, lua_State *L,
                             struct l_object *object)
{
        object->type = lua_type(L, -1);

        switch (object->type) {
        case LUA_TNIL:
                assert(false);
                break;
        case LUA_TNUMBER:
                object->number = lua_tonumber(L, -1);
                break;
        case LUA_TBOOLEAN:
                object->boolean = lua_toboolean(L, -1);
                break;
        case LUA_TSTRING:
                object->string = strdup(lua_tostring(L, -1));
                break;
        case LUA_TTABLE:
                object->table = serialize_table(cb, L);
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

static void map_object(struct upvalue_cache *cache, void *key,
                       void *object_id)
{
        struct object_mapping *m;

        m = calloc(1, sizeof(*m));
        assert(m);
        m->key = key;
        m->object_id = object_id;

        m->next = cache->object_map;
        cache->object_map = m;
}

static const struct object_mapping *lookup_object(struct upvalue_cache *cache,
                                                  void *key)
{
        struct object_mapping *m;

        for (m = cache->object_map; m; m = m->next) {
                if (m->key == key)
                        return m;
        }
        return NULL;
}

static void *cache_object(struct upvalue_cache *cache, lua_State *L)
{
        void *id;

        id = (void *) lua_topointer(L, -1);
        lua_pushlightuserdata(L, id);
        lua_pushvalue(L, -2);
        lua_rawset(L, cache->object_tbl_idx);

        return id;
}

static void fetch_object(struct upvalue_cache *cache, lua_State *L, void *id)
{
        lua_pushlightuserdata(L, id);
        lua_rawget(L, cache->object_tbl_idx);
}

static void map_upvalue(struct upvalue_cache *cache, void *key,
                        void *function_id, int upvalue_num)
{
        struct upvalue_mapping *m;

        m = calloc(1, sizeof(*m));
        assert(m);
        m->key = key;
        m->function_id = function_id;
        m->upvalue_num = upvalue_num;

        m->next = cache->upvalue_map;
        cache->upvalue_map = m;
}

static const struct upvalue_mapping *lookup_upvalue(struct upvalue_cache *cache,
                                                    void *key)
{
        struct upvalue_mapping *m;

        for (m = cache->upvalue_map; m; m = m->next) {
                if (m->key == key)
                        return m;
        }
        return NULL;
}

static void push_table(struct callbacks *cb, lua_State *L,
                       struct upvalue_cache *cache,
                       struct l_table *table)
{
        struct l_table_entry *e;
        void *id;

        lua_newtable(L);

        id = cache_object(cache, L);
        map_object(cache, table->id, id);

        for (e = table->entries; e; e = e->next) {
                push_object(cb, L, cache, &e->key);
                push_object(cb, L, cache, &e->value);
                lua_rawset(L, -3);
        }
}

int deserialize_function(struct callbacks *cb, lua_State *L,
                         struct l_function *func, const char *name)
{
        return load_function_bytecode(cb, L, func->code, name);
}

static void push_object(struct callbacks *cb, lua_State *L,
                        struct upvalue_cache *cache,
                        const struct l_object *object)
{
        const struct object_mapping *m;

        if (object->type == LUA_TTABLE) {
                m = lookup_object(cache, object->table->id);
                if (m) {
                        fetch_object(cache, L, m->object_id);
                        return;
                }
        }

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
                push_table(cb, L, cache, object->table);
                break;
        default:
                assert(false);
                break;
        }
}

static void set_upvalue(struct callbacks *cb, lua_State *L,
                        struct upvalue_cache *cache,
                        const struct l_upvalue *upvalue)
{
        const char *name;

        push_object(cb, L, cache, &upvalue->value);
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

struct upvalue_cache *upvalue_cache_new(void)
{
        return calloc(1, sizeof(struct upvalue_cache));
}

static void destroy_object_map(struct object_mapping *m)
{
        struct object_mapping *m_next;

        while (m) {
                m_next = m->next;
                free(m);
                m = m_next;
        }
}

static void destroy_upvalue_map(struct upvalue_mapping *m)
{
        struct upvalue_mapping *m_next;

        while (m) {
                m_next = m->next;
                free(m);
                m = m_next;
        }
}

void upvalue_cache_free(struct upvalue_cache *c)
{
        if (c) {
                destroy_object_map(c->object_map);
                destroy_upvalue_map(c->upvalue_map);
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
        const struct upvalue_mapping *m;

        upvalue_cache->object_tbl_idx = cache_idx;

        get_cached_object(L, cache_idx, func_id);
        m = lookup_upvalue(upvalue_cache, upvalue->id);
        if (m) {
                /* An already seen upvalue, we're sharing */
                get_cached_object(L, cache_idx, m->function_id);
                lua_upvaluejoin(L, -2, upvalue->number, -1, m->upvalue_num);
                lua_pop(L, 1);
        } else {
                /* Upvalue seen for the first time */
                set_upvalue(cb, L, upvalue_cache, upvalue);
                map_upvalue(upvalue_cache, upvalue->id,
                            func_id, upvalue->number);
        }
        lua_pop(L, 1);
}
