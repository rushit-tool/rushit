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

#include <stdlib.h>

#include "common.h"
#include "lib.h"


void *buf_alloc(struct options *opts)
{
        size_t alloc_size = opts->request_size;
        void *buf;

        if (alloc_size < opts->response_size)
                alloc_size = opts->response_size;
        if (alloc_size > opts->buffer_size)
                alloc_size = opts->buffer_size;

        buf = calloc(alloc_size, sizeof(char));
        if (!buf)
                return NULL;

        if (opts->enable_write)
                fill_random(buf, alloc_size);

        return buf;
}
