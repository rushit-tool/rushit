#!/bin/bash
#
# Check if build succeeds with all user settable flags overriden
#

basedir=$(dirname "$0")
rootdir=$basedir/../..

make -C $rootdir clean
make -C $rootdir CPPFLAGS= CFLAGS= LDFLAGS=
