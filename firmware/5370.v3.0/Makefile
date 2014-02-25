# Makefile for compiling 5370 project

VERSION_MAJ = 3
VERSION_MIN = 0

#INSTRUMENT = HP5370A
INSTRUMENT = HP5370B
#INSTRUMENT = HP5359A

# Chip & board & setup used for compilation
#SETUP = SETUP_SAM7X_EVAL_KIT
#SETUP = SETUP_TIME_NUTS_PCB_V1
#SETUP = SETUP_TIME_NUTS_PCB_V2
SETUP = SETUP_TIME_NUTS_PCB_V3

ifeq ($(SETUP), SETUP_SAM7X_EVAL_KIT)
ARCH = sam7x
ARCH_INCLUDE = \"sam7x.h\"
CHIP = at91sam7x256
CHIP_INCLUDE = \"AT91SAM7X256.h\"
BOARD = at91sam7x-ek
PHY = PHY_DM9161A
PCB_NAME = \"AT91SAM7X-EK\"
endif

ifeq ($(SETUP), SETUP_TIME_NUTS_PCB_V1)
ARCH = sam7x
ARCH_INCLUDE = \"sam7x.h\"
CHIP = at91sam7x512
CHIP_INCLUDE = \"AT91SAM7X512.h\"
BOARD = at91sam7x-ek
PHY = PHY_DM9161
PCB_NAME = \"TIME-NUTS_PCB\"
endif

ifeq ($(SETUP), SETUP_TIME_NUTS_PCB_V2)
ARCH = stm32f4
ARCH_INCLUDE = \"stm32f4.h\"
CHIP = stm32f407
CHIP_INCLUDE = \"stm32f407.h\"
BOARD = fixme_new_board
PHY = fixme_new_phy
PCB_NAME = \"TIME-NUTS_PCB\"
endif

ifeq ($(SETUP), SETUP_TIME_NUTS_PCB_V3)
ARCH = sitara
ARCH_INCLUDE = \"sitara.h\"
CHIP = AM3359
CHIP_INCLUDE = \"AM3359.h\"
PCB_NAME = \"TIME-NUTS_PCB\"
endif

SIM_FLAGS = -D$(INSTRUMENT) -D$(SETUP) -DARCH_INCLUDE=$(ARCH_INCLUDE) -D$(CHIP) -DCHIP_INCLUDE=$(CHIP_INCLUDE) -DPCB_NAME=$(PCB_NAME)
VERSION = -DFIRMWARE_VER_MAJ=$(VERSION_MAJ) -DFIRMWARE_VER_MIN=$(VERSION_MIN)

VPATH = .
VPATH += ./5370
VPATH += ./6800
VPATH += ./arch
VPATH += ./arch/$(ARCH)
VPATH += ./support
VPATH += ./misc
VPATH += ./user
VPATH += ./pru ./pru/pasm

INC = $(addprefix -I, $(VPATH))

RMT_SOURCE = 5370/5370.c 5370/test.c 5370/hpib.c 5370/bug.c main.c 6800/6800.c 6800/6800.trace.c
RMT_SOURCE += arch/$(ARCH)/bus.c arch/$(ARCH)/timer.c
RMT_SOURCE += support/misc.c support/net.c support/front_panel.c user/example.c
RMT_SOURCE += pru/prussdrv.c pru/pru_realtime.c

RMT_INCLUDES = config.h boot.h sim.h
RMT_INCLUDES += 5370.h 5370_regs.h hpib.h 6800.h 6800.cc.h 6800.core.h 6800.insns.h
RMT_INCLUDES += arch/chip.h arch/$(ARCH)/$(CHIP).h arch/$(ARCH)/$(ARCH).h arch/$(ARCH)/bus.h arch/$(ARCH)/timer.h
RMT_INCLUDES += support/misc.h support/net.h support/front_panel.h
RMT_INCLUDES += $(wildcard pru/*.h)

RMT_BINARIES = hd.bin h.bin bc
RMT_DEPENDS = $(RMT_SOURCE) $(RMT_INCLUDES) Makefile 6800.debug.c pru/pru_realtime.bin
RMT_FLAGS = $(SIM_FLAGS) $(VERSION) -Wno-int-to-void-pointer-cast -lpthread
RMT_DIRS += -L/usr/local/lib -I/usr/local/include $(INC)

all: hd.bin h.bin
h: h.bin
hd: hd.bin

# interpreter with debugging enabled
hd.bin:	$(RMT_DEPENDS)
	cc -DDEBUG -DINSN_TRACE -g $(RMT_FLAGS) $(RMT_SOURCE) $(RMT_DIRS) -o $@

# full -O3 optimization
h.bin:	$(RMT_DEPENDS)
	cc -O3 $(RMT_FLAGS) $(RMT_SOURCE) $(RMT_DIRS) -o $@

PASM_SOURCE = $(wildcard pru/pasm/*.c)
PASM_INCLUDES = $(wildcard pru/pasm/*.h)
pas:	$(PASM_INCLUDES) $(PASM_SOURCE) Makefile
	cc -Wall -D_UNIX_ -I./pru/pasm $(PASM_SOURCE) -o pas

pru/pru_realtime.bin:	pas pru/pru_realtime.p pru/pru_realtime.h pru/pru_realtime.hp
	(cd pru; ../pas -V3 -b -L -l -D_PASM_ -D$(SETUP) pru_realtime.p)

CAPE = cape-bone-5370-00A0
PRU  = cape-bone-5370-P-00A0
TEST = cape-bone-test-00A0

install: hd.bin h.bin
	cp h.bin hp5370d
	cp pru/pru_realtime.bin hp5370d_realtime.bin
	install -D -s -o root -g root hp5370d /usr/local/bin/hp5370d
	install -D -o root -g root hp5370d_realtime.bin /usr/local/bin/hp5370d_realtime.bin
	rm hp5370d hp5370d_realtime.bin
	install -o root -g root unix_env/hp5370d /etc/init.d
	install -o root -g root unix_env/hp5370d.service /etc/systemd/system
	install -D -o root -g root -m 0644 unix_env/$(CAPE).dts /lib/firmware/$(CAPE).dts
	install -D -o root -g root -m 0644 unix_env/$(PRU).dts /lib/firmware/$(PRU).dts
#	install -D -o root -g root -m 0644 unix_env/$(TEST).dts /lib/firmware/$(TEST).dts
	/usr/lib/connman/test/set-global-timeservers pool.ntp.org
	install -D -o root -g root -m 0644 unix_env/.bashrc /home/root/.bashrc
	install -D -o root -g root -m 0644 unix_env/.profile /home/root/.profile
	systemctl enable hp5370d
# disable Gnome display manager (on HDMI port) to improve real-time response and decrease boot time
	systemctl disable gdm

enable disable start stop status:
	systemctl --lines=100 $@ hp5370d

log:
	journalctl -b | grep 5370

# client code example of transfering HPIB binary data over a TCP connection
bc:	$(RMT_DEPENDS) 5370/hpib_client.c
	cc -O3 -DCLIENT_SIDE $(RMT_FLAGS) 5370/hpib_client.c support/misc.c support/net.c $(RMT_DIRS) -o $@

v ver version:
	@echo "you are running version" $(VERSION_MAJ)"."$(VERSION_MIN)

DIST = 5370.v$(VERSION_MAJ).$(VERSION_MIN)

tar:
	make clean
	@rm -f ../5370
	@ln -s $(DIST) ../5370
	tar cfz ../$(DIST).tgz ../$(DIST) ../5370
	@rm -f ../5370.tgz
	@ln -s $(DIST).tgz ../5370.tgz

BBB_IP = $(IP_BBB_5370)

scp: tar
	scp ../$(DIST).tgz root@$(BBB_IP):/home/root

clean:
	-rm -rf $(RMT_BINARIES) $(wildcard hp5370d*) *.dSYM ../$(DIST).tgz ../5370.tgz pas $(addprefix pru/pru_realtime., bin lst txt)
