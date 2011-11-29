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

function check()
{
        expected=$1
        got=$2
        msg=$3

        if [ "$expected" != "$got" ]
        then    die "$msg - expected $expected but got $got"
	fi
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
check 0 $? "raw listing failed"
echo "OK."


### empty file section

file=$(basename `mktemp`)
echo -n "mknod $file... "
touch $file > /dev/null 2>&1
check 0 $? "mknod failed"
echo "OK."

echo -n "check its default size... "
size=$(stat --printf "%s" $file)
check 0 $? "stat failed"
check 0 $size "wrong size"
echo "OK."

echo -n "check its default owner... "
owner=$(stat --printf "%u" $file)
check 0 $? "stat failed"
uid=$(id -u)
check $uid $owner "wrong uid"
echo "OK."

echo -n "check its default group... "
group=$(stat --printf "%g" $file)
check 0 $? "stat failed"
gid=$(id -g)
check $gid $group "wrong gid"
echo "OK."

echo -n "rm this empty file... "
rm $file
check 0 $? "rm $file failed"
echo "OK."


### standard file section

local=$(mktemp)
dd if=/dev/urandom of=$local count=1 bs=20KB > /dev/null
echo -n "copying random binary file $local... "
remote=$(basename $local)
cp $local $remote
check 0 $? "cp failed"
echo "OK."

echo -n "compare the md5... "
local_md5=$(md5sum $local | cut -f1 -d' ')
remote_md5=$(md5sum $remote | cut -f1 -d' ')
check 0 $? "remote md5 failed"
check $local_md5 $remote_md5 "different md5s"
echo "OK."

echo -n "compare the sizes... "
local_size=$(stat --format "%s" $local)
remote_size=$(stat --format "%s" $remote)
check $local_size $remote_size "different sizes"
echo "OK."

echo -n "remove $remote... "
rm $remote
check 0 $? "rm $remote failed"
echo "OK."

echo -n "check that we can execute a remote file... "
echo /bin/ls > $local
chmod +x $local
cp $local $remote

# first, is the x bit set?
local_mode=$(stat --format "%f" $local)
remote_mode=$(stat --format "%f" $remote)
check $local_mode $remote_mode "different modes"
echo "OK."

echo -n "can we remotely execute this file... "
./$remote > /dev/null
check 0 $? "remote execution failed"
echo "OK."

echo -n "append data to the file... "
echo "123" >> $remote
local_size=$(stat --format "%s" $local)
remote_size=$(stat --format "%s" $remote)
expected_size=$(($local_size + 4))
check $expected_size $remote_size "different sizes"
echo "OK"

echo -n "rewrite the file... "
echo "123" > $remote
local_size=$(stat --format "%s" $local)
remote_size=$(stat --format "%s" $remote)
expected_size=4
check $expected_size $remote_size "different sizes"
echo "OK"

# cleanup
rm $local
rm $remote

### dir section

dir=$(basename `mktemp -d`)
echo -n "create the directory $dir... "
mkdir $dir
check 0 $? "mkdir failed"
echo "OK."

echo -n "remove the (empty) directory $dir... "
rmdir $dir
check 0 $? "rmdir failed"
echo "OK."

dir="$(basename `mktemp -d`)/a/b"
echo -n "create a tree $dir..."
mkdir -p $dir
check 0 $? "mkdir -p failed"
echo "OK."

echo -n "remove the tree $dir... "
rmdir -p $dir
check 0 $? "rmdir -p failed"
echo "OK."


# teardown
popd > /dev/null
rmdir "$testdir"
