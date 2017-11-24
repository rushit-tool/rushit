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


/* Allocate and initialize a buffer big enough for sending/receiving. */
void *buf_alloc(struct options *opts);

/* Open, configure according to options, and connect a client socket. */
int client_connect(struct thread *t);


#endif
