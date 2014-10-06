[edited 6-oct-2014]

This document describes the HP 5370 processor replacement board project.

Version 3.x uses an inexpensive BeagleBone Black (BBB) single-board computer as the host for the 5370 application code (app). The BBB runs Linux from the on-board eMMC flash memory, either the Angstrom distribution on the rev B BBBs with 2GB of eMMC or Debian on the rev C BBBs with 4GB of eMMC.


QUICK START

The file "READ_MORE.txt" elaborates on the information given here.
github.com/jks-prv/5370_proc/blob/master/READ_MORE.txt

1. If your new board was purchased with the BBB already attached then the 5370 software is pre-installed and the board is ready to be plugged in. Otherwise see step 8 below "Installing your own BBB and software" before proceeding.

2. Install the board in your 5370. Remove the old processor board from the far left A9 slot. The boards can be difficult to remove and install. Original boards from the instrument have ejector levers to assist with this task. Grasp the board outer top edges, or use the ejector levers, and try to rock the ends up and down slightly, one at a time, to get the springs fingers of the connectors to loosen up rather than pulling the board straight out with a lot of force. Like the old processor the new one faces component-side towards the inside of the instrument. The new board is not full height (cost savings) which means the card guides on the edges don't fully engage. Be sure the board is aligned before pushing it into the connector.

3. The HPIB board in slot A15 can be left installed even if the app option to divert HPIB transfers to the network is used. If you have an older instrument you can remove the ROM board from slot A12. You can also leave these boards installed in case you want to revert to using the original processor board.

4. Although the new board will run the instrument fine without a network connection you'll probably want one to do any development. There are several ways to connect the BBB to a network and the easiest is probably by using a wired Ethernet connection (not included) from the RJ45 jack on the BBB to a network hub or router port. It is also easiest if your network is running a DHCP server (usually in the network router) that assigns IP addresses.

5. Be prepared to note the assigned IP address that will be shown on the 5370 front panel display after instrument power on. You'll need this address for a subsequent command to login to Linux running on the BBB (see below). After turning on the instrument it will take less than 30 seconds for the BBB to boot and begin running the 5370 app. The version number of the app will appear on the display, e.g. "v3.2". If you're running a DHCP server on your network the assigned IP address will then be shown on the display (e.g. 192.168.1.2). If not a default IP address will be shown that can be changed via the front panel as described in the READ_MORE.txt file. There are four blue LEDs on the BBB that should blink as booting progresses (fron the "top" edge of the board down: heartbeat, SD card access, process running, eMMC/filesystem access). If you're having trouble determining the IP address see step 8 below for some suggestions.

6. Your instrument should now respond as usual although you will notice it is somewhat faster. Before powering off the instrument it is strongly recommended you first halt Linux and then wait ten seconds to avoid possible unrecoverable filesystem corruption (although this is unlikely). Definitely do not power off during the booting process when lots of filesystem writes are occurring. You can halt by using the front panel menu interface or by logging into the BBB via ssh and typing 'halt'. It is also possible to keep the BBB running by using a USB-mini cable from an external hub or USB charger to keep it powered up, even with the instrument powered down. This has the advantage of providing 'instant on' when the instrument is next powered on (i.e. no Linux booting required). See the READ_MORE.txt file for details. 

7. Look at the file READ_MORE.txt to learn about how to use, modify and re-compile the app. This file also discusses using "git" to track changes to the released software.


8. Installing your own BBB and software:
The software is designed to work with the Angstrom or Debian Linux distribution installed on a new BBB. If you've installed something else (e.g. Ubuntu) then things probably won't work.

Attach the BBB to the board being very careful to observe that the RJ45 Ethernet connector of the BBB goes over the notch in the white silkscreen outline on the board. The connector pins are not polarized so don't get this wrong. See photos on http://www.jks.com/5370/5370.html

Install the board in your 5370 as described in step 2 above. Attach an Ethernet cable to the BBB from your network. Do _not_ attach an external power supply to the BBB 5V barrel connector as this will conflict with the 5V being supplied from the board via the expansion pins. However the 5V present on a USB cable, if you're using one, is okay since the BBB knows how to select between the two power sources. Power up the 5370. You should observe the 4 LEDs blinking on the BBB as described in step 5 above.

The IP address assigned to the BBB by DHCP can be determined by several means: A free app called "Fing" on iphone/ipad, viewing the IP assignment table maintained by your DHCP server, using various network scanning tools that are available, etc. Look for the IP address associated with an Ethernet "MAC" address belonging to the Texas Instruments MAC range (TI makes the Sitara ARM processor chip used on the BBB).

Fing in particular will automatically lookup the MAC address and show "Texas Instruments" and also show the Linux hostname which is "beaglebone" unless you've changed it. Most routers have a browser interface that can show a table of DHCP information.

You could also decide to assign a fixed address to the instrument outside of the range of addresses managed by DHCP. Set this address by using the front panel menu interface as described in the READ_MORE.txt file.

Also possible is using a USB cable to establish a separate "ad-hoc" network connection and then logging in to the BBB over that and displaying the Ethernet IP address. Also by logging in using a serial port connection. See the READ_MORE.txt file for details.

Now login to the BBB. From the command line of another computer on your network try "ssh root@ip_address" (e.g. ssh root@192.168.1.2). Just type "return" for the password. Also see the READ_MORE.txt file and this link for other login suggestions: http://elinux.org/Beagleboard:Terminal_Shells

Once you're on the BBB test that you have a connection to the Internet. Type "ping -c 3 google.com" and see that you get a response.

To install the software properly the current time & date must be set. Type the "date" command to check. If the year is 2000 then set the date manually (the value of HHMM doesn't really matter):
	"date mmddHHMMyyyy"		(e.g. date 011700002014)
The current Angstrom distribution doesn't have NTP configured properly by default but the software installation will fix this.

Now type the following to fetch and install the software:

	cd  (takes you to "/home/root" if you're not there already)
	curl -o 5370.tgz www.jks.com/5370/5370.tgz
	tar xfz 5370.tgz
	cd 5370
	make install

If there were no errors test the install by running the 5370 app manually. Just type the command "./hd" and then, after the 5370 seems to be working properly, control-C to stop. The "make install" above installs the app so it automatically runs when the BBB is next booted. You can type "reboot" to test this.

[ end of document ]
