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

#ifndef NEPER_WORKLOAD_H
#define NEPER_WORKLOAD_H

/*
 * Logic shared by all workloads.
 */

#include <stdint.h>


struct epoll_event;

/* Callback invoked from main thread loop for processing socket events. */
typedef void (*process_events_t)(struct thread *t, int epoll_fd,
                                 struct epoll_event *events, int nfds,
                                 int listen_fd, char *buf);


/* Allocate and initialize a buffer big enough for sending/receiving. */
void *buf_alloc(struct options *opts);

/* Open, configure according to options, and connect a client socket. */
int client_connect(struct thread *t);

/* Convert run-time options to a set of epoll events */
uint32_t epoll_events(struct options *opts);

/* Main routine for client threads, both stream & request/response workloads */
void run_client(struct thread *t, process_events_t process_events);

/* Main routine for server threads, both stream & request/response workloads */
void run_server(struct thread *t, process_events_t process_events);

/* XXX: Internal API. Still used by dummy_test. */
int do_socket_open(struct script_slave *ss, struct addrinfo *ai);


#endif
