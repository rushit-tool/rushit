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

#include "thread.h"
#include <errno.h>
#include <sched.h>
#include <string.h>
#include <sys/eventfd.h>
#include <sys/resource.h>
#include <unistd.h>
#include "common.h"
#include "control_plane.h"
#include "cpuinfo.h"
#include "logging.h"
#include "sample.h"
#include "script.h"


struct rusage_interval {
        struct timespec time_start; /* shared by flows */
        pthread_mutex_t time_start_mutex;

        struct rusage rusage_start; /* updated when first packet comes */
        struct rusage rusage_end;   /* updated only from main thread */
};


static int get_cpuset(cpu_set_t *cpuset, struct callbacks *cb)
{
        int i, j, n, num_cores, physical_id[CPU_SETSIZE], core_id[CPU_SETSIZE];
        struct cpuinfo *cpus;

        cpus = calloc(CPU_SETSIZE, sizeof(struct cpuinfo));
        if (!cpus)
                PLOG_FATAL(cb, "calloc cpus");
        n = get_cpuinfo(cpus, CPU_SETSIZE);
        if (n == -1)
                PLOG_FATAL(cb, "get_cpuinfo");
        if (n == 0)
                LOG_FATAL(cb, "no cpu found in /proc/cpuinfo");
        num_cores = 0;
        for (i = 0; i < n; i++) {
                LOG_INFO(cb, "%d\t%d\t%d\t%d\t%d", cpus[i].processor,
                         cpus[i].physical_id, cpus[i].siblings, cpus[i].core_id,
                         cpus[i].cpu_cores);
                for (j = 0; j < num_cores; j++) {
                        if (physical_id[j] == cpus[i].physical_id &&
                            core_id[j] == cpus[i].core_id)
                                break;
                }
                if (j == num_cores) {
                        num_cores++;
                        CPU_ZERO(&cpuset[j]);
                        core_id[j] = cpus[i].core_id;
                        physical_id[j] = cpus[i].physical_id;
                }
                CPU_SET(cpus[i].processor, &cpuset[j]);
        }
        free(cpus);
        return num_cores;
}

static void start_worker_threads(struct callbacks *cb, struct thread *threads,
                                 int n_threads, void *(*thread_func)(void *),
                                 bool pin_cpu)
{
        CLEANUP(free) cpu_set_t *cpu_set = NULL;
        pthread_attr_t attr;
        struct thread *t;
        int n_cores = 1;
        int i, s;

        cpu_set = calloc(CPU_SETSIZE, sizeof(*cpu_set));
        if (!cpu_set)
                PLOG_FATAL(cb, "calloc cpu_set");
        if (pin_cpu)
                n_cores = get_cpuset(cpu_set, cb);

        s = pthread_attr_init(&attr);
        if (s != 0)
                LOG_FATAL(cb, "pthread_attr_init: %s", strerror(s));

        for (i = 0, t = threads; i < n_threads; i++, t++) {
                if (pin_cpu) {
                        s = pthread_attr_setaffinity_np(&attr,
                                                        sizeof(*cpu_set),
                                                        &cpu_set[i % n_cores]);
                        if (s != 0) {
                                LOG_FATAL(cb, "pthread_attr_setaffinity_np: %s",
                                          strerror(s));
                        }
                }

                s = pthread_create(&t->id, &attr, thread_func, t);
                if (s != 0)
                        LOG_FATAL(cb, "pthread_create: %s", strerror(s));
        }

        s = pthread_attr_destroy(&attr);
        if (s != 0)
                LOG_FATAL(cb, "pthread_attr_destroy: %s", strerror(s));
}

void create_worker_threads(struct options *opts, struct callbacks *cb,
                          struct thread *t, void *(*thread_func)(void *),
                          pthread_barrier_t *ready, struct rusage_interval *rui,
                          struct addrinfo *ai, struct script_engine *se)
{
        int s, i;

        for (i = 0; i < opts->num_threads; i++) {
                t[i].index = i;
                t[i].ai = copy_addrinfo(ai);
                t[i].stop_efd = eventfd(0, 0);
                if (t[i].stop_efd == -1)
                        PLOG_FATAL(cb, "eventfd");
                t[i].samples = NULL;
                t[i].opts = opts;
                t[i].cb = cb;
                t[i].ready = ready;
                t[i].time_start = &rui->time_start;
                t[i].time_start_mutex = &rui->time_start_mutex;
                t[i].rusage_start = &rui->rusage_start;

                s = script_slave_create(&t[i].script_slave, se);
                if (s < 0) {
                        LOG_FATAL(cb, "failed to create script slave: %s",
                                  strerror(-s));
                }
        }
}

void stop_worker_threads(struct callbacks *cb, int num_threads,
                         struct thread *t)
{
        int i, s;

        // tell them to stop
        for (i = 0; i < num_threads; i++) {
                if (eventfd_write(t[i].stop_efd, 1))
                        PLOG_FATAL(cb, "eventfd_write");
                else
                        LOG_INFO(cb, "told thread %d to stop", i);
        }

        // wait for them to stop
        for (i = 0; i < num_threads; i++) {
                s = pthread_join(t[i].id, NULL);
                if (s != 0)
                        LOG_FATAL(cb, "pthread_join: %s", strerror(s));
                else
                        LOG_INFO(cb, "joined thread %d", i);
        }
}

static void free_worker_threads(int num_threads, struct thread *t)
{
        int i;

        for (i = 0; i < num_threads; i++) {
                do_close(t[i].stop_efd);
                free(t[i].ai);
                free_samples(t[i].samples);
                script_slave_destroy(t[i].script_slave);
        }
        free(t);
}

static void run_worker_threads(struct callbacks *cb, struct control_plane *cp,
                               struct rusage_interval *rui,
                               struct thread *threads, int n_threads,
                               void *(*thread_func)(void *), bool pin_cpu,
                               pthread_barrier_t *threads_ready)
{
        start_worker_threads(cb, threads, n_threads, thread_func, pin_cpu);
        LOG_INFO(cb, "started worker threads");

        pthread_barrier_wait(threads_ready);
        LOG_INFO(cb, "worker threads are ready");

        getrusage(RUSAGE_SELF, &rui->rusage_start);
        control_plane_wait_until_done(cp);
        getrusage(RUSAGE_SELF, &rui->rusage_end);

        stop_worker_threads(cb, n_threads, threads);
        LOG_INFO(cb, "stopped worker threads");
}

static void report_rusage(struct callbacks *cb,
                          const struct rusage_interval *rui)
{
        const struct timespec *time_start = &rui->time_start;
        const struct rusage *rusage_start = &rui->rusage_start;
        const struct rusage *rusage_end = &rui->rusage_end;

        PRINT(cb, "time_start", "%ld.%09ld",
              time_start->tv_sec, time_start->tv_nsec);
        PRINT(cb, "utime_start", "%ld.%06ld",
              rusage_start->ru_utime.tv_sec, rusage_start->ru_utime.tv_usec);
        PRINT(cb, "utime_end", "%ld.%06ld",
              rusage_end->ru_utime.tv_sec, rusage_end->ru_utime.tv_usec);
        PRINT(cb, "stime_start", "%ld.%06ld",
              rusage_start->ru_stime.tv_sec, rusage_start->ru_stime.tv_usec);
        PRINT(cb, "stime_end", "%ld.%06ld",
              rusage_end->ru_stime.tv_sec, rusage_end->ru_stime.tv_usec);
        PRINT(cb, "maxrss_start", "%ld", rusage_start->ru_maxrss);
        PRINT(cb, "maxrss_end", "%ld", rusage_end->ru_maxrss);
        PRINT(cb, "minflt_start", "%ld", rusage_start->ru_minflt);
        PRINT(cb, "minflt_end", "%ld", rusage_end->ru_minflt);
        PRINT(cb, "majflt_start", "%ld", rusage_start->ru_majflt);
        PRINT(cb, "majflt_end", "%ld", rusage_end->ru_majflt);
        PRINT(cb, "nvcsw_start", "%ld", rusage_start->ru_nvcsw);
        PRINT(cb, "nvcsw_end", "%ld", rusage_end->ru_nvcsw);
        PRINT(cb, "nivcsw_start", "%ld", rusage_start->ru_nivcsw);
        PRINT(cb, "nivcsw_end", "%ld", rusage_end->ru_nivcsw);
}

int run_main_thread(struct options *opts, struct callbacks *cb,
                    void *(*thread_func)(void *),
                    void (*report_stats)(struct thread *))
{
        pthread_barrier_t ready_barrier; // shared by threads
        struct rusage_interval rui = {
                .time_start = { 0 },
                .time_start_mutex = PTHREAD_MUTEX_INITIALIZER,
        };
        struct addrinfo *ai;
        struct thread *ts; // worker threads
        struct control_plane *cp;
        struct script_engine *se;
        int r;

        PRINT(cb, "total_run_time", "%d", opts->test_length);
        if (opts->dry_run)
                return 0;

        r = script_engine_create(&se, cb, opts->client);
        if (r < 0)
                LOG_FATAL(cb, "failed to create script engine: %s", strerror(-r));

        cp = control_plane_create(opts, cb, se);
        if (!cp)
                LOG_FATAL(cb, "failed to create control plane");
        control_plane_start(cp, &ai);

        r = pthread_barrier_init(&ready_barrier, NULL, opts->num_threads + 1);
        if (r != 0)
                LOG_FATAL(cb, "pthread_barrier_init: %s", strerror(r));

        // start threads *after* control plane is up, to reuse addrinfo.
        ts = calloc(opts->num_threads, sizeof(struct thread));
        create_worker_threads(opts, cb, ts, thread_func, &ready_barrier, &rui,
                              ai, se);
        free(ai);

        if (opts->script) {
                r = script_engine_run_file(se, opts->script, NULL, NULL);
                if (r < 0)
                        LOG_FATAL(cb, "script failed: %s: %s",
                                  opts->script, strerror(-r));
        }
        run_worker_threads(cb, cp, &rui, ts, opts->num_threads, thread_func,
                           opts->pin_cpu, &ready_barrier);

        r = pthread_barrier_destroy(&ready_barrier);
        if (r != 0)
                LOG_FATAL(cb, "pthread_barrier_destroy: %s", strerror(r));

        control_plane_stop(cp);
        PRINT(cb, "invalid_secret_count", "%d", control_plane_incidents(cp));
        report_rusage(cb, &rui);
        report_stats(ts);
        free_worker_threads(opts->num_threads, ts);
        control_plane_destroy(cp);
        se = script_engine_destroy(se);

        return 0;
}
