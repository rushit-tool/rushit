/*
 * Copyright 2018 Red Hat, Inc.
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
 * Tests for statistics calculation from samples collected during workload run.
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <float.h>
#include <math.h>

#include "common.h"
#include "lib.h"
#include "sample.h"
#include "thread.h"
#include "workload.h"


#define SAMPLE(thread_id, flow_id_, timestamp_sec, bytes_read_) \
        {                                                       \
                .tid = thread_id,                               \
                .flow_id = flow_id_,                            \
                .bytes_read = bytes_read_,                      \
                .timestamp = { .tv_sec = timestamp_sec },       \
        }


#define INVALID_STATS                                   \
        {                                               \
                .num_samples = -1,                      \
                .throughput = NAN,                      \
                .correlation_coefficient = NAN,         \
                .end_time = { -1, -1 }                  \
        }

#define FAKE_CALLBACKS(stats)                   \
        {                                       \
                .logger = stats,                \
                .print = mock_log_value,        \
                .log_fatal = mock_log_msg,      \
                .log_error = mock_log_msg,      \
                .log_warn = mock_log_msg,       \
                .log_info = mock_log_msg,       \
        }

#define FAKE_OPTIONS(num_threads_)              \
        {                                       \
                .num_threads = num_threads_,    \
        }

#define FAKE_THREAD(callbacks, options, samples_)       \
        {                                               \
                .cb = callbacks,                        \
                .opts = options,                        \
                .samples = samples_,                    \
        }

#define TIMESPEC(sec, nsec) (struct timespec) { .tv_sec = sec, .tv_nsec = nsec }


enum {
        THREAD_0 = 0,
        THREAD_1 = 1,
};

enum {
        FLOW_1 = 1,
        FLOW_2 = 2,
};


static void _assert_dbl_equal(double a, double b,
                              const char *file, const int line)
{
        double delta = DBL_EPSILON;

        if (isnan(a) && isnan(b))
                return;
        if (fabs(a - b) < delta)
                return;

        print_error("|%f - %f| >= %f\n", a, b, delta);
        _fail(file, line);
}
#define assert_dbl_equal(a, b) _assert_dbl_equal(a, b, __FILE__, __LINE__)

static void _assert_tv_equal(const struct timespec *a, const struct timespec *b,
                             const char *file, const int line)
{
        _assert_int_equal(a->tv_sec, b->tv_sec, file, line);
        _assert_int_equal(b->tv_sec, b->tv_sec, file, line);
}
#define assert_tv_equal(a, b) _assert_tv_equal(a, b, __FILE__, __LINE__)

static void link_samples(struct sample *samples, size_t n_samples)
{
        size_t i;

        if (n_samples == 0)
                return;

        for (i = 0; i < n_samples-1; i++)
                samples[i].next = &samples[i+1];
        samples[i].next = NULL;
}

static void mock_log_value(void *logger, const char *key, const char *fmt, ...)
{
        struct stats *stats = logger;
        va_list ap;

        va_start(ap, fmt);
        if (!strcmp(key, "start_index") ||
            !strcmp(key, "end_index")) {
                /* Ignored, deprecated. */
        } else if (!strcmp(key, "num_samples")) {
                stats->num_samples = va_arg(ap, int);
        } else if (!strcmp(key, "throughput_Mbps")) {
                stats->throughput = va_arg(ap, double);
        } else if (!strcmp(key, "correlation_coefficient")) {
                stats->correlation_coefficient = va_arg(ap, double);
        } else if (!strcmp(key, "time_end")) {
                stats->end_time.tv_sec = va_arg(ap, time_t);
                stats->end_time.tv_nsec = va_arg(ap, long);
        } else {
                fail_msg("Unexpected key '%s'", key);
        }
        va_end(ap);
}

static void mock_log_msg(void *logger, const char *file, int line,
                         const char *function, const char *fmt, ...)
{
        va_list argp;

        UNUSED(logger);
        UNUSED(file);
        UNUSED(line);
        UNUSED(function);

        va_start(argp, fmt);
        vfprintf(stderr, fmt, argp);
        va_end(argp);
        fputc('\n', stderr);
}


static void t_stream_stats_zero_samples(void **state)
{
        const int num_threads = 1;
        struct sample *samples = NULL;

        struct stats stats = INVALID_STATS;
        struct callbacks cb = FAKE_CALLBACKS(&stats);
        struct options opts = FAKE_OPTIONS(num_threads);
        struct thread thread = FAKE_THREAD(&cb, &opts, samples);
        UNUSED(state);

        report_stream_stats(&thread);
        assert_int_equal(-1, stats.num_samples);
        assert_dbl_equal(NAN, stats.throughput);
        assert_dbl_equal(NAN, stats.correlation_coefficient);
        assert_tv_equal(&TIMESPEC(-1, -1), &stats.end_time);
}

static void t_stream_stats_one_sample(void **state)
{
        const int num_threads = 1;
        struct sample samples[] = {
                SAMPLE(THREAD_0, FLOW_1, 0, 0),
        };

        struct stats stats = INVALID_STATS;
        struct callbacks cb = FAKE_CALLBACKS(&stats);
        struct options opts = FAKE_OPTIONS(num_threads);
        struct thread thread = FAKE_THREAD(&cb, &opts, samples);
        UNUSED(state);

        report_stream_stats(&thread);
        assert_int_equal(1, stats.num_samples);
        assert_dbl_equal(0.0, stats.throughput);
        assert_dbl_equal(0.0, stats.correlation_coefficient);
        assert_tv_equal(&TIMESPEC(0, 0), &stats.end_time);
}

static void t_stream_stats_one_thread_one_flow_two_samples(void **state)
{
        const int num_threads = 1;
        struct sample samples[] = {
                SAMPLE(THREAD_0, FLOW_1, 0, 0),
                SAMPLE(THREAD_0, FLOW_1, 1, 1000/8 * 1000000),
        };
        link_samples(samples, ARRAY_SIZE(samples));

        struct stats stats = INVALID_STATS;
        struct callbacks cb = FAKE_CALLBACKS(&stats);
        struct options opts = FAKE_OPTIONS(num_threads);
        struct thread thread = FAKE_THREAD(&cb, &opts, samples);
        UNUSED(state);

        report_stream_stats(&thread);
        assert_int_equal(2, stats.num_samples);
        assert_dbl_equal(1000.0, stats.throughput);
        assert_dbl_equal(1.0, stats.correlation_coefficient);
        assert_tv_equal(&TIMESPEC(1, 0), &stats.end_time);
}

static void t_stream_stats_one_thread_one_flow_three_samples(void **state)
{
        const int num_threads = 1;
        struct sample samples[] = {
                SAMPLE(THREAD_0, FLOW_1, 0, 0),
                SAMPLE(THREAD_0, FLOW_1, 1, 400/8 * 1000000),
                SAMPLE(THREAD_0, FLOW_1, 2, 800/8 * 1000000),
        };
        link_samples(samples, ARRAY_SIZE(samples));

        struct stats stats = INVALID_STATS;
        struct callbacks cb = FAKE_CALLBACKS(&stats);
        struct options opts = FAKE_OPTIONS(num_threads);
        struct thread thread = FAKE_THREAD(&cb, &opts, samples);
        UNUSED(state);

        report_stream_stats(&thread);
        assert_int_equal(3, stats.num_samples);
        assert_dbl_equal(400.0, stats.throughput);
        assert_dbl_equal(1.0, stats.correlation_coefficient);
        assert_tv_equal(&TIMESPEC(2, 0), &stats.end_time);
}

static void t_stream_stats_one_thread_two_flows_four_samples(void **state)
{
        const int num_threads = 1;
        struct sample samples[] = {
                SAMPLE(THREAD_0, FLOW_1, 0, 0),
                SAMPLE(THREAD_0, FLOW_2, 1, 240/8 * 1000000),
                SAMPLE(THREAD_0, FLOW_1, 2, 480/8 * 1000000),
                SAMPLE(THREAD_0, FLOW_2, 3, 720/8 * 1000000),
        };
        link_samples(samples, ARRAY_SIZE(samples));

        struct stats stats = INVALID_STATS;
        struct callbacks cb = FAKE_CALLBACKS(&stats);
        struct options opts = FAKE_OPTIONS(num_threads);
        struct thread thread = FAKE_THREAD(&cb, &opts, samples);
        UNUSED(state);

        report_stream_stats(&thread);
        assert_int_equal(4, stats.num_samples);
        assert_dbl_equal(400.0, stats.throughput);
        assert_tv_equal(&TIMESPEC(3, 0), &stats.end_time);
        /*
         * FIXME: Correlation coefficient calculation for multiple flows is
         * broken. Two flows that have equal and constant pace have perfect
         * correlation (r = 1) because you can draw a straight line through all
         * samples.
         */
        // assert_dbl_equal(1.0, stats.correlation_coefficient);
}

static void t_stream_stats_two_threads_two_flows_four_samples(void **state)
{
        const int num_threads = 2;
        struct sample samples[2][2] = {
                {
                        SAMPLE(THREAD_0, FLOW_1, 0, 0),
                        SAMPLE(THREAD_0, FLOW_1, 1, 200/8 * 1000000),
                }, {
                        SAMPLE(THREAD_1, FLOW_1, 0, 0),
                        SAMPLE(THREAD_1, FLOW_1, 1, 200/8 * 1000000),
                },
        };
        link_samples(samples[0], ARRAY_SIZE(samples[0]));
        link_samples(samples[1], ARRAY_SIZE(samples[1]));

        struct stats stats = INVALID_STATS;
        struct callbacks cb = FAKE_CALLBACKS(&stats);
        struct options opts = FAKE_OPTIONS(num_threads);
        struct thread threads[] = {
                FAKE_THREAD(&cb, &opts, samples[0]),
                FAKE_THREAD(&cb, &opts, samples[1]),
        };
        UNUSED(state);

        report_stream_stats(threads);
        assert_int_equal(4, stats.num_samples);
        assert_dbl_equal(400.0, stats.throughput);
        assert_tv_equal(&TIMESPEC(1, 0), &stats.end_time);
        /*
         * FIXME: Correlation coefficient calculation for multiple flows is
         * broken. Two flows that have equal and constant pace have perfect
         * correlation (r = 1) because you can draw a straight line through all
         * samples.
         */
        // assert_dbl_equal(1.0, stats.correlation_coefficient);
}

int main(void)
{
        const struct CMUnitTest tests[] = {
                cmocka_unit_test(t_stream_stats_zero_samples),
                cmocka_unit_test(t_stream_stats_one_sample),
                cmocka_unit_test(t_stream_stats_one_thread_one_flow_two_samples),
                cmocka_unit_test(t_stream_stats_one_thread_one_flow_three_samples),
                cmocka_unit_test(t_stream_stats_one_thread_two_flows_four_samples),
                cmocka_unit_test(t_stream_stats_two_threads_two_flows_four_samples),
        };

        return cmocka_run_group_tests(tests, NULL, NULL);
}
