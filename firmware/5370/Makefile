#
# Makefile for 5370 processor board project
#

#
# This Makefile can be run on both a build machine (I use a MacBook Pro) and the
# BeagleBone Black target (Angstrom or Debian release).
# Which machine you're on is figured out by this:
#
#	DEBIAN_ANGSTROM_DEVSYS = $(shell grep -q -s Debian /etc/dogtag; echo $$?)
#	DEBIAN = 0
#	ANGSTROM = 1
#	DEVSYS = 2
#	ifeq ($(DEBIAN_ANGSTROM_DEVSYS),$(DEBIAN))
#		...
#	ifeq ($(DEBIAN_ANGSTROM_DEVSYS),$(DEVSYS))
#		...
#
# The '/etc/dogtag' file is present on the Beagle and not on the dev machine.
# Grep returns 0 if "Debian" is found in /etc/dogtag, 1 if it isn't and 2 if /etc/dogtag doesn't exist.
# This same mechanism is used in the wrapper shell script because device tree files need to be
# loaded only on the Beagle.
#

VERSION_MAJ = 3
VERSION_MIN = 2

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

DEBIAN_ANGSTROM_DEVSYS = $(shell grep -q -s Debian /etc/dogtag; echo $$?)
DEBIAN = 0
ANGSTROM = 1
DEVSYS = 2

OBJ_DIR = obj
OBJ_DIR_O3 = $(OBJ_DIR)_O3
DIRS = . 5370 6800 arch arch/$(ARCH) pru support user web $(wildcard pkgs/*)
VPATH = $(DIRS)
I = $(addprefix -I,$(DIRS))
H = $(wildcard $(addsuffix /*.h,$(DIRS)))
H += 6800/6800.debug.h
C = $(wildcard $(addsuffix /*.c,$(DIRS)))
CFILES = $(subst web/$(EDATA),,$(C)) web/$(EDATA)

EDATA = embedded_data.c
BINS = hd.bin h.bin bc

#ALL_DEPS = pru/pru_realtime.bin
SRC_DEPS = Makefile pru/pru_realtime.bin $(EDATA)
BIN_DEPS =

ifeq ($(DEBIAN_ANGSTROM_DEVSYS),$(DEVSYS))
	ROOT_DIR = \".\"
endif
ifeq ($(DEBIAN_ANGSTROM_DEVSYS),$(DEBIAN))
	ROOT_DIR = \"/root\"
endif
ifeq ($(DEBIAN_ANGSTROM_DEVSYS),$(ANGSTROM))
	ROOT_DIR = \"/home/root\"
endif

LIBS = -L/usr/local/lib -lpthread
SIM_FLAGS = -D$(INSTRUMENT) -D$(SETUP) -DARCH_INCLUDE=$(ARCH_INCLUDE) -D$(CHIP) -DCHIP_INCLUDE=$(CHIP_INCLUDE) -DPCB_NAME=$(PCB_NAME)
VERSION = -DFIRMWARE_VER_MAJ=$(VERSION_MAJ) -DFIRMWARE_VER_MIN=$(VERSION_MIN)
FLAGS = -Wno-int-to-void-pointer-cast $(I) -I/usr/local/include $(SIM_FLAGS) $(VERSION) -DROOT_DIR=$(ROOT_DIR)
CSRC = $(notdir $(CFILES))
OBJECTS = $(CSRC:%.c=$(OBJ_DIR)/%.o)
O3_OBJECTS = $(CSRC:%.c=$(OBJ_DIR_O3)/%.o)

all: hd.bin h.bin
h: h.bin
hd: hd.bin

# interpreter with debugging enabled
hd.bin:	$(OBJ_DIR) $(OBJECTS) $(BIN_DEPS)
	cc -g $(OBJECTS) $(LIBS) -o $@

# full -O3 optimization
h.bin:	$(OBJ_DIR_O3) $(O3_OBJECTS) $(BIN_DEPS)
	cc -O3 $(O3_OBJECTS) $(LIBS) -o $@

$(OBJ_DIR)/%.o: %.c $(H) $(SRC_DEPS)
	cc -g -DDEBUG -DINSN_TRACE $(FLAGS) -c -o $@ $<

$(OBJ_DIR_O3)/%.o: %.c $(H) $(SRC_DEPS)
	cc -O3 $(FLAGS) -c -o $@ $<

$(OBJ_DIR):
	@mkdir -p $(OBJ_DIR)

$(OBJ_DIR_O3):
	@mkdir -p $(OBJ_DIR_O3)

PASM_SOURCE = $(wildcard pru/pasm/*.c)
PASM_INCLUDES = $(wildcard pru/pasm/*.h)
pas: $(PASM_INCLUDES) $(PASM_SOURCE) Makefile
	cc -Wall -D_UNIX_ -I./pru/pasm $(PASM_SOURCE) -o pas

pru/pru_realtime.bin: pas pru/pru_realtime.p pru/pru_realtime.h pru/pru_realtime.hp
	(cd pru; ../pas -V3 -b -L -l -D_PASM_ -D$(SETUP) pru_realtime.p)

WEB = 5370.html 5370.js
web/$(EDATA): $(addprefix web/,$(WEB))
	(cd web; perl mkdata.pl $(WEB) >$(EDATA))

debug:
	@echo DEBIAN_ANGSTROM_DEVSYS = $(DEBIAN_ANGSTROM_DEVSYS)
	@echo SRC_DEPS: $(SRC_DEPS)
	@echo BIN_DEPS: $(BIN_DEPS)
	@echo ALL_DEPS: $(ALL_DEPS)
	@echo WEB: $(WEB)
	@echo VPATH: $(VPATH)
	@echo H: $(H)
	@echo C: $(C)
	@echo OBJECTS: $(OBJECTS)

DEV = 5370
CAPE = cape-bone-$(DEV)-00A0
PRU  = cape-bone-$(DEV)-P-00A0
TEST = cape-bone-test-00A0

install: hd.bin h.bin
ifeq ($(DEBIAN_ANGSTROM_DEVSYS),$(DEVSYS))
	@echo only run \'make install\' on target
else
	cp h.bin hp5370d
	cp pru/pru_realtime.bin hp5370d_realtime.bin
	install -D -s -o root -g root hp5370d /usr/local/bin/hp5370d
	install -D -o root -g root hp5370d_realtime.bin /usr/local/bin/hp5370d_realtime.bin
	rm -f hp5370d hp5370d_realtime.bin
	install -o root -g root unix_env/hp5370d /etc/init.d
	install -o root -g root unix_env/hp5370d.service /etc/systemd/system
	install -D -o root -g root -m 0644 unix_env/$(CAPE).dts /lib/firmware/$(CAPE).dts
	install -D -o root -g root -m 0644 unix_env/$(PRU).dts /lib/firmware/$(PRU).dts
#	install -D -o root -g root -m 0644 unix_env/$(TEST).dts /lib/firmware/$(TEST).dts
	install -D -o root -g root -m 0644 unix_env/.bashrc ~root/.bashrc
	install -D -o root -g root -m 0644 unix_env/.profile ~root/.profile

# disable display manager (on HDMI port) to improve real-time response and decrease boot time
ifeq ($(DEBIAN_ANGSTROM_DEVSYS),$(DEBIAN))
	systemctl enable hp5370d.service
#	update-rc.d hp5370d defaults
#	update-rc.d hp5370d enable
	update-rc.d lightdm disable
endif
ifeq ($(DEBIAN_ANGSTROM_DEVSYS),$(ANGSTROM))
	systemctl enable hp5370d
	systemctl disable gdm
	/usr/lib/connman/test/set-global-timeservers pool.ntp.org
endif
endif

ifeq ($(DEBIAN_ANGSTROM_DEVSYS),$(DEBIAN))
enable disable start stop status:
	systemctl --lines=100 $@ hp5370d.service
endif
ifeq ($(DEBIAN_ANGSTROM_DEVSYS),$(DEBIAN_USING_UPDATE_RC))
enable disable:
	update-rc.d hp5370d $@
start stop:
	/etc/init.d/hp5370d $@
status:
	@ls -la /etc/rc?.d/*hp5370d
endif
ifeq ($(DEBIAN_ANGSTROM_DEVSYS),$(ANGSTROM))
enable disable start stop status:
	systemctl --lines=100 $@ hp5370d
endif

log:
	journalctl -b | grep 5370

# client code example of transfering HPIB binary data over a TCP connection
BCFILES = 5370/hpib_client.c support/misc.c support/net.c
bc:	$(H) $(BCFILES)
	cc -O3 -DCLIENT_SIDE $(FLAGS) $(BCFILES) $(LIBS) -o $@

v ver version:
	@echo "you are running version" $(VERSION_MAJ)"."$(VERSION_MIN)

DIST = 5370
BBB_IP = $(IP_BBB_5370)

tar:
	make clean
	tar cfz ../$(DIST).tgz ../$(DIST)

scp: tar
	scp ../$(DIST).tgz root@$(BBB_IP):~root

# selectively transfer files to the target so everything isn't compiled each time
EXCLUDE = "/obj" "/obj_O3" "*.dSYM" "*.bin"
rsync:
	rsync -av --delete $(addprefix --exclude , $(EXCLUDE)) . root@$(BBB_IP):~root/$(DIST)

clean:
	-rm -rf $(OBJ_DIR) $(OBJ_DIR_O3) $(BINS) $(wildcard hp5370d*) *.dSYM ../$(DIST).tgz pas $(addprefix pru/pru_realtime.,bin lst txt) web/$(EDATA)
