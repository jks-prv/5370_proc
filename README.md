[edited 20-october-2017]

The HP 5370 processor replacement board project
-----------------------------------------------

Version 3.x of the project uses an inexpensive BeagleBone Black (BBB) single-board computer as the host for the 5370 application code (app). The BBB runs Linux from the on-board eMMC flash memory, either the Angstrom distribution on the rev B BBBs with 2GB of eMMC or Debian 7 on the rev C BBBs with 4GB of eMMC (the less expensive BB Green also works).

**Important note (2017)**: The most recently manufactured Beagles ship with Debian 8 distributions that are incompatible with the 5370 software. The software installation instructions below will describe a procedure to re-flash the memory of the Beagle with a compatible version of Debian 7 together with the 5370 software.


### Quick Start

The file [READ_MORE.md](http://github.com/jks-prv/5370_proc/blob/master/READ_MORE.md) elaborates on the information given here.

1. If your new board was purchased with the BBB already attached then the 5370 software is pre-installed and the board is ready to be plugged in. Otherwise see the section below "Installing your own BBB and software" before proceeding. _Note: we no longer sell the Beagle / 5370 board combo. You must supply your own Beagle, either Black or Green._

2. Install the board in your 5370. Remove the old processor board from the far left A9 slot. The boards can be difficult to remove and install. Original boards from the instrument have ejector levers to assist with this task. Grasp the board outer top edges, or use the ejector levers, and try to rock the ends up and down slightly, one at a time, to get the springs fingers of the connectors to loosen up rather than pulling the board straight out with a lot of force. Like the old processor the new one faces component-side towards the inside of the instrument. The new board is not full height (cost savings) which means the card guides on the edges don't fully engage. Be sure the board is aligned before pushing it into the connector.

3. The HPIB board in slot A15 can be left installed even if the app option to divert HPIB transfers to the network is used. If you have an older instrument you can remove the ROM board from slot A12. You can also leave these boards installed in case you want to revert to using the original processor board.

4. Although the new board will run the instrument fine without a network connection you'll probably want one to do any development. There are several ways to connect the BBB to a network and the easiest is probably by using an Ethernet cable (not included) from the RJ45 jack on the BBB to a network hub or router port. It is also easiest if your network is running a DHCP server (usually in the network router) that assigns IP addresses.

5. Be prepared to note the assigned IP address that will be shown on the 5370 front panel display after instrument power on. You'll need this address for a subsequent command to login to Linux running on the BBB (see below). After turning on the instrument it will take less than 30 seconds for the BBB to boot and begin running the 5370 app. The version number of the app will appear on the display, e.g. "v3.2". If you're running a DHCP server on your network the assigned IP address will then be shown on the display (e.g. 192.168.1.2). If not, a default IP address will be shown that can be changed via the front panel as described in the [READ_MORE.md](http://github.com/jks-prv/5370_proc/blob/master/READ_MORE.md) file. There are four blue LEDs on the BBB that should blink as booting progresses (fron the "top" edge of the board down: heartbeat, SD card access, process running, eMMC/filesystem access). If you're having trouble determining the IP address see the software installation section below for some suggestions.

6. Your instrument should now respond as usual although you will notice it is somewhat faster. Before powering off the instrument it is strongly recommended you first halt Linux to avoid possible unrecoverable filesystem corruption (although this is unlikely). Definitely do not power off during the booting process when lots of filesystem writes are occurring. You can halt by using the front panel menu interface or by logging into the BBB via ssh and typing 'halt'. When halting from the front-panel menu the message 'halting...' will appear and it will be safe to power-off the instrument after the display goes blank. It is also possible to keep the BBB running by using a USB-mini cable from an external hub or USB charger to keep it powered up, even with the instrument powered down. This has the advantage of providing 'instant on' when the instrument is next powered on (i.e. no Linux booting required). See the [READ_MORE.md](http://github.com/jks-prv/5370_proc/blob/master/READ_MORE.md) file for details. 

7. Look at the file [READ_MORE.md](http://github.com/jks-prv/5370_proc/blob/master/READ_MORE.md) to learn about how to use, modify and re-compile the app. This file also discusses using "git" to track changes to the released software.


### Installing your own BBB and software

The software is designed to work with the Angstrom or Debian 7 Linux distribution installed on on older BBBs. If you've installed something else (e.g. Ubuntu) the 5370 software will not work, re-flash as described below. For newer BBBs running Debian 8 re-flash as described below.

There are two possibilities.

* If the BBB is running Angstrom or Debian 7 the 5370 software alone can be installed from a downloaded "tar" archive file.

* If the distribution on the BBB is Debian 8, or cannot be determined, the BBB can be re-flashed from a downloaded image file that is then copied onto a micro-SD card and plugged into the BBB.

Follow the steps below for each case.

#### Installing the 5370 software alone under Angstrom or Debian 7

Install the 5370 software on the BBB before it is attached to the 5370 board and placed in the instrument. This is because if you decide that you need to re-flash the board, as described in the second section below, it will be much easier to access the micro-SD card slot if the Beagle is not attached to the 5370 board and is outside of the instrument.

Attach an Ethernet cable to the BBB from your network. Apply 5V power to the BBB. You should observe the 4 LEDs blinking on the BBB.

The IP address assigned to the BBB by DHCP can be determined by several means: A free app called "Fing" on iphone/ipad, viewing the IP assignment table maintained by your DHCP server, using various network scanning tools that are available, etc. Look for the IP address associated with an Ethernet "MAC" address belonging to the Texas Instruments MAC range (TI makes the Sitara ARM processor chip used on the BBB).

Fing in particular will automatically lookup the MAC address and show "Texas Instruments" and also show the Linux hostname which is "beaglebone" unless you've changed it. Most routers have a browser interface that can show a table of DHCP information.

You could also decide to assign a fixed address to the instrument outside of the range of addresses managed by DHCP. Later, after the 5370 card is installed in the instrument, set this address by using the front panel menu interface as described in the [READ_MORE.md](http://github.com/jks-prv/5370_proc/blob/master/READ_MORE.md) file.

Also possible is using a USB cable to establish a separate "ad-hoc" network connection and then logging in to the BBB over that and displaying the Ethernet IP address. Also by logging in using a serial port connection. See the [READ_MORE.md](http://github.com/jks-prv/5370_proc/blob/master/READ_MORE.md) file for details.

Now login to the BBB. From the command line of another computer on your network try "ssh root@ip\_address" (e.g. ssh root@192.168.1.2). Just type "return" for the password. Also see the [READ\_MORE.md](http://github.com/jks-prv/5370_proc/blob/master/READ_MORE.md) file and this link for other login suggestions: http://elinux.org/Beagleboard:Terminal_Shells

For Debian you can determine if the distribution is incompatible with the 5370 software as follows. After logging in type the shell command "cat /etc/dogtag". You should see something like _BeagleBoard.org Debian Image 2016-05-13_. If the date is anything later than this you are running Debian 8 and should re-flash as described in the next section.

Now test that you have a connection to the Internet. Type "ping -c 3 google.com" and see that you get a response.

To install the software properly the current time & date must be set. Type the "date" command to check. If the current date is not shown then set the date manually (the value of HHMM doesn't really matter):

	"date mmddHHMMyyyy"		(e.g. "date 032900002017" for March 29, 2017)

The current Angstrom distribution doesn't have NTP configured properly by default but the software installation will fix this.

Now type the following to fetch and install the software:

	cd  (takes you to "/home/root" if you're not there already)
	curl -o 5370.tgz www.jks.com/5370/5370.tgz
	tar xfz 5370.tgz
	cd 5370
	make install

If there are no errors type "halt" to shutdown the Beagle. Remove the 5V power and Ethernet connections.

Attach the BBB to the board being very careful to observe that the RJ45 Ethernet connector of the BBB goes over the notch in the white silkscreen outline on the board. The connector pins are not polarized so don't get this wrong. See photos on [jks.com/5370/5370.html](http://www.jks.com/5370/5370.html)

Install the board in your 5370 as described in step 2 above. Do _not_ attach an external power supply to the BBB 5V barrel connector as this will conflict with the 5V being supplied from the board via the expansion pins. However the 5V present on a USB cable, if you're using one, is okay since the BBB knows how to select between the two power sources. Power up the 5370. The instrument should respond as described in steps 6 and 7 above.

This completes the software install.


#### Re-flashing the BBB with a complete distribution including the 5370 software

A large image file containing a complete Debian distribution for the Beagle including the working 5370 software can be downloaded and placed on a micro-SD card. This SD card can then be plugged into the Beagle and used to re-flash.

This process is described fully in a section of the KiwiSDR project documentation: [Optionally downloading the software](http://kiwisdr.com/quickstart/index.html#id-dload). However, instead of the image files and scripts for the KiwiSDR specified in that documentation use the ones below.

* Mac & Windows:
 * Download this 235 MB .img.xz file to your Mac/PC
(SHA256: 221892a7ea38edf01e27ef5b66810cb07e86d5dffa4686cebe7d0805ca16359f)
  * [dropbox.com](https://www.dropbox.com/s/t2dkk2k4ebuqdmi/5370_v3.5_BBB_Debian_7.img.xz?dl=1)

* Beagle / Debian Linux:
 * Get install script by typing one of the following:
  * wget [http://jks.com/files/5370-download-5370-create-micro-SD-flasher.sh](http://jks.com/files/5370-download-5370-create-micro-SD-flasher.sh)
  * wget [https://www.dropbox.com/s/e62h414qxlr0ty5/5370-download-5370-create-micro-SD-flasher.sh](https://www.dropbox.com/s/e62h414qxlr0ty5/5370-download-5370-create-micro-SD-flasher.sh?dl=1)

After the re-flashing process remove the 5V power and Ethernet connections.

Attach the BBB to the board being very careful to observe that the RJ45 Ethernet connector of the BBB goes over the notch in the white silkscreen outline on the board. The connector pins are not polarized so don't get this wrong. See photos on [jks.com/5370/5370.html](http://www.jks.com/5370/5370.html)

Install the board in your 5370 as described in step 2 above. Do _not_ attach an external power supply to the BBB 5V barrel connector as this will conflict with the 5V being supplied from the board via the expansion pins. However the 5V present on a USB cable, if you're using one, is okay since the BBB knows how to select between the two power sources. Power up the 5370. The instrument should respond as described in steps 6 and 7 above.

This completes the software install.

[ end of document ]
