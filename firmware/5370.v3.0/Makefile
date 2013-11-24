# Makefile for compiling 5370 project

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
CHIP = am3359
CHIP_INCLUDE = \"AM3359.h\"
PCB_NAME = \"TIME-NUTS_PCB\"
endif

SIM_FLAGS = -D$(INSTRUMENT) -D$(SETUP) -DARCH_INCLUDE=$(ARCH_INCLUDE) -D$(CHIP) -DCHIP_INCLUDE=$(CHIP_INCLUDE) -DPCB_NAME=$(PCB_NAME)

VPATH += ./5370
VPATH += ./6800
VPATH += ./arch/$(ARCH)
VPATH += ./support
VPATH += ./misc
VPATH += ./user

INC =  -I.
INC += -I./5370
INC += -I./6800
INC += -I./arch
INC += -I./arch/$(ARCH)
INC += -I./support
INC += -I./misc
INC += -I./user

RMT_SOURCE = 5370/5370.c 5370/test.c 5370/hpib.c 5370/bug.c main.c 6800/6800.c 6800/6800.trace.c
RMT_SOURCE += arch/$(ARCH)/bus.c arch/$(ARCH)/timer.c
RMT_SOURCE += support/misc.c support/net.c support/front_panel.c user/example.c

RMT_INCLUDES = config.h boot.h sim.h
RMT_INCLUDES += 5370.h 5370_regs.h hpib.h 6800.h 6800.cc.h 6800.core.h 6800.insns.h
RMT_INCLUDES += arch/chip.h arch/$(ARCH)/$(ARCH).h arch/$(ARCH)/timer.h
RMT_INCLUDES += support/misc.h support/net.h support/front_panel.h

RMT_BINARIES = hd h bc
RMT_DEPENDS = $(RMT_SOURCE) $(RMT_INCLUDES) Makefile 6800.debug.c
RMT_FLAGS = $(SIM_FLAGS)
RMT_DIRS += -L/usr/local/lib -I/usr/local/include $(INC)

all: hd h

# interpreter with debugging enabled
hd:	$(RMT_DEPENDS)
	cc -DDEBUG -DINSN_TRACE -g $(RMT_FLAGS) $(RMT_SOURCE) $(RMT_DIRS) -o $@

# full -O3 optimization
h:	$(RMT_DEPENDS)
	cc -O3 $(RMT_FLAGS) $(RMT_SOURCE) $(RMT_DIRS) -o $@

# client code example of transfering HPIB binary data over a TCP connection
bc:	$(RMT_DEPENDS) 5370/hpib_client.c
	cc -O3 -DCLIENT_SIDE $(RMT_FLAGS) 5370/hpib_client.c support/misc.c support/net.c $(RMT_DIRS) -o $@

clean:
	-rm -rf $(RMT_BINARIES) *.dSYM
