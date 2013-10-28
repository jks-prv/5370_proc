This document describes the hp5370 processor replacement board project

        firmware release:       v3.0, October 25, 2013
        PCB release:            v3.1, October 25, 2013

Version 3.x uses an inexpensive BeagleBone Black (BBB) single-board computer as
the host for the 5370 application code. The BBB runs Angstrom Linux from the 2GB
on-board eMMC flash memory.


QUICK START

The section "ADDITIONAL DETAILS" below elaborates on the information given here.

Install the board in your 5370: Remove the old processor board from the far left
A9 slot. If you have an older instrument remove the ROM board from slot A12. The
HPIB board can be left installed even if the software option to divert HPIB
transfer to Ethernet is used. The boards can be difficult to install and remove.
Grasp the board outer top edges and try to rock the ends up and down slightly to
get the springs fingers of the connectors to loosen up rather than pulling the
board straight out with a lot of force. Like the old processor the new one faces
component-side towards the inside of the instrument.

The BBB has the 5370 software pre-installed. Currently you must "ssh" into the
BBB over a wired Ethernet connection (login as "root", just hit return for the
password). Plug the Ethernet cable into the RJ45 port on the BBB. Type the
command "t" to run a script which starts the 5370 app. The instrument should now
function as usual.


ADDITIONAL DETAILS

Currently you must login to the BBB and start the 5370 app manually (i.e. the
BBB boots when you power-on the 5370 but the 5370 won't function as an
instrument until the app is started). This will be fixed when I add an install
target to the Makefile and the appropriate entry to /etc/init.d 

The best way to login to the board is by using ssh (e.g. ssh
root@BBB_ip_address). Your network must be running a DHCP server somewhere for
the BBB to obtain its IP address (usually your Internet router or modem). It is
possible to use a USB WiFi dongle under Angstrom but commentary on the net says
this is difficult and I haven't got it working yet. The BBB supplies drivers
that allow networking over a USB cable, but I couldn't get DHCP to work with
this setup (point-to-point IP using default non-routable addresses worked fine).

You also need a way to determine what IP address DHCP has given to the BBB. I
use the iphone/ipad app "Fing" and look for the IP address associated with an
Ethernet MAC address belonging to the Texas Instruments MAC range (TI makes the
Sitara ARM processor chip used on the BBB). The IP address usually remains fixed
once initially assigned. Most DHCP servers even have a way for you to associate
an IP with a particular MAC permanently. Eventually, when the app starts on
power up, the assigned IP address will be shown on the 5370 display briefly.

In the "5370.v3" directory are the full app sources. Also a client-side example
program "bc" you would run on another machine to connect to the 5370 over TCP
and do HPIB-style transfers including regular (12K meas/sec) and a new fast (39K
meas/sec) binary mode. You can build a new app by typing "m" or "make" in this
directory and running "hd" for the debug or "h" for the optimized version of the
app.

The "t" script in the root home directory does a few setup chores before running
the app: if required, a .profile for the shell is installed, ntpd is started and
the 5370 device-tree file is compiled and installed.

Technically you should really issue the "halt" command to Angstrom Linux and
wait until the four blue activity LEDs on the BBB go off before powering the
5370 off (about 10 seconds). In practice I've been able to get away with
skipping this, but the first time you're left with an unrecoverable corrupted
filesystem you'll be very sorry.

We all know stuff breaks when using different versions of things. All my BBBs
have been hardware revision "A5C" boards. The software distribution of Angstrom
Linux I use is "2013.06.20" found by doing:
        mount /dev/mmcblk0p1 /mnt/boot
        more /mnt/boot/ID.txt
Which produces: "Cloud9 GNOME Image 2013.06.20". This distribution uses a Linux
3.8 kernel. See http://circuitco.com/support/index.php?title=BeagleBoneBlack for
the latest information about BBB.

If you're re-installing the BBB be certain to match the outline of the BBB to
the silkscreen outline on the 5370 board. The BBB expansion connectors are not
polarized.


FILES

The files in the github repository are organized as follows:

        firmware/<version>/     C code that runs on the BBB.
        pcb/<version>/          Complete documentation for replicating or 
                                modifying the PCB.
        
        firmware/5370[AB].ROM   Just for reference, packed-binary format copies
                                of the ROM code from version A and B 
                                instruments. The .h files of the ROM code 
                                compiled with the code were derived from these.
        
        firmware/<version>/
                6800/6800*      The Motorola M6800 microprocessor interpreter.
                5370/5370*      Called from the interpreter for 5370 bus I/O.
                5370/hpib*      The HPIB board emulator.
                arch/sitara/*   BBB GPIO-specific generation of 5370 bus cycles.
                support/*       Code to support the 7-seg display, LEDs, etc.
                user/*          An example of code to extend the capabilities of

                                the instrument.
                unix_env/*      A few customization files for Angstrom Linux 
                                including the device-tree file necessary to 
                                setup the GPIO lines. The shell script here 
                                called 't' is what you generally run.
        
        pcb/<version>/
                5370.pro        KiCAD project file
                5370.sch        KiCAD schematic source
                5370.brd        KiCAD PCB layout source
                5370.gvp        Gerbv project file
                5370.BOM.*      Bill-of-materials file: prices, part notes.
                plot/5370-*.*   Gerber plots from KiCAD: 2-layer board, mask, 
                                silkscreen and edge cut files.
                plot/5370.drl   KiCAD drill file: Excellon format, inches,
                                keep zeros, minimal header, absolute origin, 2:4
                plot/makefile   Will zip all the files together.
        
        pcb/data.sheets/        Cut sheets for all components.
        
        pcb/libraries.v1/       KiCAD-format library (schematic) and module 
        pcb/modules.v1/         (footprint) files for all the components used. 
                                No references are made to outside libraries to
                                keep things simple.
                

DESIGN OVERVIEW

The idea is to run the unmodified instrument rom firmware by emulating the
Motorola mc6800 processor used on the original 5370 processor board. The
assumption is that the BeagleBone Black processor
will be able to run the 5370 firmware, with the mc6800 interpreter, fast enough
to outperform the original 1.5 MHz mc6800. This turns out to be the case, even
with bit-banging the 5370 bus cycles using the BBB's GPIOs.

A superior solution would be to disassemble the rom code and re-code it in a
higher level language. This can be done in a mechanical way or with a complete
understanding of the underlying hardware in
order to make the resulting code more understandable. Successful disassembly
also depends on the amount of firmware involved and whether the firmware and
processor architecture are well known. In this case the emulation solution was
used to see if the technique would work and also be applied to other situations.

The same philosophy was applied to the 5370's HPIB controller. Rather than
disassembling the associated firmware and contents of the ROM on the HPIB board,
the bus cycles transferring data to and from the board were observed, an
understanding of the board hardware obtained from looking at the schematics and
then a software emulator was constructed. Now the HPIB board can be removed. The
software then fools the unchanged 5370 firmware into thinking the HPIB board is
still present even though the data now comes from the Ethernet, USB or the
keyboard and display.


KEYBOARD COMMANDS

When the app is running it responds to several commands (type "?" or "help" for
list). These are primarily to assist development when you're not sitting in the
same room as the noisy beast:

        d               show instrument display including units and key LEDs
        h [HPIB cmd]    emulate HPIB command input, e.g. "h md2"
        k [f1 .. f4]    emulate function key 1-4 press, e.g. "k f1" is TI key
        k [g1 .. g4]    emulate gate time key 1-4 press
        k [m1 .. m8]    emulate math key 1-8 press
        k [s1 .. s5]    emulate sample size key 1-5 press
        k [o1 .. o6]    emulate "other" key 1-6 press (see code for mapping)
        m               call measurement extension example code
        q               quit

For DEBUG versions of the app there are some others (see code).


EXPANDING INSTRUMENT FUNCTIONALITY

In the file user/example.c is an example of how you might add some code to be
called when a certain combination of front-panel keys are pressed. The routine
meas_extend_example_init() is called from main.c at app startup time. It calls
register_key_callback() to have the routine meas_extend_example() called
whenever the MEAN and MIN keys are pressed together (or MEAN when LCL/RMT is
used as a "shift"-style key).

Your extension code is being called is when the instrument firmware is at the
point of polling for new key presses. This means that theoretically nothing bad
should happen with your code taking over control of the instrument. You have
access to the routines in support/front_panel.[ch] to control the display and of
course can read and write to the registers of the instrument (see
5370/5370_regs.h).

The example shows how to perform a TI measurement using the same technique as
found in the disassembled HPIB binary mode transfer firmware. The code assumes
you're measuring the internal 10 MHz reference against itself by running a cable
from the freq std output jack at the back of the unit to the start input on the
front.

I believe this code doesn't work for large TI values where the 16-bit N0 counter
overflows (+/- 32K * 5ns = ~163ms = ~6Hz). But it's a simple matter to sample
the overflow flag in N0ST and compensate (a message is printed currently).


[ end of document ]
