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

#include <sys/types.h>
#include <stdbool.h>

struct addrinfo;
struct msghdr;

struct lua_State;
struct byte_array;
struct l_upvalue;

/* Stay out of errno range */
#define SCRIPT_HOOK_ERROR_BASE (1 << 8)

enum script_hook_error {
        /* no hook to invoke */
        EHOOKEMPTY = SCRIPT_HOOK_ERROR_BASE,
        /* hook didn't return a value */
        EHOOKRETVAL,
        /* hook runtime error (LUA_ERRRUN) */
        EHOOKRUN,
        /* syntax error during hook pre-compilation (LUA_ERRSYNTAX) */
        EHOOKSYNTAX,
        /* hook memory allocation error (LUA_ERRMEM) */
        EHOOKMEM,
        /* error while running the hook error handler function (LUA_ERRERR) */
        EHOOKERR,
};

enum script_hook_id {
        SCRIPT_HOOK_SOCKET = 0,
        SCRIPT_HOOK_CLOSE,
        SCRIPT_HOOK_SENDMSG,
        SCRIPT_HOOK_RECVMSG,
        SCRIPT_HOOK_RECVERR,
        SCRIPT_HOOK_MAX
};

struct script_hook {
        const char *name;
        struct byte_array *bytecode;
        struct l_upvalue *upvalues;
};

struct script_engine {
        struct lua_State *L;
        struct callbacks *cb;
        struct script_hook hooks[SCRIPT_HOOK_MAX];
        void (*run_func)(void *);
        void *run_data;
        int run_mode;
};

struct script_slave {
        struct script_engine *se;
        struct lua_State *L;
        struct callbacks *cb;
        void *hook_key[SCRIPT_HOOK_MAX];
};

int script_engine_create(struct script_engine **sep, struct callbacks *cb,
                         bool is_client);
struct script_engine *script_engine_destroy(struct script_engine *se);

int script_slave_create(struct script_slave **ssp, struct script_engine *se);
struct script_slave *script_slave_destroy(struct script_slave *ss);

int script_engine_run_string(struct script_engine *se, const char *script,
                             void (*run_func)(void *), void *run_data);
int script_engine_run_file(struct script_engine *se, const char *filename,
                           void (*run_func)(void *), void *data);

/**
 * Run post-create socket hook.
 */
int script_slave_socket_hook(struct script_slave *ss, int sockfd, struct addrinfo *ai);

/**
 * Run pre-close socket hook.
 */
int script_slave_close_hook(struct script_slave *ss, int sockfd, struct addrinfo *ai);

/**
 * Run send message hook (on EPOLLIN event).
 */
ssize_t script_slave_sendmsg_hook(struct script_slave *ss, int sockfd,
                                  struct msghdr *msg, int flags);

/**
 * Run receive message hook (on EPOLLOUT event).
 */
ssize_t script_slave_recvmsg_hook(struct script_slave *ss, int sockfd,
                                  struct msghdr *msg, int flags);

/**
 * Run receive error message hook (EPOLLPRI event).
 */
ssize_t script_slave_recverr_hook(struct script_slave *ss, int sockfd,
                                  struct msghdr *msg, int flags);

enum script_hook_error errno_lua(int err);
const char *script_strerror(int errnum);

#endif
