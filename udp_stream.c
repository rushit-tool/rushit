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

#include <sys/prctl.h>

#include "common.h"
#include "flow.h"
#include "interval.h"
#include "lib.h"
#include "logging.h"
#include "thread.h"
#include "workload.h"

static void process_events(struct thread *t, int epfd,
                           struct epoll_event *events, int nfds,
                           int listen_fd, char *buf)
{
        struct script_slave *ss = t->script_slave;
        struct options *opts = t->opts;
        struct callbacks *cb = t->cb;

        struct flow *flow;
        ssize_t num_bytes;
        int i;

        UNUSED(epfd);
        UNUSED(listen_fd);

        for (i = 0; i < nfds; i++) {
                flow = events[i].data.ptr;

                if (flow->fd == t->stop_efd) {
                        t->stop = 1;
                        break;
                }

                if (opts->enable_read && (events[i].events & EPOLLIN)) {
                        ssize_t to_read = opts->buffer_size;
read_again:
                        num_bytes = do_read(ss, flow->fd, buf, to_read, 0);
                        if (num_bytes == -1) {
                                if (errno != EAGAIN)
                                        PLOG_ERROR(cb, "read");
                                continue;
                        }

                        flow->bytes_read += num_bytes;
                        flow->transactions++;
                        interval_collect(flow, t);

                        if (opts->edge_trigger)
                                goto read_again;
                }

                if (opts->enable_write && (events[i].events & EPOLLOUT)) {
                        ssize_t to_write = opts->buffer_size;
write_again:
                        num_bytes = do_write(ss, flow->fd, buf, to_write, 0);
                        if (num_bytes == -1) {
                                if (errno != EAGAIN)
                                        PLOG_ERROR(cb, "write");
                                continue;
                        }

                        if (opts->edge_trigger)
                                goto write_again;
                }

                if (events[i].events & EPOLLERR) {
                        ssize_t to_read = opts->buffer_size;
readerr_again:
                        num_bytes = do_readerr(ss, flow->fd, buf, to_read, 0);
                        if (num_bytes == -1) {
                                if (errno != EAGAIN)
                                        PLOG_ERROR(cb, "readerr");
                                continue;
                        }

                        if (opts->edge_trigger)
                                goto readerr_again;
                }
        }
}

static void *worker_thread(void *arg)
{
        struct thread *t = arg;
        struct options *opts = t->opts;
        int port_off;

        port_off = opts->reuseport ? 0 : t->index;
        reset_port(t->ai, atoi(opts->port) + port_off, t->cb);

        if (t->opts->client)
                run_client(t, &udp_socket_ops, process_events);
        else
                run_server(t, &udp_socket_ops, process_events);

        return NULL;
}

int udp_stream(struct options *opts, struct callbacks *cb)
{
        return run_main_thread(opts, cb, worker_thread, report_stream_stats);
}
