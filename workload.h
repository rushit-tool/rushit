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

#include <sys/socket.h>
#include <stdint.h>


struct addrinfo;
struct epoll_event;

struct callbacks;
struct options;
struct thread;

/* Set of all possible socket operations. open() is mandatory, rest is optional. */
struct socket_ops {
        int (*open)(const struct addrinfo *hints);
        int (*bind)(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
        int (*listen)(int sockfd, int backlog);
        int (*accept)(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
        int (*connect)(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
        int (*close)(int sockfd);

        /* For dummy/fake workloads only. Defaults to epoll_wait(2) if not set. */
        int (*epoll_wait)(int epfd, struct epoll_event *events, int maxevents, int timeout);
};

/* Statistics we calculate from a set of samples for stream workloads. */
struct stats {
        int num_samples;
        double throughput;      /* bytes per second */
        double correlation_coefficient;
        struct timespec end_time;
};


/* Operations for TCP sockets. */
extern const struct socket_ops tcp_socket_ops;

/* Operations for connected UDP sockets. */
extern const struct socket_ops udp_socket_ops;

/* Callback invoked from main thread loop for processing socket events. */
typedef void (*process_events_t)(struct thread *t, int epoll_fd,
                                 struct epoll_event *events, int nfds,
                                 int listen_fd, char *buf);


/* Convert run-time options to a set of epoll events */
uint32_t epoll_events(struct options *opts);

/* Configure a connected socket according to run-time options */
void setup_connected_socket(int fd, struct options *opts, struct callbacks *cb);

/* Main routine for client threads, both stream & request/response workloads */
void run_client(struct thread *t, const struct socket_ops *ops,
                process_events_t process_events);

/* Main routine for server threads, both stream & request/response workloads */
void run_server(struct thread *t, const struct socket_ops *ops,
                process_events_t process_events);

/* Calculates statistics from samples collected by threads. Optionally, return
 * the aggregated list of samples that caller needs to free(). */
void calculate_stream_stats(const struct thread *threads, int num_threads,
                            struct stats *stats, struct sample **samples_);

/* Calculate and print out statistics for a stream workload */
void report_stream_stats(struct thread *tinfo);


#endif
