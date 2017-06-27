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
#include <stdlib.h>

#include "common.h"
#include "flow.h"
#include "lib.h"
#include "logging.h"
#include "thread.h"

static void client_events(struct thread *t, int epfd,
                          struct epoll_event *events, int nfds, char *buf)
{
        struct flow *flow;
        int i;

        for (i = 0; i < nfds; i++) {
                flow = events[i].data.ptr;
                if (flow->fd == t->stop_efd) {
                        t->stop = 1;
                        break;
                }
                /* STUB: Delete flow on EPOLLRDHUP */
                /* STUB: Write on EPOLLOUT */
                /* STUB: Read on EPOLLIN */
        }
}

static void run_client(struct thread *t)
{
        struct options *opts = t->opts;
        struct callbacks *cb = t->cb;
        struct epoll_event *events;
        struct flow *stop_fl;
        char *buf = NULL;
        int epfd;

        /* Setup I/O multiplexer */
        epfd = epoll_create1(0);
        if (epfd == -1)
                PLOG_FATAL(cb, "epoll_create1");
        stop_fl = addflow_lite(epfd, t->stop_efd, EPOLLIN, cb);
        /* STUB: For each flow connect to server */
        events = calloc(opts->maxevents, sizeof(struct epoll_event));

        /* STUB: Allocate buffers */

        /* Sync threads */
        pthread_barrier_wait(t->ready);

        /* Main loop */
        while (!t->stop) {
                int ms = opts->nonblocking ? 10 /* milliseconds */ : -1;
                int nfds = epoll_wait(epfd, events, opts->maxevents, ms);
                if (nfds == -1) {
                        if (errno == EINTR)
                                continue;
                        PLOG_FATAL(cb, "epoll_wait");
                }
                client_events(t, epfd, events, nfds, buf);
        }

        free(events);
        free(stop_fl);
        do_close(epfd);
}

static void server_events(struct thread *t, int epfd,
			  struct epoll_event *events, int nfds, int fd_listen,
			  char *buf)
{
	int i;

	for (i = 0; i < nfds; i++) {
		struct flow *flow = events[i].data.ptr;
		if (flow->fd == t->stop_efd) {
			t->stop = 1;
			break;
		}
		/* STUB: Accept incoming data connections */
		/* STUB: Delete flow on EPOLLRDHUP */
		/* STUB: Read on EPOLLIN */
		/* STUB: Write on EPOLLOUT */
	}
}

static void run_server(struct thread *t)
{
	struct options *opts = t->opts;
        struct callbacks *cb = t->cb;
	struct epoll_event *events;
        struct flow *stop_fl;
	int fd_listen = -1, epfd;
	char *buf = NULL;

        assert(opts->maxevents > 0);

	/* STUB: Create data plane listening socket */
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
		int nfds =  epoll_wait(epfd, events, opts->maxevents, ms);
		if (nfds == -1) {
			if (errno == EINTR)
				continue;
			PLOG_FATAL(cb, "epoll_wait");
		}
		/* Process events */
		server_events(t, epfd, events, nfds, fd_listen, buf);
	}

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
	return run_main_thread(opts, cb, worker_thread, report_stats);
}
