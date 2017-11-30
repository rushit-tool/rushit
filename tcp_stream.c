/*
 * Copyright 2016 Google Inc.
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

#include <math.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <unistd.h>
#include "common.h"
#include "flow.h"
#include "interval.h"
#include "lib.h"
#include "logging.h"
#include "sample.h"
#include "thread.h"
#include "workload.h"

/**
 * The function expects @fd_listen is in a "ready" state in the @epfd
 * epoll set, and directly calls accept() on @fd_listen. The readiness
 * should guarantee that the accept() doesn't block.
 *
 * After a client socket fd is obtained, a new flow is created as part
 * of the thread @t.
 */
static void server_accept(int fd_listen, int epfd, struct thread *t)
{
        struct options *opts = t->opts;
        struct callbacks *cb = t->cb;
        struct sockaddr_storage cli_addr;
        socklen_t cli_len;
        struct flow *flow;
        int client;

        cli_len = sizeof(cli_addr);
        client = accept(fd_listen, (struct sockaddr *)&cli_addr, &cli_len);
        if (client == -1) {
                if (errno == EINTR || errno == ECONNABORTED)
                        return;
                PLOG_ERROR(cb, "accept");
                return;
        }
        setup_connected_socket(client, opts, cb);

        flow = addflow(t->index, epfd, client, t->next_flow_id++,
                       epoll_events(opts), cb);
        flow->itv = interval_create(opts->interval, t);
}

static void process_events(struct thread *t, int epfd,
                           struct epoll_event *events, int nfds, int fd_listen,
                           char *buf)
{
        struct script_slave *ss = t->script_slave;
        struct options *opts = t->opts;
        struct callbacks *cb = t->cb;
        struct timespec ts;
        ssize_t num_bytes;
        int i;

        for (i = 0; i < nfds; i++) {
                struct flow *flow = events[i].data.ptr;
                if (flow->fd == t->stop_efd) {
                        t->stop = 1;
                        break;
                }
                if (flow->fd == fd_listen) {
                        server_accept(fd_listen, epfd, t);
                        continue;
                }
                if (events[i].events & EPOLLRDHUP) {
                        delflow(t->index, epfd, flow, cb);
                        continue;
                }
                if (opts->enable_read && (events[i].events & EPOLLIN)) {
read_again:
                        num_bytes = do_read(ss, flow->fd, buf,
                                            opts->buffer_size, 0);
                        if (num_bytes == -1) {
                                if (errno != EAGAIN)
                                        PLOG_ERROR(cb, "read");
                                continue;
                        }
                        if (num_bytes == 0) {
                                delflow(t->index, epfd, flow, cb);
                                continue;
                        }
                        flow->bytes_read += num_bytes;
                        flow->transactions++;
                        interval_collect(flow, t);
                        if (opts->edge_trigger)
                                goto read_again;
                }
                if (opts->enable_write && (events[i].events & EPOLLOUT)) {
write_again:
                        num_bytes = do_write(ss, flow->fd, buf,
                                             opts->buffer_size, 0);
                        if (num_bytes == -1) {
                                if (errno != EAGAIN)
                                        PLOG_ERROR(cb, "write");
                                continue;
                        }
                        if (opts->delay) {
                                ts.tv_sec = opts->delay / (1000*1000*1000);
                                ts.tv_nsec = opts->delay % (1000*1000*1000);
                                nanosleep(&ts, NULL);
                        }
                        if (opts->edge_trigger)
                                goto write_again;
                }
                if (events[i].events & EPOLLERR) {
                        num_bytes = do_readerr(ss, flow->fd, buf,
                                               opts->buffer_size, 0);
                        if (num_bytes == -1) {
                                if (errno != EAGAIN)
                                        PLOG_ERROR(cb, "readerr");
                                continue;
                        }
                }
        }
}

static void *worker_thread(void *arg)
{
        struct thread *t = arg;
        reset_port(t->ai, atoi(t->opts->port), t->cb);
        if (t->opts->client)
                run_client(t, &tcp_socket_ops, process_events);
        else
                run_server(t, &tcp_socket_ops, process_events);
        return NULL;
}

int tcp_stream(struct options *opts, struct callbacks *cb)
{
        if (opts->delay)
                prctl(PR_SET_TIMERSLACK, 1UL);
        return run_main_thread(opts, cb, worker_thread, report_stream_stats);
}
