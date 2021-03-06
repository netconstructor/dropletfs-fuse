#!/bin/sh
# $FreeBSD: src/tools/regression/fstest/tests/rmdir/03.t,v 1.1 2007/01/17 01:42:11 pjd Exp $

desc="rmdir returns ENAMETOOLONG if an entire path name exceeded 1023 characters"

dir=`dirname $0`
. ${dir}/../misc.sh

echo "1..12"

expect 0 mkdir ${name255} 0755
expect 0 mkdir ${name255}/${name255} 0755
expect 0 mkdir ${name255}/${name255}/${name255} 0755
expect 0 mkdir ${path1021} 0755
expect 0 mkdir ${path1023} 0755
expect 0 rmdir ${path1023}
expect ENOENT rmdir ${path1023}

case "${os}" in
FreeBSD)
        expect ENAMETOOLONG rmdir ${path1024}
        ;;
Linux)
        expect ENAMETOOLONG rmdir ${path4096}
        ;;
esac

expect 0 rmdir ${path1021}
expect 0 rmdir ${name255}/${name255}/${name255}
expect 0 rmdir ${name255}/${name255}
expect 0 rmdir ${name255}
