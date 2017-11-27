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

#include <assert.h>
#include <stdlib.h>

#include "common.h"
#include "flow.h"
#include "interval.h"
#include "lib.h"
#include "thread.h"
#include "workload.h"


void *buf_alloc(struct options *opts)
{
        size_t alloc_size = opts->request_size;
        void *buf;

        if (alloc_size < opts->response_size)
                alloc_size = opts->response_size;
        /* request/response sizes are zero for stream workloads */
        if (!alloc_size || alloc_size > opts->buffer_size)
                alloc_size = opts->buffer_size;

        buf = calloc(alloc_size, sizeof(char));
        if (!buf)
                return NULL;

        if (opts->enable_write)
                fill_random(buf, alloc_size);

        return buf;
}

int client_connect(struct thread *t)
{
        struct script_slave *ss = t->script_slave;
        struct options *opts = t->opts;
        struct callbacks *cb = t->cb;
        struct addrinfo *ai = t->ai;
        int fd;

        fd = do_socket_open(ss, ai);
        if (fd == -1) {
                PLOG_FATAL(cb, "socket");
                return fd;
        }

        if (opts->min_rto)
                set_min_rto(fd, opts->min_rto, cb);
        if (opts->debug)
                set_debug(fd, 1, cb);
        if (opts->local_host)
                set_local_host(fd, opts, cb);
        if (do_connect(fd, ai->ai_addr, ai->ai_addrlen))
                PLOG_FATAL(cb, "do_connect");

        return fd;
}

uint32_t epoll_events(struct options *opts)
{
        uint32_t events = 0;
        if (opts->enable_write)
                events |= EPOLLOUT;
        if (opts->enable_read)
                events |= EPOLLIN;
        if (opts->edge_trigger)
                events |= EPOLLET;
        return events;
}

int do_socket_open(struct script_slave *ss, struct addrinfo *ai)
{
        int fd, r;

        assert(ai);

        fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (fd == -1)
                return -1;

        r = script_slave_socket_hook(ss, fd, ai);
        if (r < 0 && r != -EHOOKEMPTY && r != -EHOOKRETVAL) {
                do_close(fd);
                errno = -r;
                return -1;
        }

        return fd;
}

int do_socket_close(struct script_slave *ss, int sockfd, struct addrinfo *ai)
{
        int r;

        r = script_slave_close_hook(ss, sockfd, ai);
        if (r < 0 && r != -EHOOKEMPTY && r != -EHOOKRETVAL) {
                errno = -r;
                return -1;
        }

        return do_close(sockfd);
}

void run_client(struct thread *t, process_events_t process_events)
{
        struct script_slave *ss = t->script_slave;
        struct options *opts = t->opts;
        const int flows_in_this_thread = flows_in_thread(opts->num_flows,
                                                         opts->num_threads,
                                                         t->index);
        struct callbacks *cb = t->cb;
        struct addrinfo *ai = t->ai;
        struct epoll_event *events;
        struct flow *flow, *stop_fl;
        int epfd, fd, i;
        char *buf;
        CLEANUP(free) int *client_fds = NULL;

        client_fds = calloc(flows_in_this_thread, sizeof(int));
        if (!client_fds)
                PLOG_FATAL(cb, "alloc client_fds array");

        LOG_INFO(cb, "flows_in_this_thread=%d", flows_in_this_thread);
        epfd = epoll_create1(0);
        if (epfd == -1)
                PLOG_FATAL(cb, "epoll_create1");
        stop_fl = addflow_lite(epfd, t->stop_efd, EPOLLIN, cb);
        for (i = 0; i < flows_in_this_thread; i++) {
                fd = client_connect(t);

                flow = addflow(t->index, epfd, fd, i, epoll_events(opts), opts, cb);
                flow->bytes_to_write = opts->request_size;
                flow->itv = interval_create(opts->interval, t);

                client_fds[i] = fd;
                /* flow will be deleted by process_events() */
        }

        events = calloc(opts->maxevents, sizeof(struct epoll_event));
        buf = buf_alloc(opts);
        if (!buf)
                PLOG_FATAL(cb, "buf_alloc");
        pthread_barrier_wait(t->ready);
        while (!t->stop) {
                int ms = opts->nonblocking ? 10 /* milliseconds */ : -1;
                int nfds = epoll_wait(epfd, events, opts->maxevents, ms);
                if (nfds == -1) {
                        if (errno == EINTR)
                                continue;
                        PLOG_FATAL(cb, "epoll_wait");
                }
                process_events(t, epfd, events, nfds, -1, buf);
        }

        for (i = 0; i < flows_in_this_thread; i++) {
                if (do_socket_close(ss, client_fds[i], ai) < 0)
                        /* PLOG_FATAL(cb, "close"); */
                        /* XXX: ignore errors */ ;
        }

        free(buf);
        free(events);
        free(stop_fl);
        do_close(epfd);
}

void run_server(struct thread *t, process_events_t process_events)
{
        struct script_slave *ss = t->script_slave;
        struct options *opts = t->opts;
        struct callbacks *cb = t->cb;
        struct addrinfo *ai = t->ai;
        struct epoll_event *events;
        struct flow *listen_fl;
        struct flow *stop_fl;
        int fd_listen, epfd;
        char *buf;

        fd_listen = do_socket_open(ss, ai);
        if (fd_listen == -1)
                PLOG_FATAL(cb, "socket");
        set_reuseport(fd_listen, cb);
        set_reuseaddr(fd_listen, 1, cb);
        if (bind(fd_listen, ai->ai_addr, ai->ai_addrlen))
                PLOG_FATAL(cb, "bind");
        if (opts->min_rto)
                set_min_rto(fd_listen, opts->min_rto, cb);
        if (listen(fd_listen, opts->listen_backlog))
                PLOG_FATAL(cb, "listen");
        epfd = epoll_create1(0);
        if (epfd == -1)
                PLOG_FATAL(cb, "epoll_create1");
        listen_fl = addflow_lite(epfd, fd_listen, EPOLLIN, cb);
        stop_fl = addflow_lite(epfd, t->stop_efd, EPOLLIN, cb);
        events = calloc(opts->maxevents, sizeof(struct epoll_event));
        buf = buf_alloc(opts);
        if (!buf)
                PLOG_FATAL(cb, "buf_alloc");
        pthread_barrier_wait(t->ready);
        while (!t->stop) {
                int ms = opts->nonblocking ? 10 /* milliseconds */ : -1;
                int nfds = epoll_wait(epfd, events, opts->maxevents, ms);
                if (nfds == -1) {
                        if (errno == EINTR)
                                continue;
                        PLOG_FATAL(cb, "epoll_wait");
                }
                process_events(t, epfd, events, nfds, fd_listen, buf);
        }

        if (do_socket_close(ss, fd_listen, ai) < 0)
                PLOG_FATAL(cb, "close");

        free(buf);
        free(events);
        free(stop_fl);
        free(listen_fl);
        do_close(epfd);
}
