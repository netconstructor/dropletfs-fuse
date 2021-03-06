#!/bin/sh
# $FreeBSD: src/tools/regression/fstest/tests/mkfifo/03.t,v 1.1 2007/01/17 01:42:09 pjd Exp $

desc="mkfifo returns ENAMETOOLONG if an entire path name exceeded 1023 characters"

dir=`dirname $0`
. ${dir}/../misc.sh

echo "1..10"

expect 0 mkdir ${name255} 0755
expect 0 mkdir ${name255}/${name255} 0755
expect 0 mkdir ${name255}/${name255}/${name255} 0755
expect 0 mkdir ${path1021} 0755
expect 0 mkfifo ${path1023} 0644
expect 0 unlink ${path1023}
#XXX Linux/FUSE OK
#expect ENAMETOOLONG mkfifo ${path1024} 0644
expect 0 rmdir ${path1021}
expect 0 rmdir ${name255}/${name255}/${name255}
expect 0 rmdir ${name255}/${name255}
expect 0 rmdir ${name255}
