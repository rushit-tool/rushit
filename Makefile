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


CFLAGS = -std=c99 -Wall -Werror -O3 -g -D_GNU_SOURCE

staging-dir := staging

luajit-dir := vendor/luajit.org/luajit-2.1
luajit-lib := $(staging-dir)/lib/libluajit-5.1.a

base-lib := \
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
	thread.o \
	version.o

all-libs := $(base-lib) $(luajit-lib)

tcp_rr-objs := tcp_rr_main.o tcp_rr.o $(all-libs)
tcp_stream-objs := tcp_stream_main.o tcp_stream.o $(all-libs)
dummy_test-objs := dummy_test_main.o dummy_test.o $(all-libs)

binaries := tcp_rr tcp_stream dummy_test

ext-libs := -lm -lpthread -lrt

tcp_rr: $(tcp_rr-objs)
	$(CC) -o $@ $^ $(ext-libs)

tcp_stream: $(tcp_stream-objs)
	$(CC) -o $@ $^ $(ext-libs)

dummy_test: $(dummy_test-objs)
	$(CC) -o $@ $^ $(ext-libs)

all: $(binaries)

clean: clean-luajit
	rm -f *.o $(binaries)
	rm -rf $(staging-dir)

.PHONY: luajit
luajit: $(luajit-lib)

$(luajit-lib):
	$(MAKE) -C $(luajit-dir) PREFIX=$(abspath $(staging-dir))
	$(MAKE) -C $(luajit-dir) PREFIX=$(abspath $(staging-dir)) install

clean-luajit:
	$(MAKE) -C $(luajit-dir) clean
