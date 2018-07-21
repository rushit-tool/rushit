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

#ifndef NEPER_SAMPLE_H
#define NEPER_SAMPLE_H

#include <stdio.h>
#include <sys/resource.h>
#include <sys/time.h>

struct callbacks;
struct flow;
struct numlist;
struct percentiles;

struct sample {
        int tid;                    /* Thread identifier. */
        int flow_id;                /* Flow (connection) identifier. */
        ssize_t bytes_read;         /* Count of bytes read (client only). */
        unsigned long transactions; /* Count of reads (client) or writes (server). */
        struct numlist *latency;    /* Time from write to read for each transaction. */
        struct timespec timestamp;  /* When sample was collected. */
        struct rusage rusage;       /* RUSAGE_THREAD stats at time of collection. */
        struct sample *next;
};

void add_sample(int tid, struct flow *flow, struct timespec *ts,
                struct sample **samples, struct callbacks *cb);

void print_sample(FILE *csv, struct percentiles *percentiles,
                  struct sample *sample);
void print_samples(struct percentiles *percentiles, struct sample *samples,
                   int num, const char *filename, struct callbacks *cb);
int compare_samples(const void *a, const void *b);
void free_samples(struct sample *samples);

#endif
