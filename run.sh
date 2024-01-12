#!/bin/bash

if [[ $(id -u) -ne 0 ]] ;
then echo "Please run as root" ;
	 exit -1 ;
fi

set +x

DEVICE_SIZE=51200
MOUNTPOINT="/mnt/disdrive"
SOCKET="/tmp/nbdsocket.disdrive"

modprobe nbd

umount --force "$MOUNTPOINT"
mkdir -p "$MOUNTPOINT"
nbd-client -d /dev/nbd0
killall -9 nbdkit
rm -f "$SOCKET"

if [ "$1" = "stop" ]; then
	echo "Stopped."
	df -k "$MOUNTPOINT"
	exit 0
fi

nbdkit -f --verbose -U "$SOCKET" "./disdrive.so" --run "./mount.sh \$unixsocket $MOUNTPOINT"
