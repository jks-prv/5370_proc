#!/bin/sh

DEV=5370
CAPE=cape-bone-${DEV}-00A0
SLOTS=`ls /sys/devices/bone_capemgr.*/slots`

echo $1
if test ! -d /lib/firmware; then
	./$1
	exit 0
fi

if test ! -f .profile; then
        cp unix_env/.profile .;
fi

# out-of-the-box BBB doesn't seem to have NTP configured
if grep -q 'NTPSERVERS=""' /etc/default/ntpdate ; then
	cp unix_env/ntpdate /etc/default;
fi

if date | grep -q 2000; then (echo start NTP; systemctl reload-or-restart ntpdate); fi

# setup device tree before running interpreter
if test ! -f /lib/firmware/${CAPE}.dts; then
	echo create ${DEV} device tree;
	cp unix_env/${CAPE}.dts /lib/firmware;
	(cd /lib/firmware; dtc -O dtb -o ${CAPE}.dtbo -b 0 -@ ${CAPE}.dts);
fi

if ! grep -q ${DEV} $SLOTS ; then
	echo load ${DEV} device tree;
	echo cape-bone-${DEV} > $SLOTS;
fi

if [ $DEV != "test" ] ; then
	./$1
fi
