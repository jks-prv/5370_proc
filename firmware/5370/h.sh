#!/bin/sh
# wrapper for running the 5370 app

DEV=5370

# hack to see if we're running on a BBB
if test ! -f /etc/dogtag; then
	$*
	exit 0
fi

SLOTS="/sys/devices/platform/bone_capemgr/slots"
if test ! -f ${SLOTS} ; then
	echo DEBIAN 7
	SLOTS=`ls /sys/devices/bone_capemgr.*/slots`
else
	echo DEBIAN 8
	echo But Debian 8 doesn\'t work yet because the loading of the SPI driver in /boot/uEnv.txt
	echo needs to be commented out. Also, our Debian 7 PRU interface is broken by the ti kernels
	echo used in Debian 8. So for now downgrade to Debian 7 by installing the canned system image
	echo that is known to work \(see jks.com/5370, same strategy as the KiwiSDR project\).
	exit 1
fi

DEVID=cape-bone-${DEV}

DEV_PRU=${DEV}-P
DEVID_PRU=cape-bone-${DEV_PRU}

CAPE=${DEVID}-00A0
PRU=${DEVID_PRU}-00A0

# cape
if test \( ! -f /lib/firmware/${CAPE}.dtbo \) -o \( /lib/firmware/${CAPE}.dts -nt /lib/firmware/${CAPE}.dtbo \) ; then
	echo compile ${DEV} device tree;
	(cd /lib/firmware; dtc -O dtb -o ${CAPE}.dtbo -b 0 -@ ${CAPE}.dts);
# don't unload old slot because this is known to cause panics; must reboot
fi

if ! grep -q ${DEVID} ${SLOTS} ; then
	echo load ${DEV} device tree;
	echo ${DEVID} > ${SLOTS};
fi

# PRU
if test \( ! -f /lib/firmware/${PRU}.dtbo \) -o \( /lib/firmware/${PRU}.dts -nt /lib/firmware/${PRU}.dtbo \) ; then
	echo compile ${DEV_PRU} device tree;
	(cd /lib/firmware; dtc -O dtb -o ${PRU}.dtbo -b 0 -@ ${PRU}.dts);
# don't unload old slot because this is known to cause panics; must reboot
fi

if ! grep -q ${DEVID_PRU} ${SLOTS} ; then
	echo load ${DEV_PRU} device tree;
	echo ${DEVID_PRU} > ${SLOTS};
fi

$*
