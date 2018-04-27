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
#include <math.h>
#include <stdlib.h>

#include "common.h"
#include "flow.h"
#include "interval.h"
#include "lib.h"
#include "sample.h"
#include "thread.h"
#include "workload.h"


static int tcp_socket_open(const struct addrinfo *hints)
{
        return socket(hints->ai_family, SOCK_STREAM, IPPROTO_TCP);
}

static int udp_socket_open(const struct addrinfo *hints)
{
        return socket(hints->ai_family, SOCK_DGRAM, IPPROTO_UDP);
}

static int socket_bind(const struct socket_ops *ops, int sockfd,
                       const struct sockaddr *addr, socklen_t addrlen)
{
        return ops->bind ? ops->bind(sockfd, addr, addrlen) : 0;
}

static int socket_listen(const struct socket_ops *ops, int sockfd, int backlog)
{
        return ops->listen ? ops->listen(sockfd, backlog) : 0;
}

static int socket_connect(const struct socket_ops *ops, int sockfd,
                          const struct sockaddr *addr, socklen_t addrlen)
{
        return ops->connect ? ops->connect(sockfd, addr, addrlen) : 0;
}

static int socket_close(const struct socket_ops *ops, int sockfd)
{
        return ops->close ? ops->close(sockfd) : 0;
}

static int do_epoll_wait(const struct socket_ops *ops, int epfd,
                         struct epoll_event *events, int maxevents, int timeout)
{
        if (ops->epoll_wait)
                return ops->epoll_wait(epfd, events, maxevents, timeout);
        else
                return epoll_wait(epfd, events, maxevents, timeout);
}

static int do_socket_open(const struct socket_ops *ops, struct script_slave *ss,
                          struct addrinfo *ai)
{
        int fd, r;

        assert(ai);

        fd = ops->open(ai);
        if (fd == -1)
                return -1;

        r = script_slave_socket_hook(ss, fd, ai);
        if (r < 0 && r != -EHOOKEMPTY && r != -EHOOKRETVAL) {
                socket_close(ops, fd);
                errno = -r;
                return -1;
        }

        return fd;
}

static int do_socket_close(const struct socket_ops *ops, struct script_slave *ss,
                           int sockfd, struct addrinfo *ai)
{
        int r;

        r = script_slave_close_hook(ss, sockfd, ai);
        if (r < 0 && r != -EHOOKEMPTY && r != -EHOOKRETVAL) {
                errno = -r;
                return -1;
        }

        return socket_close(ops, sockfd);
}

const struct socket_ops tcp_socket_ops = {
        .open = tcp_socket_open,
        .bind = bind,
        .listen = listen,
        .accept = accept,
        .connect = do_connect,
        .close = do_close,
};

const struct socket_ops udp_socket_ops = {
        .open = udp_socket_open,
        .bind = bind,
        .connect = do_connect,
};

/* Allocate and initialize a buffer big enough for sending/receiving. */
static void *buf_alloc(struct options *opts)
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

/* Open, configure according to options, and connect a client socket. */
static int client_connect(struct thread *t, const struct socket_ops *ops)
{
        struct script_slave *ss = t->script_slave;
        struct options *opts = t->opts;
        struct callbacks *cb = t->cb;
        struct addrinfo *ai = t->ai;
        int fd;

        fd = do_socket_open(ops, ss, ai);
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
        if (socket_connect(ops, fd, ai->ai_addr, ai->ai_addrlen))
                PLOG_FATAL(cb, "socket_connect");

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

void setup_connected_socket(int fd, struct options *opts, struct callbacks *cb)
{
        if (opts->debug)
                set_debug(fd, 1, cb);
        if (opts->max_pacing_rate)
                set_max_pacing_rate(fd, opts->max_pacing_rate, cb);
        if (opts->reuseaddr)
                set_reuseaddr(fd, 1, cb);
}

void run_client(struct thread *t, const struct socket_ops *ops,
                process_events_t process_events)
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

        assert(ops);

        client_fds = calloc(flows_in_this_thread, sizeof(int));
        if (!client_fds)
                PLOG_FATAL(cb, "alloc client_fds array");

        LOG_INFO(cb, "flows_in_this_thread=%d", flows_in_this_thread);
        epfd = epoll_create1(0);
        if (epfd == -1)
                PLOG_FATAL(cb, "epoll_create1");
        stop_fl = addflow_lite(epfd, t->stop_efd, EPOLLIN, cb);
        for (i = 0; i < flows_in_this_thread; i++) {
                fd = client_connect(t, ops);
                setup_connected_socket(fd, opts, cb);

                flow = addflow(t->index, epfd, fd, i, epoll_events(opts), cb);
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
                int nfds = do_epoll_wait(ops, epfd, events, opts->maxevents, ms);
                if (nfds == -1) {
                        if (errno == EINTR)
                                continue;
                        PLOG_FATAL(cb, "epoll_wait");
                }
                process_events(t, epfd, events, nfds, -1, buf);
        }

        for (i = 0; i < flows_in_this_thread; i++) {
                if (do_socket_close(ops, ss, client_fds[i], ai) < 0)
                        /* PLOG_FATAL(cb, "close"); */
                        /* XXX: ignore errors */ ;
        }

        free(buf);
        free(events);
        free(stop_fl);
        do_close(epfd);
}

void run_server(struct thread *t, const struct socket_ops *ops,
                process_events_t process_events)
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

        assert(ops);

        fd_listen = do_socket_open(ops, ss, ai);
        if (fd_listen == -1)
                PLOG_FATAL(cb, "socket");
        if (opts->reuseport)
                set_reuseport(fd_listen, cb);
        set_reuseaddr(fd_listen, 1, cb);
        if (socket_bind(ops, fd_listen, ai->ai_addr, ai->ai_addrlen))
                PLOG_FATAL(cb, "bind");
        if (opts->min_rto)
                set_min_rto(fd_listen, opts->min_rto, cb);
        if (socket_listen(ops, fd_listen, opts->listen_backlog))
                PLOG_FATAL(cb, "listen");
        epfd = epoll_create1(0);
        if (epfd == -1)
                PLOG_FATAL(cb, "epoll_create1");

        listen_fl = addflow(t->index, epfd, fd_listen, t->next_flow_id++,
                            EPOLLIN, cb);
        listen_fl->itv = interval_create(opts->interval, t);

        stop_fl = addflow_lite(epfd, t->stop_efd, EPOLLIN, cb);
        events = calloc(opts->maxevents, sizeof(struct epoll_event));
        buf = buf_alloc(opts);
        if (!buf)
                PLOG_FATAL(cb, "buf_alloc");
        pthread_barrier_wait(t->ready);
        while (!t->stop) {
                int ms = opts->nonblocking ? 10 /* milliseconds */ : -1;
                int nfds = do_epoll_wait(ops, epfd, events, opts->maxevents, ms);
                if (nfds == -1) {
                        if (errno == EINTR)
                                continue;
                        PLOG_FATAL(cb, "epoll_wait");
                }
                process_events(t, epfd, events, nfds, fd_listen, buf);
        }

        if (do_socket_close(ops, ss, fd_listen, ai) < 0)
                PLOG_FATAL(cb, "close");
        delflow(t->index, epfd, listen_fl, cb);

        free(buf);
        free(events);
        free(stop_fl);
        do_close(epfd);
}

static void collect_samples(const struct thread *threads, int num_threads,
                              struct sample **samples_p, int *num_samples_p)
{
        struct sample *samples;
        struct sample *s;
        int num_samples;
        int i, j;

        num_samples = 0;
        for (i = 0; i < num_threads; i++) {
                LIST_FOR_EACH(threads[i].samples, s)
                        num_samples++;
        }

        samples = calloc(num_samples, sizeof(*samples));
        for (i = 0, j = 0; i < num_threads; i++) {
                LIST_FOR_EACH(threads[i].samples, s)
                        samples[j++] = *s;
        }
        qsort(samples, num_samples, sizeof(*samples), compare_samples);

        *samples_p = samples;
        *num_samples_p = num_samples;
}

static void calculate_stream_stats(const struct thread *threads, int num_threads,
                                   const struct sample *samples, int num_samples,
                                   struct stats *stats)
{
        CLEANUP(free) ssize_t **per_flow = NULL;
        const struct timespec *start_time, *end_time;
        ssize_t start_total, current_total;
        int start_index, end_index;
        double duration;
        double total_bytes;
        double throughput;
        double correlation_coefficient;
        double sum_xy, sum_xx, sum_yy;
        struct sample *s;
        int flow_id;
        int tid;
        int i, j;

        start_index = 0;
        end_index = num_samples - 1;

        start_time = &samples[start_index].timestamp;
        end_time = &samples[end_index].timestamp;

        per_flow = calloc(num_threads, sizeof(*per_flow));
        for (i = 0; i < num_threads; i++) {
                int max_flow_id = 0;
                LIST_FOR_EACH(threads[i].samples, s) {
                        if (s->flow_id > max_flow_id)
                                max_flow_id = s->flow_id;
                }
                per_flow[i] = calloc(max_flow_id + 1, sizeof(*per_flow[i]));
        }

        start_total = samples[start_index].bytes_read;
        current_total = start_total;

        tid = samples[start_index].tid;
        flow_id = samples[start_index].flow_id;
        per_flow[tid][flow_id] = start_total;

        duration = 0.0;
        total_bytes = 0.0;
        sum_xy = sum_xx = sum_yy = 0.0;
        for (j = start_index + 1; j <= end_index; j++) {
                tid = samples[j].tid;
                flow_id = samples[j].flow_id;
                current_total -= per_flow[tid][flow_id];
                per_flow[tid][flow_id] = samples[j].bytes_read;
                current_total += per_flow[tid][flow_id];
                duration = seconds_between(start_time, &samples[j].timestamp);
                total_bytes = current_total - start_total;
                sum_xy += duration * total_bytes;
                sum_xx += duration * duration;
                sum_yy += total_bytes * total_bytes;
        }
        if (duration == 0.0 || total_bytes == 0.0) {
                throughput = 0.0;
                correlation_coefficient = 0.0;
        } else {
                throughput = total_bytes / duration;
                correlation_coefficient = sum_xy / sqrt(sum_xx * sum_yy);
        }

        stats->num_samples = num_samples;
        stats->throughput = throughput;
        stats->correlation_coefficient = correlation_coefficient;
        stats->end_time = *end_time;

        for (i = 0; i < num_threads; i++)
                free(per_flow[i]);
}

static void print_stream_stats(const struct callbacks *cb,
                               const struct stats *stats)
{
        PRINT(cb, "num_samples", "%d", stats->num_samples);
        PRINT(cb, "throughput_Mbps", "%.2f", stats->throughput * 8 / 1e6);
        PRINT(cb, "correlation_coefficient", "%.2f",
              stats->correlation_coefficient);
        PRINT(cb, "time_end", "%ld.%09ld",
              stats->end_time.tv_sec, stats->end_time.tv_nsec);
}

void report_stream_stats(struct thread *threads)
{
        CLEANUP(free) struct sample *samples = NULL;
        struct stats stats = { 0 };
        struct callbacks *cb;
        struct options *opts;
        int num_samples;

        cb = threads[0].cb;
        opts = threads[0].opts;

        collect_samples(threads, opts->num_threads, &samples, &num_samples);
        if (num_samples == 0) {
                LOG_WARN(cb, "no samples collected");
                return;
        } else if (num_samples == 1) {
                LOG_WARN(cb, "insufficient number of samples");
                /* We will print some stats. */
        }

        calculate_stream_stats(threads, opts->num_threads, samples, num_samples,
                               &stats);
        print_stream_stats(cb, &stats);

        if (opts->all_samples)
                print_samples(0, samples, num_samples, opts->all_samples, cb);

}
