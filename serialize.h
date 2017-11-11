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

/* Serialized Lua function together with its upvalues. */
struct sfunction;
struct svalue;
struct upvalue_cache;


struct upvalue_cache * upvalue_cache_new(void);
void free_upvalue_cache(struct upvalue_cache *c);

void free_sfunction(struct sfunction *f);
void free_svalue(struct svalue *v);

struct svalue *serialize_value(struct callbacks *cb, lua_State *L);
void deserialize_value(struct callbacks *cb, lua_State *L,
                       struct upvalue_cache *cache, int cache_idx,
                       const struct svalue *value);

/**
 * Serializes the Lua function at the top of the stack.
 */
struct sfunction *serialize_function(struct callbacks *cb, lua_State *L);

/**
 * Deserializes the function and pushes it on top the stack.
 *
 * Caches the deserialized objects so that they can be shared with other
 * deserialized functions.
 */
int deserialize_function(struct callbacks *cb, lua_State *L,
                         struct upvalue_cache *cache, int cache_idx,
                         const struct sfunction *func, const char *name,
                         void **object_key);

#endif
