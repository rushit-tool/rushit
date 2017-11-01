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

/*
 * Tools for transferring Lua values from one Lua state to another.
 */

#ifndef NEPER_SERIALIZE_H
#define NEPER_SERIALIZE_H

#include <stdbool.h>

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"


struct callbacks;

struct l_object {
        int type;
        union {
                bool boolean;
                lua_Number number;
                char *string;
                struct byte_array *function;
                struct l_table *table;
        };
};

struct l_upvalue {
        struct l_upvalue *next;
        void *id;
        int number;
        struct l_object value;
};

struct l_function;
struct upvalue_cache;


struct upvalue_cache * upvalue_cache_new(void);
void upvalue_cache_free(struct upvalue_cache *c);

void l_function_free(struct l_function *f);

/**
 * Serializes the Lua function at the top of the stack. Just the function code
 * without its upvalues.
 */
struct l_function *serialize_function(struct callbacks *cb, lua_State *L);

/**
 * Deserializes the function and leaves it on top the stack. Function upvalues
 * have to be set separately.
 */
int deserialize_function(struct callbacks *cb, lua_State *L,
                         struct l_function *func, const char *name);

/**
 * Serializes an upvalue. Expects the upvalue to be at the top of the stack.
 * Takes the upvalue's number for use during deserialization at a later time.
 */
struct l_upvalue *serialize_upvalue(struct callbacks *cb, lua_State *L,
                                    void *id, int number);

/**
 * Deserializes an upvalue value and sets it as an upvalue of a function
 * identified by func_id.
 *
 * Records each upvalue deserialized for the first time in the cache table
 * located at given index on the stack. If an upvalue has been deserialized
 * before, it will be reused the next time it is encountered via
 * lua_upvaluejoin().
 */
void set_shared_upvalue(struct callbacks *cb, lua_State *L,
                        struct upvalue_cache *upvalue_cache,
                        int cache_idx, void *func_id,
                        const struct l_upvalue *upvalue);

/**
 * Frees a list of upvalues. List head pointer gets reset to NULL.
 */
void destroy_upvalues(struct l_upvalue **head);

/**
 * Inserts a given upvale at the begining of a list.
 */
void prepend_upvalue(struct l_upvalue **head, struct l_upvalue *upvalue);

#endif
