#!/bin/sh
# wrapper for running the 5370 app

# hack to see if we're running on a BBB
if test ! -d /lib/firmware; then
	./$1
	exit 0
fi

DEV=5370
CAPE=cape-bone-${DEV}-00A0
DEV_PRU=${DEV}-P
PRU=cape-bone-${DEV_PRU}-00A0
SLOTS=`ls /sys/devices/bone_capemgr.*/slots`

# out-of-the-box BBB doesn't seem to have NTP configured
if grep -q 'NTPSERVERS=""' /etc/default/ntpdate ; then
	cp unix_env/ntpdate /etc/default;
fi

if date | grep -q 2000; then (echo start NTP; systemctl reload-or-restart ntpdate); fi

if [ /lib/firmware/${CAPE}.dts -nt /lib/firmware/${CAPE}.dtbo ] ; then
	echo compile ${DEV} device tree;
	(cd /lib/firmware; dtc -O dtb -o ${CAPE}.dtbo -b 0 -@ ${CAPE}.dts);
# don't unload old slot because this is known to cause panics; must reboot
fi

if ! grep -q ${DEV} $SLOTS ; then
	echo load ${DEV} device tree;
	echo cape-bone-${DEV} > $SLOTS;
fi

if [ /lib/firmware/${PRU}.dts -nt /lib/firmware/${PRU}.dtbo ] ; then
	echo compile ${DEV_PRU} device tree;
	(cd /lib/firmware; dtc -O dtb -o ${PRU}.dtbo -b 0 -@ ${PRU}.dts);
# don't unload old slot because this is known to cause panics; must reboot
fi

if ! grep -q ${DEV_PRU} $SLOTS ; then
	echo load ${DEV_PRU} device tree;
	echo cape-bone-${DEV_PRU} > $SLOTS;
fi

./$1
