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

#ifndef NEPER_SCRIPT_H
#define NEPER_SCRIPT_H

struct addrinfo;
struct lua_State;

struct script_engine {
        struct lua_State *L;
        struct callbacks *cb;
};

struct script_slave {
        struct script_engine *se;
        struct lua_State *L;
        struct callbacks *cb;
};

int script_engine_create(struct script_engine **sep, struct callbacks *cb);
struct script_engine *script_engine_destroy(struct script_engine *se);

int script_slave_create(struct script_slave **ssp, struct script_engine *se);
struct script_slave *script_slave_destroy(struct script_slave *ss);

/**
 * Runs the given script passed in a string. Returns 0 on success or a negative
 * value on error.
 */
int script_engine_run_string(struct script_engine *se, const char *script);

/* Callbacks for the client/server workloads */
void script_slave_init(struct script_slave *ss, int sockfd, struct addrinfo *ai);

#endif
