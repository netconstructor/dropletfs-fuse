#!/bin/sh

# consider the fuse partition already mounted
# if no argument, then we assumre $cwd is in this very mountpoint
# otherwise, the parameter is the path to this mountpoint
MNT=$(pwd)

function die()
{
    echo
    echo "$1"
    exit -1
}

if [ "$1" != "" ]
then
    MNT="$1"
fi

# setup
testdir="$MNT/$(basename `mktemp -d`)"
mkdir "$testdir"
pushd "$testdir"



echo -n "test basic listing..."
/bin/ls > /dev/null 2>&1
[ "0" == "$?" ] || die "raw listing failed"
echo "OK."


### empty file section

file=$(basename `mktemp`)
echo -n "mknod $file... "
touch $file > /dev/null 2>&1
[ "0" = "$?" ] || die "mknod failed"
echo "OK."

echo -n "check its default size... "
size=$(stat --printf "%s" $file)
[ "0" = "$?" ] || die "stat failed"
[ "0" = "$size" ] || die "got size $size while 0 was expected"
echo "OK."

echo -n "check its default owner... "
owner=$(stat --printf "%u" $file)
[ "0" = "$?" ] || die "stat failed"
uid=$(id -u)
[ "$uid" = "$owner" ] || die "got uid $owner while $uid was expected"
echo "OK."

echo -n "check its default group... "
group=$(stat --printf "%g" $file)
[ "0" = "$?" ] || die "stat failed"
gid=$(id -g)
[ "$gid" = "$group" ] || die "got gid $group while $gid was expected"
echo "OK."

echo -n "rm this empty file... "
rm $file
[ "0" = "$?" ] || die "rm $file failed"
echo "OK."


### standard file section

echo -n "copying /etc/hosts... "
local=/etc/hosts
remote=$(basename $local)
cp $local $remote
[ "0" = "$?" ] || die "cp failed"
echo "OK."

echo -n "compare the md5... "
local_md5=$(md5sum $local | cut -f1 -d' ')
remote_md5=$(md5sum $remote | cut -f1 -d' ')
[ "0" = "$?" ] || die "remote md5 failed"
[ "$local_md5" = "$remote_md5" ] || die "local (${local_md5}) and remote (${remote_md5}) md5 differ"
echo "OK."

echo -n "compare the sizes... "
local_size=$(stat --format "%s" $local)
remote_size=$(stat --format "%s" $remote)
[ "$local_size" = "$remote_size" ] || die "local and remote size differ"
echo "OK."

echo -n "remove $remote... "
rm $remote
[ "0" = "$?" ] || die "rm $remote failed"
echo "OK."


### dir section

dir=$(basename `mktemp -d`)
echo -n "create the directory $dir... "
mkdir $dir
[ "0" = "$?" ] || die "mkdir failed"
echo "OK."

echo -n "remove the (empty) directory $dir... "
rmdir $dir
[ "0" = "$?" ] || die "rmdir failed"
echo "OK."

dir="$(basename `mktemp -d`)/a/b"
echo -n "create a tree... $dir "
mkdir -p $dir
[ "0" = "$?" ] || die "mkdir -p failed"
echo "OK."

echo -n "remove the tree $dir... "
rmdir -p $dir
[ "0" = "$?" ] || die "rmdir -p failed"
echo "OK."


# teardown
popd > /dev/null
rmdir "$testdir"
