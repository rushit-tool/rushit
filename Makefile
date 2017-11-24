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
OUR_LDFLAGS  = -L$(staging-dir)/lib -Wl,-E
OUR_LDLIBS   = -ldl -lm -lpthread -lrt
OUR_LDLIBS  += -lluajit-5.1
OUR_LDLIBS  += -Wl,--whole-archive -lljsyscall -Wl,--no-whole-archive

# Merged flags
ALL_CPPFLAGS = $(OUR_CPPFLAGS) $(CPPFLAGS)
ALL_CFLAGS   = $(OUR_CFLAGS) $(CFLAGS)
ALL_LDFLAGS  = $(OUR_LDFLAGS) $(LDFLAGS)
ALL_LDLIBS   = $(OUR_LDLIBS) $(LDLIBS)

# Directory containing this Makefile
top-dir := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
staging-dir := $(top-dir)/staging

# Packaging related - the version number is specified only inside the spec file
VERSION		:= $(shell awk '/%define rushit_version /{print $$NF}' rushit.spec)
DIST_ARCHIVES	:= rushit-$(VERSION).tar.gz
RPMBUILD_TOP	:= ${top-dir}/rpm/

luajit-dir := $(top-dir)/vendor/luajit.org/luajit-2.1
luajit-inc := $(staging-dir)/include/luajit-2.1
luajit-lib := $(staging-dir)/lib/libluajit-5.1.a
luajit-exe := $(staging-dir)/bin/luajit-2.1.0-beta3
luajit-jit := $(staging-dir)/share/luajit-2.1.0-beta3/jit/

ljsyscall-dir  := $(top-dir)/vendor/github.com/justincormack/ljsyscall
ljsyscall-srcs := $(shell find $(ljsyscall-dir)/syscall.lua $(ljsyscall-dir)/syscall -name '*.lua')
ljsyscall-objs := $(patsubst %.lua,%.o,$(ljsyscall-srcs))
ljsyscall-lib  := $(staging-dir)/lib/libljsyscall.a

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
	script_prelude.o \
	serialize.o \
	thread.o \
	version.o \
	workload.o

tcp_rr-objs := tcp_rr_main.o tcp_rr.o
tcp_stream-objs := tcp_stream_main.o tcp_stream.o
dummy_test-objs := dummy_test_main.o dummy_test.o
udp_stream-objs := udp_stream_main.o udp_stream.o

binaries := tcp_rr tcp_stream dummy_test udp_stream

default: all

# avoid dep geneation on clean targets
# FIXME: on make 4.2 the expression evaluate uncorrectly with a full 'clean'
# pattern is this a make bug?
ifneq (,$(findstring $(MAKECMDGOALS),clea))
-include $(base-objs:.o=.d)
-include $(tcp_rr-objs:.o=.d)
-include $(tcp_stream-objs:.o=.d)
-include $(dummy_test-objs:.o=.d)
endif

%.o: %.c
	$(CC) -c $(ALL_CPPFLAGS) $(ALL_CFLAGS) $< -o $@

%.o: %.lua
	$(luajit-exe) -b -t o -n $(basename $<) $< $@

%.d: %.c
	@$(CC) -M $(ALL_CPPFLAGS) $< | \
	sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' > $@;

$(base-objs): $(luajit-lib)
$(binaries) $(test-binaries): $(base-objs) $(luajit-lib) $(ljsyscall-lib)

tcp_rr: $(tcp_rr-objs)
	$(CC) -o $@ $^ $(ALL_CFLAGS) $(ALL_LDFLAGS) $(ALL_LDLIBS)

tcp_stream: $(tcp_stream-objs)
	$(CC) -o $@ $^ $(ALL_CFLAGS) $(ALL_LDFLAGS) $(ALL_LDLIBS)

dummy_test: $(dummy_test-objs)
	$(CC) -o $@ $^ $(ALL_CFLAGS) $(ALL_LDFLAGS) $(ALL_LDLIBS)

udp_stream: $(udp_stream-objs)
	$(CC) -o $@ $^ $(ALL_CFLAGS) $(ALL_LDFLAGS) $(ALL_LDLIBS)

all: $(binaries)

# beware: dist and rpm target work only inside a git tree
dist:
	git archive --output=$(DIST_ARCHIVES) --prefix=rushit-$(VERSION)/ HEAD

rpm: dist
	mkdir -p ${RPMBUILD_TOP}/SOURCES
	cp ${DIST_ARCHIVES} ${RPMBUILD_TOP}/SOURCES
	rpmbuild -D "_topdir ${RPMBUILD_TOP}" -ba rushit.spec

# Clean up just the files that are most likely to change. That is,
# exclude the dependencies living under vendor/.
clean: clean-tests
	rm -f *.[do] $(binaries) $(DIST_ARCHIVES)

# Clean up all files, even those that you usually don't want to
# rebuild. That is, include the dependencies living under vendor/.
distclean: clean clean-luajit clean-ljsyscall
	rm -rf $(staging-dir)
	rm -rf rpm/

.PHONY: all clean distclean dist rpm

#
# LuaJIT
# TODO: Move it to its own Makefile?
#

# this implies also luajit-inc and luajit-exe; don't list them explicitly
# to avoid issues on parallel builds
$(luajit-lib):
	$(MAKE) -C $(luajit-dir) PREFIX=$(staging-dir)
	$(MAKE) -C $(luajit-dir) PREFIX=$(staging-dir) install
	# module search dir is not configurable, workaround with a symlink
	ln -s $(luajit-jit) jit
	rm $(staging-dir)/lib/*.so*

clean-luajit:
	$(MAKE) -C $(luajit-dir) clean
	rm -f jit

.PHONY: clean-luajit

#
# ljsyscall
#

$(ljsyscall-objs): $(luajit-lib)

$(ljsyscall-dir)/%.o: $(ljsyscall-dir)/%.lua
	$(luajit-exe) -b -t o -n $(subst /,.,$(subst $(ljsyscall-dir)/,,$(basename $<))) $< $@

$(ljsyscall-lib): $(ljsyscall-objs)
	$(AR) cr $@ $^

clean-ljsyscall:
	$(RM) $(ljsyscall-lib) $(ljsyscall-objs)

.PHONY: clean-ljsyscall

#
# Tests
#

test-dir       := $(top-dir)/tests
func-test-dir  := $(test-dir)/func
unit-test-dir  := $(test-dir)/unit
unit-test-libs := $(shell pkg-config --libs cmocka)

t_script-objs := $(unit-test-dir)/t_script.o
test-binaries := $(unit-test-dir)/t_script

-include $(t_script-objs:.o=.d)

$(unit-test-dir)/t_script: $(t_script-objs)
	$(CC) $(ALL_CPPFLAGS) $(ALL_CFLAGS) -o $@ $^ $(ALL_LDFLAGS) $(ALL_LDLIBS) $(unit-test-libs)

$(test-binaries): $(base-objs) $(luajit-lib) $(ljsyscall-lib)

build-tests: $(test-binaries)

clean-tests:
	$(RM) -f $(unit-test-dir)/*.[do] $(test-binaries)

check-unit: $(test-binaries)
	$(unit-test-dir)/t_script

check-func: dummy_test tcp_stream tcp_rr
	$(func-test-dir)/0001
	$(func-test-dir)/0002
	$(func-test-dir)/0003
	$(func-test-dir)/0004
	$(func-test-dir)/0005
	$(func-test-dir)/0006
	$(func-test-dir)/0007
	$(func-test-dir)/0008

check: check-unit check-func

.PHONY: build-tests clean-tests check-unit check-func check
