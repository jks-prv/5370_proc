// routines called for 5370 device I/O

#include "boot.h"
#include "sim.h"
#include "front_panel.h"
#include "5370.h"
#include "misc.h"
#include "net.h"
#include "hpib.h"
#include "chip.h"
#include "timer.h"

#include ARCH_INCLUDE

#include <sys/file.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <setjmp.h>
#include <ctype.h>
#include <unistd.h>

typedef u1_t (*dev_read_t)(u2_t addr);
typedef void (*dev_write_t)(u2_t addr, u1_t data);

typedef struct {
   dev_read_t dev_read;
   dev_write_t dev_write;
} dev_io_t;

dev_io_t dev_io[DEV_SPACE_SIZE];

	static int tty;

u1_t bus_read(u2_t addr)
{
	u1_t data;
	
	data = bus_fast_read(addr);

	return data;
}

void bus_write(u2_t addr, u1_t data)
{
	bus_fast_write(addr, data);
}

#define BIT_AREG(reg)	(1 << ((RREG_ ## reg) - ADDR_ARM(0)))

static u4_t dump_regs;

#define DUMP_AREG(areg, addr, data) \
({ \
	u2_t a = RREG_ ## areg; \
	u2_t bit = BIT_AREG(areg); \
	if ((addr == a) && (dump_regs & bit)) { printf("%s 0x%x %d  ", # areg, data, data); dump_regs &= ~bit; if (!dump_regs) printf("\n"); } \
})

static u1_t shifted_key;

u1_t handler_dev_arm_read(u2_t addr)
{
	u1_t data, key;

	data = bus_read(addr);

//#define SELF_TEST
#ifdef SELF_TEST
	if (addr == ADDR_ARM(2)) data = 0;	// force self-test mode (simulate all switches down)
#endif

#ifdef DEBUG
	if (iDump) {
		printf("[ARM read @ 0x%x 0x%x] -------------------------\n", addr, data);
	}
#endif

	// The interrupt from pressing the LCL/RMT key causes the RREG_I1 to be read.
	// Check for a regular key being simultaneously pressed and then when the
	// register is accessed outside of the interrupt routine use the "sim key"
	// mechanism to process the "shifted" key press.
	if (addr == RREG_I1) {
		if (!(data & I1_IRQ)) {
			if (!(data & I1_LRTL)) {
				key = bus_read(RREG_KEY_SCAN);
				if ((key & KEY_IDLE2) != KEY_IDLE2) {
					shifted_key = key;
				}
			}
		} else {
			if (shifted_key) {
				sim_key = shifted_key;
				shifted_key = 0;
				sim_key_intr = 1;
			}
		}
	}

	DUMP_AREG(N0ST, addr, data);
	DUMP_AREG(N1N2H, addr, data);
	DUMP_AREG(N1N2L, addr, data);
	DUMP_AREG(N0H, addr, data);
	DUMP_AREG(N0L, addr, data);
	
	return data;
}

void handler_dev_arm_write(u2_t addr, u1_t data)
{

#ifdef DEBUG
	if (iDump) {
		printf("[ARM write @ 0x%x = 0x%x] ----------------------\n", addr, data);
	}
#endif

	bus_write(addr, data);
}

u1_t sim_key, sim_key_intr;

u1_t handler_dev_display_read(u2_t addr)
{
	u1_t data;

	data = bus_read(addr);

#if 0
if (addr == RREG_KEY_SCAN) { printf("k"); fflush(stdout); }
//else { printf("x"); fflush(stdout); }
if ((addr == RREG_KEY_SCAN) && (data != KEY_IDLE)) { printf("k%02x ", data); fflush(stdout); }
#endif

	if (addr == RREG_KEY_SCAN) {
		if (data != KEY_IDLE) {
//if ((addr == RREG_KEY_SCAN) && (data != KEY_IDLE)) { printf("k%02x ", data); fflush(stdout); }
			process_key(data);
		} else

		if (sim_key) {
			data = sim_key;
			if (!sim_key_intr) sim_key = 0;		// allow it to be read twice, but generate interrupt only once
			sim_key_intr = 0;
			process_key(data);
		}
//if ((addr == RREG_KEY_SCAN) && (data != KEY_IDLE)) { printf("KEY %02x ski%d", data, sim_key_intr); fflush(stdout); }
	}

	return data;
}

void handler_dev_display_write(u2_t addr, u1_t data)
{

#if 0
	if (addr >= ADDR_7SEG(0) && addr <= ADDR_7SEG(15)) {
		if (addr == ADDR_7SEG(0)) { printf("\n"); fflush(stdout); }
		if (data) printf("%x=%02x ", addr, data);
	}
#endif

	if ((addr & ~0xf) == ADDR_7SEG(0)) {
		dsp_7seg_write(addr - ADDR_7SEG(0), 0, data);
	} else
	if ((addr & ~0xf) == ADDR_LEDS(0xf)) {	// remember ADDR_LEDS() is reversed
		dsp_leds_write(0xf - (addr - ADDR_LEDS(0xf)), data);
	} else
		bus_write(addr, data);
}

#ifdef DEBUG

u1_t handler_dev_read_bad(u2_t addr)
{
	panic("dev read bad");
	return 0;
}

void handler_dev_write_bad(u2_t addr, u1_t data)
{
	panic("dev write bad");
}

#endif

void sim_main()
{
	u2_t i;
	
	tty = open("/dev/tty", O_RDONLY | O_NONBLOCK);
	if (tty < 0) sys_panic("open tty");

#ifdef BUS_TEST
	printf("TESTING...\n\n");
	void bus_test();
	bus_test();
#endif
	
	for (i = 0; i < DEV_SPACE_SIZE; i++) {
		dev_io_t *devp = &dev_io[i];
		
		if ((i >= ADDR_ARM(0)) && (i <= ADDR_ARM(0xf))) {
			devp->dev_read = handler_dev_arm_read;
			devp->dev_write = handler_dev_arm_write;
		} else
		if ((i >= ADDR_DSP(0)) && (i <= ADDR_DSP(0x1f))) {
			devp->dev_read = handler_dev_display_read;
			devp->dev_write = handler_dev_display_write;
		} else
		if ((i >= ADDR_HPIB(0)) && (i <= ADDR_HPIB(3))) {
			devp->dev_read = handler_dev_hpib_read;
			devp->dev_write = handler_dev_hpib_write;
#ifdef DEBUG
		} else {
			devp->dev_read = handler_dev_read_bad;
			devp->dev_write = handler_dev_write_bad;
#endif
		}
	}
	
	net_connect(SERVER, NULL, NULL);

	sim_processor();
}

#if defined(HPIB_RECORD) || defined(HPIB_SIM_DEBUG)
u1_t readDev(u2_t addr, u4_t iCount, u2_t rPC, u1_t n_irq, u4_t irq_masked)
#else
u1_t readDev(u2_t addr)
#endif
{

#if defined(HPIB_RECORD) || defined(HPIB_SIM_DEBUG)
	hpib_stamp(0, iCount, rPC, n_irq, irq_masked);
#endif

	// ROM bug?!
	if (addr == 0x804f)
		return 0;

#ifdef DEBUG
	if (addr >= DEV_SPACE_SIZE) {
		panic("too big");
	}
#endif

	return (dev_io[addr].dev_read)(addr);
}

#if defined(HPIB_RECORD) || defined(HPIB_SIM_DEBUG)
void writeDev(u2_t addr, u1_t data, u4_t iCount, u2_t rPC, u1_t n_irq, u4_t irq_masked)
#else
void writeDev(u2_t addr, u1_t data)
#endif
{
	
#if defined(HPIB_RECORD) || defined(HPIB_SIM_DEBUG)
	hpib_stamp(1, iCount, rPC, n_irq, irq_masked);
	
	if (n_irq) {
		return;
	}
#endif

#ifdef DEBUG
	if (addr >= DEV_SPACE_SIZE) {
		panic("too big");
	}
#endif

	(dev_io[addr].dev_write)(addr, data);
}

enum front_pnl_loc_e skey_func[] = { TI, TRG_LVL, FREQ, PERIOD };
enum front_pnl_loc_e skey_gate[] = { _1_PER, _PT01_SEC, _PT1_SEC, _1_SEC };
enum front_pnl_loc_e skey_math[] = { MEAN, STD_DEV, MIN, MAX, DSP_REF, CLR_REF, DSP_EVTS, SET_REF };
enum front_pnl_loc_e skey_size[] = { PT1, _100, _1K, _10K, _100K };
enum front_pnl_loc_e skey_othr[] = { P_TI_ONLY, P_M_TI, EXT_HOFF, PER_COMPL, EXT_ARM, MAN_RATE };

static char ibuf[32], dbuf[64];

char *sim_input()
{
	int n, e;
	char *cp = ibuf;

	n = read(tty, cp, sizeof(ibuf));
	if (n >= 1) {
		if ((n == 1) || (*cp == '?') || (strcmp(cp, "help\n") == 0) || ((*cp == 'h') && (n == 2))) {
			printf("commands:\n"
				"d\t\tshow instrument display including unit and key LEDs\n"
				"h [HPIB cmd]\temulate HPIB command input, e.g. \"h md2\"\n"
				"k [f1 .. f4]\temulate function key 1-4 press, e.g. \"k f2\" is the \"trig lvl\" key\n"
				"k [g1 .. g4]\temulate gate time key 1-4 press\n"
				"k [m1 .. m8]\temulate math key 1-8 press\n"
				"k [s1 .. s5]\temulate sample size key 1-5 press\n"
				"k [o1 .. o6]\temulate \"other\" key 1-6 press (see code for mapping)\n"
				"m\t\tcall measurement extension example code\n"
				"r\t\tshow values of count-chain registers (one sample)\n"
				"q\t\tquit\n\n"
				);
			return 0;
		}
		
		if (*cp == 'q') {
			exit(0);
		}
		
		if (*cp == 'm') {
			meas_extend_example(0);
			return 0;
		}
		
		// show what's on the 7 segment display, units display and key LEDs
		if (*cp == 'd') {
			n = dsp_7seg_2_char(dbuf);
			dbuf[n] = 0;
			printf("display: %s\n", dbuf);
			n = dsp_key_leds_2_char(dbuf);		// which keys have their LEDs lit
			dbuf[n] = 0;
			printf("keys: %s\n", dbuf);
			return 0;
		}

		// emulate a key press by causing an interrupt and returning the correct
		// scan code for the subsequent read of the RREG_KEY_SCAN register
		if (*cp == 'k') {
			e = 0;
			
			if (sscanf(cp, "k f%d", &n) == 1) {		// function keys 1..4
				if (n >= 1 && n <= 4)
					e = skey_func[n-1];
			} else
			if (sscanf(cp, "k g%d", &n) == 1) {		// gate time keys 1..4
				if (n >= 1 && n <= 4)
					e = skey_gate[n-1];
			} else
			if (sscanf(cp, "k m%d", &n) == 1) {		// math keys 1..8
				if (n >= 1 && n <= 8)
					e = skey_math[n-1];
			} else
			if (sscanf(cp, "k s%d", &n) == 1) {		// sample size keys 1..5
				if (n >= 1 && n <= 5)
					e = skey_size[n-1];
			} else
			if (sscanf(cp, "k o%d", &n) == 1) {		// other keys 1..6
				if (n >= 1 && n <= 6)
					e = skey_othr[n-1];
			}
			
			if (e) {
				sim_key = KEY(e);
				sim_key_intr = 1;
				printf("key press: %s (%d 0x%2x)\n", front_pnl_led[e].name, n, sim_key);
			} else {
				cp[strlen(cp)-1] = 0;
				printf("bad key command: \"%s\"\n", cp);
			}
			return 0;
		}

		if (*cp == 'r') {
			dump_regs = BIT_AREG(N0ST) | BIT_AREG(N1N2H) | BIT_AREG(N1N2L) | BIT_AREG(N0H) | BIT_AREG(N0L);
			return 0;
		}
		
#ifdef HPIB_SIM_DEBUG
		// emulate input of an HPIB command
		// e.g. "h md2" "h mr" "h md1" "h tb1" "h tb0"
		if (*cp == 'h' && cp[1] == ' ') {
			hpib_input(cp+2);	// presumes that hpib input is processed before another sim input
			return 0;
		}
#endif

#ifdef HPIB_RECORD
		if (*cp == 'h' && cp[1] == 'r') {
			hpib_dump();
			return 0;
		}
#endif

		return ibuf;	// pass to caller
	} else {
		return 0;
	}
}
