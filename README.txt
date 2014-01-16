[edited 1/17/2014]

This document describes the HP 5370 processor replacement board project.

        Firmware release:       v3.0, October 25, 2013
        PCB release:            v3.1, October 25, 2013

Version 3.x uses an inexpensive BeagleBone Black (BBB) single-board computer as
the host for the 5370 application code (app). The BBB runs Angstrom Linux from the
2GB on-board eMMC flash memory.


QUICK START

The file "READ_MORE.txt" elaborates on the information given here.

1. If your new board was purchased with the BBB already attached then the 5370 software
is pre-installed and the board is ready to be plugged in. Otherwise see step 8 below
"Installing your own BBB and software" before proceeding.

2. Install the board in your 5370. Remove the old processor board from the far left
A9 slot. The boards can be difficult to remove and install.
Original boards from the instrument have ejector levers to assist with this task.
Grasp the board outer top edges, or use the ejector levers, and try to rock the ends up and
down slightly, one at a time, to get the springs fingers of the connectors to loosen up
rather than pulling the board straight out with a lot of force.
Like the old processor the new one faces component-side towards the inside of the instrument.
The new board is not full height (cost savings) which means the card guides on the edges
don't fully engage. Be sure the board is aligned before pushing it into the connector.

3. The HPIB board in slot A15 can be left installed even if the app option to
divert HPIB transfers to Ethernet is used. If you have an older instrument you can
remove the ROM board from slot A12. You can also leave these boards installed in case
you want to revert to using the original processor board.

4. Although the new board will run the instrument fine without a network connection
you'll probably want one to do any development and also to be able to shutdown the BBB
properly before an instrument power off (see below).
There are several ways to connect the BBB to a network and the easiest is probably
by using a wired Ethernet connection (not included) from the RJ45 jack on the BBB
to a network hub or router port. It is also easiest if your network is running a DHCP server
(usually in the network router) that assigns IP addresses.

5. Be prepared to note the assigned IP address that will be shown on the 5370
front panel display after instrument power on. You'll need this address for a
subsequent command to login to Linux running on the BBB (see below).
After turning on the instrument it will take less than 30 seconds for the BBB
to boot and begin running the 5370 app. The version number of the app will appear on
the display, e.g. "v3.0". If you're running a DHCP server on your network the assigned
IP address will then be shown on the display (e.g. 192.168.1.2). A future app release will
allow the IP address to also be set manually from the front panel. There are four blue
LEDs on the BBB that should blink as booting progresses (fron the "top" edge of the board
down: heartbeat, SD card access, process running, eMMC/filesystem access). If you're
having trouble determining the IP address see step 8 below for some suggestions.

6. Your instrument should now respond as usual although you will notice it is somewhat faster.
Before powering off the instrument it is strongly recommended you first login and halt Linux
and then wait ten seconds to avoid possible unrecoverable filesystem corruption
(although this is unlikely). Definitely do not power off during the booting process when
lots of filesystem writes are occurring.

7. Look at the file "READ_MORE.txt" to learn about how to use, modify and re-compile the app.

8. Installing your own BBB and software:
The software is designed to work with the Angstrom Linux distribution installed on a new BBB.
If you've installed something else (e.g. Ubuntu) then things probably won't work.

Attach the BBB to the board being very careful to observe that the RJ45 Ethernet connector of
the BBB goes over the notch in the white silkscreen outline on the board. The connector
pins are not polarized so don't get this wrong. See photos on http://www.jks.com/5370/5370.html

Install the board in your 5370 as described in step 2 above. Attach an Ethernet cable to the
BBB from your network. Do _not_ attach an external power supply to the BBB 5V barrel connector
as this will conflict with the 5V being supplied from the board via the expansion pins.
However the 5V present on a USB cable, if you're using one, is okay since the BBB knows how
to deal with this case. Power up the 5370. You should observe the 4 LEDs blinking on the BBB
as described in step 5 above.

The IP address assigned to the BBB can be determined by several means: A free app called
"Fing" on iphone/ipad, viewing the IP assignment table maintained by your DHCP server,
using various network scanning tools that are available, etc. Look for the IP address
associated with an Ethernet "MAC" address belonging to the Texas Instruments MAC range
(TI makes the Sitara ARM processor chip used on the BBB).

Fing in particular will automatically lookup the MAC address and show "Texas Instruments"
and also show the Linux hostname which is "beaglebone" unless you've changed it.

You can also use a USB cable to establish a separate "ad-hoc" network connection and then
login to the BBB over than and display the Ethernet IP address.
Also by logging in using a serial port connection. See the READ_MORE.txt file for details.

Now login to the BBB. From the command line of another computer on your network try
"ssh root@ip_address" (e.g. ssh root@192.168.1.2). Just type "return" for the password.
Also see the READ_MORE.txt file and this link for other login suggestions:
http://elinux.org/Beagleboard:Terminal_Shells

Once you're on the BBB test that you have a connection to the Internet.
Type "ping -c 3 google.com" and see that you get a response.

To install the software properly the current time & date must be set.
Type the "date" command to check. If the year is 2000 then set the date manually
"date mmddHHMMyyyy" e.g. date 011700002014
The current Angstrom distribution doesn't do a good job of starting NTP if the Internet
connection establishment is delayed at boot time. Also the NTP configuration file in
/etc/default/ntpdate isn't even setup (NTPSERVERS="" instead of "pool.ntp.org") but the
software installation will fix this.

Now type the following to fetch and install the software:

	cd /home/root (if you're not there already)
	curl -o 5370.tgz jks.com/5370/5370.v3.0.tgz
	tar xfz 5370.tgz
	cd 5370.v3.0
	make install

If there were no errors test the install by running the 5370 app manually.
Just type the command "hd" and then, after the 5370 has responded properly, control-C to stop.
The "make install" above installs the app so it automatically runs when the BBB is
next booted. You can type "reboot" to test this. Remember to type "halt" and wait 10 seconds
before powering off.


[ end of document ]
