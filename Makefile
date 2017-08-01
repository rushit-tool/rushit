# Copyright 2016 Google Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
################################################################################
#
# Makefile.

# Recommended flags (may be overridden by the user from environment/command line)
CPPFLAGS =
CFLAGS   = -std=c99 -Wall -Werror -O3 -g
LDFLAGS  =
LDLIBS   =

# Madatory flags (required for proper compilation)
OUR_CPPFLAGS = -D_GNU_SOURCE -I$(top-dir) -I$(luajit-inc)
OUR_CFLAGS   =
OUR_LDFLAGS  =
OUR_LDLIBS   = -ldl -lm -lpthread -lrt

# Merged flags
ALL_CPPFLAGS = $(OUR_CPPFLAGS) $(CPPFLAGS)
ALL_CFLAGS   = $(OUR_CFLAGS) $(CFLAGS)
ALL_LDFLAGS  = $(OUR_LDFLAGS) $(LDFLAGS)
ALL_LDLIBS   = $(OUR_LDLIBS) $(LDLIBS)

# Directory containing this Makefile
top-dir := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
staging-dir := $(top-dir)/staging

luajit-dir := $(top-dir)/vendor/luajit.org/luajit-2.1
luajit-inc := $(staging-dir)/include/luajit-2.1
luajit-lib := $(staging-dir)/lib/libluajit-5.1.a

base-objs := \
	common.o \
	control_plane.o \
	cpuinfo.o \
	flags.o \
	flow.o \
	hexdump.o \
	interval.o \
	logging.o \
	numlist.o \
	percentiles.o \
	sample.o \
	script.o \
	thread.o \
	version.o
base-libs := $(luajit-lib)
base-deps := $(base-objs) $(base-libs)

tcp_rr-objs := tcp_rr_main.o tcp_rr.o
tcp_stream-objs := tcp_stream_main.o tcp_stream.o
dummy_test-objs := dummy_test_main.o dummy_test.o

binaries := tcp_rr tcp_stream dummy_test

default: all

.c.o:
	$(CC) -c $(ALL_CPPFLAGS) $(ALL_CFLAGS) $< -o $@


tcp_rr: $(tcp_rr-objs) $(base-deps)
	$(CC) -o $@ $^ $(ALL_CFLAGS) $(ALL_LDFLAGS) $(ALL_LDLIBS)

tcp_stream: $(tcp_stream-objs) $(base-deps)
	$(CC) -o $@ $^ $(ALL_CFLAGS) $(ALL_LDFLAGS) $(ALL_LDLIBS)

dummy_test: $(dummy_test-objs) $(base-deps)
	$(CC) -o $@ $^ $(ALL_CFLAGS) $(ALL_LDFLAGS) $(ALL_LDLIBS)

all: $(binaries)

# Clean up just the files that are most likely to change. That is,
# exclude the dependencies living under vendor/.
clean:
	rm -f *.o $(test-dir)/*.o $(binaries) $(test-binaries)

# Clean up all files, even those that you usually don't want to
# rebuild. That is, include the dependencies living under vendor/.
superclean: clean clean-luajit
	rm -rf $(staging-dir)

.PHONY: all clean superclean

#
# LuaJIT
# TODO: Move it to its own Makefile?
#

build-luajit: $(luajit-lib)

$(luajit-inc): $(luajit-lib)

$(luajit-lib):
	$(MAKE) -C $(luajit-dir) PREFIX=$(staging-dir)
	$(MAKE) -C $(luajit-dir) PREFIX=$(staging-dir) install

clean-luajit:
	$(MAKE) -C $(luajit-dir) clean

.PHONY: build-luajit clean-luajit

#
# Tests
#

test-dir := $(top-dir)/tests/unit

test-libs := $(shell pkg-config --libs cmocka)

test-binaries := t_script

t_script-objs := $(test-dir)/t_script.o

t_script: $(t_script-objs) $(base-deps)
	$(CC) -o $@ $^ $(ALL_CFLAGS) $(ALL_LDFLAGS) $(ALL_LDLIBS) $(test-libs)

tests: $(test-binaries)

.PHONY: tests
