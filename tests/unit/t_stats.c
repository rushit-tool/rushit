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
#include <time.h>

#include "common.h"
#include "lib.h"
#include "sample.h"
#include "thread.h"
#include "workload.h"


#define SAMPLE(thread_id, flow_id_, timestamp_, bytes_read_)    \
        (struct sample) {                                       \
                .tid = thread_id,                               \
                .flow_id = flow_id_,                            \
                .bytes_read = bytes_read_,                      \
                .timestamp = {                                  \
                        .tv_sec = (timestamp_)->tv_sec,         \
                        .tv_nsec = (timestamp_)->tv_nsec,       \
                },                                              \
        }

#define INVALID_STATS                                   \
        {                                               \
                .num_samples = -1,                      \
                .throughput = NAN,                      \
                .correlation_coefficient = NAN,         \
                .end_time = { -1, -1 }                  \
        }

#define FAKE_THREAD(samples_)                           \
        {                                               \
                .samples = samples_,                    \
        }

#define TIMESPEC(sec, nsec)                     \
        (struct timespec) {                     \
                .tv_sec = sec,                  \
                .tv_nsec = nsec,                \
        }

#define TIMESPEC_OFF(t0, t1_sec)                        \
        (struct timespec) {                             \
                .tv_sec = (t0)->tv_sec + t1_sec,        \
                .tv_nsec = (t0)->tv_nsec                \
        }




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

static void t_stream_stats_zero_samples(void **state)
{
        const int num_threads = 1;
        struct sample *samples = NULL;

        struct stats stats = INVALID_STATS;
        struct thread thread = FAKE_THREAD(samples);
        UNUSED(state);

        calculate_stream_stats(&thread, num_threads, &stats, NULL);
        assert_int_equal(0, stats.num_samples);
        assert_dbl_equal(0.0, stats.throughput);
        assert_dbl_equal(0.0, stats.correlation_coefficient);
        assert_tv_equal(&TIMESPEC(0, 0), &stats.end_time);
}

static void t_stream_stats_one_sample(void **state)
{
        const struct timespec *t0 = *state;
        const int num_threads = 1;

        struct timespec *t[] = {
                &TIMESPEC_OFF(t0, 0),
        };
        struct sample samples[] = {
                SAMPLE(THREAD_0, FLOW_1, t[0], 0),
        };

        struct stats stats = INVALID_STATS;
        struct thread thread = FAKE_THREAD(samples);
        UNUSED(state);

        calculate_stream_stats(&thread, num_threads, &stats, NULL);
        assert_int_equal(1, stats.num_samples);
        assert_dbl_equal(0.0, stats.throughput);
        assert_dbl_equal(0.0, stats.correlation_coefficient);
        assert_tv_equal(t[0], &stats.end_time);
}

static void t_stream_stats_one_thread_one_flow_two_samples(void **state)
{
        const struct timespec *t0 = *state;
        const int num_threads = 1;

        struct timespec *t[] = {
                &TIMESPEC_OFF(t0, 0),
                &TIMESPEC_OFF(t0, 1),
        };
        struct sample samples[] = {
                SAMPLE(THREAD_0, FLOW_1, t[0], 0),          /* 0 sec, 0 GB */
                SAMPLE(THREAD_0, FLOW_1, t[1], 1000000000), /* 1 sec, 1 GB */
        };
        link_samples(samples, ARRAY_SIZE(samples));

        struct stats stats = INVALID_STATS;
        struct thread thread = FAKE_THREAD(samples);
        UNUSED(state);

        calculate_stream_stats(&thread, num_threads, &stats, NULL);
        assert_int_equal(2, stats.num_samples);
        assert_dbl_equal(1e9, stats.throughput);
        assert_dbl_equal(1.0, stats.correlation_coefficient);
        assert_tv_equal(t[1], &stats.end_time);
}

static void t_stream_stats_one_thread_one_flow_three_samples(void **state)
{
        const struct timespec *t0 = *state;
        const int num_threads = 1;

        struct timespec *t[] = {
                &TIMESPEC_OFF(t0, 0),
                &TIMESPEC_OFF(t0, 1),
                &TIMESPEC_OFF(t0, 2),
        };
        struct sample samples[] = {
                SAMPLE(THREAD_0, FLOW_1, t[0], 0),          /* 0 sec, 0 GB */
                SAMPLE(THREAD_0, FLOW_1, t[1], 1000000000), /* 1 sec, 1 GB */
                SAMPLE(THREAD_0, FLOW_1, t[2], 2000000000), /* 2 sec, 2 GB */
        };
        link_samples(samples, ARRAY_SIZE(samples));

        struct stats stats = INVALID_STATS;
        struct thread thread = FAKE_THREAD(samples);
        UNUSED(state);

        calculate_stream_stats(&thread, num_threads, &stats, NULL);
        assert_int_equal(3, stats.num_samples);
        assert_dbl_equal(1e9, stats.throughput);
        assert_dbl_equal(1.0, stats.correlation_coefficient);
        assert_tv_equal(t[2], &stats.end_time);
}

static void t_stream_stats_one_thread_two_flows_four_samples(void **state)
{
        const struct timespec *t0 = *state;
        const int num_threads = 1;

        struct timespec *t[] = {
                &TIMESPEC_OFF(t0, 0),
                &TIMESPEC_OFF(t0, 1),
                &TIMESPEC_OFF(t0, 2),
                &TIMESPEC_OFF(t0, 3),
        };
        struct sample samples[] = {
                SAMPLE(THREAD_0, FLOW_1, t[0], 0),          /* 0 sec, 0.0 GB, flow #1 */
                SAMPLE(THREAD_0, FLOW_2, t[1], 1500000000), /* 1 sec, 1.5 GB, flow #2 */
                SAMPLE(THREAD_0, FLOW_1, t[2], 3000000000), /* 2 sec, 3.0 GB, flow #1 */
                SAMPLE(THREAD_0, FLOW_2, t[3], 6000000000), /* 3 sec, 3.0 GB, flow #2 */
        };
        link_samples(samples, ARRAY_SIZE(samples));

        struct stats stats = INVALID_STATS;
        struct thread thread = FAKE_THREAD(samples);
        UNUSED(state);

        calculate_stream_stats(&thread, num_threads, &stats, NULL);
        assert_int_equal(4, stats.num_samples);
        assert_dbl_equal(3e9, stats.throughput);
        assert_tv_equal(t[3], &stats.end_time);
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
        const struct timespec *t0 = *state;
        const int num_threads = 2;

        struct timespec *t[] = {
                &TIMESPEC_OFF(t0, 0),
                &TIMESPEC_OFF(t0, 1),
        };
        struct sample samples[2][2] = {
                {
                        SAMPLE(THREAD_0, FLOW_1, t[0], 0),          /* 0 sec, 0 GB */
                        SAMPLE(THREAD_0, FLOW_1, t[1], 1000000000), /* 1 sec, 1 GB */
                }, {
                        SAMPLE(THREAD_1, FLOW_1, t[0], 0),          /* 0 sec, 0 GB */
                        SAMPLE(THREAD_1, FLOW_1, t[1], 1000000000), /* 1 sec, 1 GB */
                },
        };
        link_samples(samples[0], ARRAY_SIZE(samples[0]));
        link_samples(samples[1], ARRAY_SIZE(samples[1]));

        struct stats stats = INVALID_STATS;
        struct thread threads[] = {
                FAKE_THREAD(samples[0]),
                FAKE_THREAD(samples[1]),
        };
        UNUSED(state);

        calculate_stream_stats(threads, num_threads, &stats, NULL);
        assert_int_equal(4, stats.num_samples);
        assert_dbl_equal(2e9, stats.throughput);
        assert_tv_equal(t[1], &stats.end_time);
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
        struct timespec t0;
        int rc;

        rc = clock_gettime(CLOCK_MONOTONIC, &t0);
        assert_return_code(rc, errno);

        const struct CMUnitTest tests[] = {
                cmocka_unit_test(t_stream_stats_zero_samples),
                cmocka_unit_test_prestate(t_stream_stats_one_sample, &t0),
                cmocka_unit_test_prestate(t_stream_stats_one_thread_one_flow_two_samples, &t0),
                cmocka_unit_test_prestate(t_stream_stats_one_thread_one_flow_three_samples, &t0),
                cmocka_unit_test_prestate(t_stream_stats_one_thread_two_flows_four_samples, &t0),
                cmocka_unit_test_prestate(t_stream_stats_two_threads_two_flows_four_samples, &t0),
        };

        return cmocka_run_group_tests(tests, NULL, NULL);
}
