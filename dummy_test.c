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
 * Dummy workload that exercises the common routines and the control plane. It
 * doesn't implement a data plane, that is it doesn't open or operate on data
 * sockets. As a side-effect, it can serve as a template/skeleton for new
 * workloads. If you're refactoring the other workloads, please update the dummy
 * workload as well.
 *
 * Code is structured the same way as in the existing, full-blown, workloads.
 * Parts that are missing but would be there in a fully functional workload are
 * marked with placeholder comment (look for STUB markers).
 */

#include <sys/epoll.h>
#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>

#include "common.h"
#include "flow.h"
#include "lib.h"
#include "logging.h"
#include "thread.h"

static struct flow fake_flow = {
        .fd = -1,
};

static struct epoll_event fake_client_events[] = {
        { .events = EPOLLOUT, .data = { .ptr = &fake_flow } },
        { .events = EPOLLIN,  .data = { .ptr = &fake_flow } },
        { .events = EPOLLERR, .data = { .ptr = &fake_flow } },
};

static struct epoll_event fake_server_events[] = {
        { .events = EPOLLIN,  .data = { .ptr = &fake_flow } },
        { .events = EPOLLOUT, .data = { .ptr = &fake_flow } },
        { .events = EPOLLERR, .data = { .ptr = &fake_flow } },
};

static struct epoll_event *fake_events;
static size_t n_fake_events;

static void init_fake_events(bool is_client)
{
        if (is_client) {
                fake_events = fake_client_events;
                n_fake_events = ARRAY_SIZE(fake_client_events);
        } else {
                fake_events = fake_server_events;
                n_fake_events = ARRAY_SIZE(fake_server_events);
        }
}

static int fake_epoll_wait(int epfd, struct epoll_event *events,
                           int maxevents, int timeout)
{
        static size_t i = 0;

        assert(maxevents > 0);

        /* Fake an event */
        if (i < n_fake_events) {
                memcpy(events, &fake_events[i], sizeof(fake_events[i]));
                ++i;
                return 1;
        }

        return epoll_wait(epfd, events, maxevents, timeout);
}

static void client_events(struct thread *t, int epfd,
                          struct epoll_event *events, int nfds, char *buf)
{
        struct callbacks *cb = t->cb;
        struct flow *flow;
        ssize_t num_bytes;
        int i;

        for (i = 0; i < nfds; i++) {
                flow = events[i].data.ptr;
                if (flow->fd == t->stop_efd) {
                        t->stop = 1;
                        break;
                }
                /* STUB: Delete flow on EPOLLRDHUP */
                if (events[i].events & EPOLLOUT) {
                        ssize_t to_write = flow->bytes_to_write;
                        int flags = 0;

                        num_bytes = do_write(t->script_slave, flow->fd, buf,
                                             to_write, flags);
                        if (num_bytes < 0) {
                                PLOG_ERROR(cb, "write failed with %zd",
                                           num_bytes);
                                continue;
                        }
                } else if (events[i].events & EPOLLIN) {
                        ssize_t to_read = flow->bytes_to_read;
                        int flags = 0;

                        num_bytes = do_read(t->script_slave, flow->fd, buf,
                                            to_read, flags);
                        if (num_bytes < 0) {
                                PLOG_ERROR(cb, "read failed with %zd",
                                           num_bytes);
                                continue;
                        }
                } else if (events[i].events & EPOLLERR) {
                        ssize_t to_read = 0;
                        int flags = 0;

                        num_bytes = do_readerr(t->script_slave, flow->fd, buf,
                                               to_read, flags);
                        if (num_bytes < 0) {
                                PLOG_ERROR(cb, "read error failed with %zd",
                                           num_bytes);
                                continue;
                        }
                }
        }
}

static void client_connect(int i, int epfd, struct thread *t)
{
        struct script_slave *ss = t->script_slave;
        struct callbacks *cb = t->cb;
        struct addrinfo *ai = t->ai;
        int fd;

        fd = do_socket_open(ss, ai);
        if (fd == -1)
                PLOG_FATAL(cb, "socket");
        /* STUB: Set socket options */
        /* STUB: Connect socket */
        /* STUB: Add flow */
}

static void run_client(struct thread *t)
{
        struct script_slave *ss = t->script_slave;
        struct options *opts = t->opts;
        const int flows_in_this_thread = flows_in_thread(opts->num_flows,
                                                         opts->num_threads,
                                                         t->index);
        struct callbacks *cb = t->cb;
        struct addrinfo *ai = t->ai;
        struct epoll_event *events;
        struct flow *stop_fl;
        char *buf = NULL;
        int epfd, i;

        /* Setup I/O multiplexer */
        epfd = epoll_create1(0);
        if (epfd == -1)
                PLOG_FATAL(cb, "epoll_create1");
        stop_fl = addflow_lite(epfd, t->stop_efd, EPOLLIN, cb);
        for (i = 0; i < flows_in_this_thread; i++)
                client_connect(i, epfd, t);
        events = calloc(opts->maxevents, sizeof(struct epoll_event));

        /* STUB: Allocate buffers */

        /* Sync threads */
        pthread_barrier_wait(t->ready);

        /* Main loop */
        while (!t->stop) {
                int ms = opts->nonblocking ? 10 /* milliseconds */ : -1;
                int nfds = fake_epoll_wait(epfd, events, opts->maxevents, ms);
                if (nfds == -1) {
                        if (errno == EINTR)
                                continue;
                        PLOG_FATAL(cb, "epoll_wait");
                }
                client_events(t, epfd, events, nfds, buf);
        }

        /* XXX: Broken. No way to access sockets opened in client_connect() ATM. */
        for (i = 0; i < flows_in_this_thread; i++) {
                if (do_socket_close(ss, -1, ai) < 0)
                        /* PLOG_FATAL(cb, "close"); */
                        /* XXX: ignore errors */ ;
        }

        free(events);
        free(stop_fl);
        do_close(epfd);
}

static void server_events(struct thread *t, int epfd,
                          struct epoll_event *events, int nfds, int fd_listen,
                          char *buf)
{
        struct callbacks *cb = t->cb;
        ssize_t num_bytes;
        int i;

        for (i = 0; i < nfds; i++) {
                struct flow *flow = events[i].data.ptr;
                if (flow->fd == t->stop_efd) {
                        t->stop = 1;
                        break;
                }
                /* STUB: Accept incoming data connections */
                /* STUB: Delete flow on EPOLLRDHUP */
                if (events[i].events & EPOLLIN) {
                        ssize_t to_write = flow->bytes_to_write;
                        int flags = 0;

                        num_bytes = do_write(t->script_slave, flow->fd, buf,
                                             to_write, flags);
                        if (num_bytes < 0) {
                                PLOG_ERROR(cb, "write failed with %zd",
                                           num_bytes);
                                continue;
                        }
                } else if (events[i].events & EPOLLOUT) {
                        ssize_t to_read = flow->bytes_to_read;
                        int flags = 0;

                        num_bytes = do_read(t->script_slave, flow->fd, buf,
                                            to_read, flags);
                        if (num_bytes < 0) {
                                PLOG_ERROR(cb, "read failed with %zd",
                                           num_bytes);
                                continue;
                        }
                } else if (events[i].events & EPOLLERR) {
                        ssize_t to_read = 0;
                        int flags = 0;

                        num_bytes = do_readerr(t->script_slave, flow->fd, buf,
                                               to_read, flags);
                        if (num_bytes < 0) {
                                PLOG_ERROR(cb, "read error failed with %zd",
                                           num_bytes);
                                continue;
                        }
                }
        }
}

static void run_server(struct thread *t)
{
        struct script_slave *ss = t->script_slave;
        struct options *opts = t->opts;
        struct callbacks *cb = t->cb;
        struct addrinfo *ai = t->ai;
        struct epoll_event *events;
        struct flow *stop_fl;
        int fd_listen = -1, epfd;
        char *buf = NULL;

        assert(opts->maxevents > 0);

        fd_listen = do_socket_open(ss, ai);
        if (fd_listen == -1)
                PLOG_FATAL(cb, "socket");
        /* STUB: Set socket options */
        /* STUB: Bind & listen */

        /* Setup I/O multiplexer */
        epfd = epoll_create1(0);
        if (epfd == -1)
                PLOG_FATAL(cb, "epoll_create1");
        stop_fl = addflow_lite(epfd, t->stop_efd, EPOLLIN, cb);
        events = calloc(opts->maxevents, sizeof(struct epoll_event));

        /* STUB: Allocate buffers */

        /* Sync threads */
        pthread_barrier_wait(t->ready);

        /* Main loop */
        while (!t->stop) {
                /* Poll for events */
                int ms = opts->nonblocking ? 10 /* milliseconds */ : -1;
                int nfds =  fake_epoll_wait(epfd, events, opts->maxevents, ms);
                if (nfds == -1) {
                        if (errno == EINTR)
                                continue;
                        PLOG_FATAL(cb, "epoll_wait");
                }
                /* Process events */
                server_events(t, epfd, events, nfds, fd_listen, buf);
        }

        /* XXX: Sync threads? */
        if (do_socket_close(ss, fd_listen, ai) < 0)
                PLOG_FATAL(cb, "close");

        /* Free resources */
        /* STUB: Free buffers */
        free(events);
        free(stop_fl);
        do_close(epfd);
}

static void *worker_thread(void *arg)
{
        struct thread *t = arg;
        struct options *opts = t->opts;

        assert(opts->port);

        reset_port(t->ai, atoi(opts->port), t->cb);

        if (opts->client)
                run_client(t);
        else
                run_server(t);

        return NULL;
}

static void report_stats(struct thread *tinfo)
{
        (void) tinfo;
}

int dummy_test(struct options *opts, struct callbacks *cb)
{
        init_fake_events(opts->client);
        return run_main_thread(opts, cb, worker_thread, report_stats);
}
