#!/bin/bash -e
# Copyright (c) 2017 John Seamons, ZL/KF6VO

# NB: this distro image is a flasher

VER="v3.5"
DEBIAN_VER="7"
CKSUM="221892a7ea38edf01e27ef5b66810cb07e86d5dffa4686cebe7d0805ca16359f"

# image files are available on both dropbox.com and jks.com
#DISTRO_HOST="http://jks.com/files"
DISTRO_HOST="https://www.dropbox.com/s/bf5yl3qd2tvm216"
DISTRO="5370_${VER}_BBB_Debian_${DEBIAN_VER}.img.xz"

echo "--- get 5370 distro image from net and create micro-SD flasher"
echo -n "--- hit enter when ready:" ; read not_used

rv=$(which xzcat || true)
if test "x$rv" = "x" ; then
	echo "--- get missing xz-utils"
	apt-get -y install xz-utils
fi

if test ! -f ${DISTRO} ; then
	echo "--- getting distro"
	wget ${DISTRO_HOST}/${DISTRO}
else
	echo "--- already seem to have the distro file, verify checksum below to be sure"
fi
echo "--- computing checksum..."
sha256sum ${DISTRO}
echo ${CKSUM} " correct checksum"
echo "--- verify that the two checksums above match"
echo "--- insert micro-SD card"
echo -n "--- hit enter when ready:" ; read not_used
echo "--- lsblk:"
lsblk

echo "--- copying to micro-SD card, will take several minutes"
echo -n "--- hit enter when ready:" ; read not_used
xzcat -v ${DISTRO} | dd of=/dev/mmcblk1

echo "--- when next booted with micro-SD installed, KiwiSDR image should be copied to Beagle eMMC flash"
echo -n "--- hit ^C to skip reboot, else enter when ready to reboot:" ; read not_used

echo "--- rebooting with flasher micro-SD installed will COMPLETELY OVERWRITE THIS BEAGLE's FILESYSTEM!"
echo -n "--- ARE YOU SURE? enter when ready to reboot:" ; read not_used

echo "--- okay, rebooting to re-flash Beagle eMMC flash from micro-SD"
echo "--- you should see a back-and-forth pattern in the LEDs during the copy"
echo "--- after all the LEDs go dark (Beagle is powered off), remove micro-SD and power up"
echo "--- you should now be running KiwiSDR distro"
echo "--- rebooting now..."
reboot
