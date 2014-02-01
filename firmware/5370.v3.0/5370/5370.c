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
#include "pru_realtime.h"

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
#include <time.h>
#include <sys/resource.h>
#include <sys/mman.h>
#include <math.h>

#ifdef DEBUG
static bool bug_stdev = FALSE;
static bool bug_freq = FALSE;
static bool gate1 = FALSE;
static bool gate2 = FALSE;
static bool gate3 = FALSE;
static bool gate4 = FALSE;
static bool deviation = FALSE;
static double dval = 10.0;
static bool display_all = FALSE;
static u4_t dump_regs = 0;
static bool trace_regs = FALSE;
#endif

static u4_t num_meas, meas_time;
static bool hold_off = FALSE;
static bool use_pru = TRUE;
static u4_t n3_ovfl_sent, n0_ovfl_sent, ovfl_none;

#ifdef FREQ_DEBUG
 #define freq_record(chan, type, addr, data, time, str)	reg_record(chan, type, addr, data, time, str)
#else
 #define freq_record(chan, type, addr, data, time, str)
#endif

typedef u1_t (*dev_read_t)(u2_t addr);
typedef void (*dev_write_t)(u2_t addr, u1_t data);

typedef struct {
   dev_read_t dev_read;
   dev_write_t dev_write;
} dev_io_t;

dev_io_t dev_io[DEV_SPACE_SIZE];

static int tty;

void sim_init(int argc, char *argv[])
{
	int i;

	for (i=1; i<argc; i++) {
#ifdef DEBUG
		if (strcmp(argv[i], "-bug_stdev") == 0) bug_stdev = TRUE;
		if (strcmp(argv[i], "-bug_freq") == 0) bug_freq = TRUE;
		if (strcmp(argv[i], "-gt1") == 0) gate1 = TRUE;
		if (strcmp(argv[i], "-gt2") == 0) gate2 = TRUE;
		if (strcmp(argv[i], "-gt3") == 0) gate3 = TRUE;
		if (strcmp(argv[i], "-gt4") == 0) gate4 = TRUE;
		if (strcmp(argv[i], "-dev") == 0) deviation = TRUE;
		if (strcmp(argv[i], "-dval") == 0) { dval = atof(argv[++i]); printf("dval %f\n", dval); }
		if (strcmp(argv[i], "-all") == 0) display_all = TRUE;
		if (strcmp(argv[i], "-tr") == 0) trace_regs = TRUE;
		if (strcmp(argv[i], "-npru") == 0) use_pru = FALSE;
#endif
	}

	// try and improve our performance a bit
    setpriority(PRIO_PROCESS, 0, -20);
	scall("mlockall", mlockall(MCL_CURRENT | MCL_FUTURE));

	// check that new polarity scheme didn't break anything
	assert(WREG_O3_RST == WREG_O3_RST_GOOD);
	assert(WREG_O2_IDLE == WREG_O2_IDLE_GOOD);
	assert(WREG_O2_ENA == WREG_O2_ENA_GOOD);
	assert(WREG_O2_ARM == WREG_O2_ARM_GOOD);

	bus_init();
	pru_start();
	bus_gpio_init();
}

static void send_pru_cmd(cmd)
{
	u4_t i;
	
	assert(pru->cmd == PRU_DONE);
	pru->cmd = cmd;
	
	for (i=0; pru->cmd != PRU_DONE; i++) {
		if ((i & 0xffffff) == 0xffffff)
			printf("PRU not responding?\n");
	}
}


// when the PRU is in use counting overflows it must also proxy regular bus cycles to prevent bus contention

u1_t bus_read(u2_t addr)
{
	CONV_ADDR_DCL(g_addr);
	CONV_DATA_DCL(g_data);
	u1_t data;
	
	if (use_pru) {
		CONV_ADDR(addr, g_addr);
		CONV_COPY_ADDR(g_addr, pru);

		send_pru_cmd(PRU_READ);

		CONV_COPY_READ_DATA(pru, g_data);
		CONV_READ_DATA(g_data, data);
	} else {
		data = bus_fast_read(addr);
	}

	return data;
}

void bus_write(u2_t addr, u1_t data)
{
	CONV_ADDR_DCL(g_addr);
	CONV_DATA_DCL(g_data);
	
	if (use_pru) {
		CONV_ADDR(addr, g_addr);
		CONV_COPY_ADDR(g_addr, pru);
		CONV_WRITE_DATA(data, g_data);
		CONV_COPY_WRITE_DATA(g_data, pru);

		send_pru_cmd(PRU_WRITE);

	} else {
		bus_fast_write(addr, data);
	}
}

#ifdef DEBUG
 char *arm_rreg[] = { "ldacsr", "1?", "switches", "in1", "status", "n0st", "n0h", "n0l", "n1n2h", "n1n2l", "n3h", "n3l" };
#endif

static u1_t shifted_key;

u1_t handler_dev_arm_read(u2_t addr)
{
	u1_t data, key;

	assert (addr == RREG_LDACSR || addr == RREG_A16_SWITCHES || addr == RREG_I1 || addr == RREG_ST ||
		addr == RREG_N0ST || addr == RREG_N0H || addr == RREG_N0L || addr == RREG_N1N2H || addr == RREG_N1N2L ||
		addr == RREG_N3H || addr == RREG_N3L);

	// derive N0ST values from PRU collection of overflow data
	if (use_pru && hold_off && (addr == RREG_N0ST)) {
		bool sent=FALSE, done_before=FALSE;

		if (pru->count == PRU_DONE) done_before=TRUE;

		if ((pru->n3_ovfl - n3_ovfl_sent) > 0) {
			n3_ovfl_sent++;
			sent=TRUE;
			data = inactive(N0ST_EOM) | active(N0ST_N3_OVFL) | inactive(N0ST_N0_OVFL);
		} else
		if ((pru->n0_ovfl - n0_ovfl_sent) > 0) {
			n0_ovfl_sent++;
			sent=TRUE;
			data = inactive(N0ST_EOM) | inactive(N0ST_N3_OVFL) | active(N0ST_N0_OVFL);
		} else {
			ovfl_none++;
			data = inactive(N0ST_EOM) | inactive(N0ST_N3_OVFL) | inactive(N0ST_N0_OVFL);
		}

		// PRU saw EOM: be careful about the race condition when looking at the counters
		if (!sent && done_before && (pru->count == PRU_DONE)) {
			freq_record(0, REG_STR, 0, 0, 0, "H");
			hold_off = FALSE;
		}
		
		return data;
	}

	data = bus_read(addr);

#ifdef FREQ_DEBUG
	if (use_pru && !hold_off) {
		if (addr == RREG_N0ST && (isActive(N0ST_EOM, data) || isActive(N0ST_N3_OVFL, data) || isActive(N0ST_N0_OVFL, data))) {
			freq_record(0, REG_READ, addr, data, 0, 0);
		}
	}
#endif

//#define SELF_TEST
#ifdef SELF_TEST
	if (addr == RREG_A16_SWITCHES) data = 0;	// force self-test mode (simulate all switches down)
#endif

#ifdef DEBUG
	if (iDump) {
		printf("ARM read %s @ 0x%x 0x%x <", arm_rreg[addr - ADDR_ARM(0)], addr, data);
		switch (addr) {
		case RREG_I1:
			PBIT(I1_IRQ);
			PBIT(I1_RTI_MASK);
			PBIT(I1_RST_TEST);
			PBIT(I1_SRATE);
			PBIT(I1_LRMT);
			PBIT(I1_LRTL);
			PBIT(I1_MAN_ARM);
			PBIT(I1_IO_FLO);
			break;
		case RREG_N0ST:
			PBIT(N0ST_N3_OVFL);
			PBIT(N0ST_EOM);
			PBIT(N0ST_ARMED);
			PBIT(N0ST_PLL_OOL);
			PBIT(N0ST_N0_OVFL);
			break;
		default:
			break;
		}
		printf(">\n");
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
	
#ifdef DEBUG
	if (trace_regs) {
		if (addr == RREG_N0ST) {
			if (isActive(N0ST_EOM, data)) printf("EOM");
			if (isActive(N0ST_N3_OVFL, data)) printf("3");
			if (isActive(N0ST_N0_OVFL, data)) printf("0");
		}
		if (addr == RREG_I1 && data == 0x7f) printf("_"); else
		if (addr == RREG_ST) { ; } else
		if (addr == RREG_A16_SWITCHES) ; else
		printf("%s=%02x ", arm_rreg[addr - ADDR_ARM(0)], data);
	}
#endif

	return data;
}

#ifdef DEBUG
 char *arm_wreg[] = { "spare", "ldaccw", "ldacstart", "ldacstop", "out2", "out1", "out3" };
#endif

void handler_dev_arm_write(u2_t addr, u1_t data)
{

#ifdef DEBUG
	if (iDump) {
		printf("ARM write %s @ 0x%x 0x%x <", arm_wreg[addr - ADDR_ARM(0)], addr, data);
		switch (addr) {
		case WREG_O2:
			PBIT(O2_FLAG);
			PBIT(O2_SRATE_EN);
			PBIT(O2_HTOGL);
			PBIT(O2_GATE_MODE);
			PBIT(O2_HARMCT3);
			PBIT(O2_ARM_MODE);
			PBIT(O2_MAN_ARM);
			PBIT(O2_ARM_EN);
			break;
		case WREG_O1:
			PBIT(O1_LRM_MASK);
			PBIT(O1_RTI_MASK);
			PBIT(O1_LHLDEN);
			PBIT(O1_STOPSW);
			PBIT(O1_STARTSW);
			PBIT(O1_HSET2);
			PBIT(O1_HSET1);
			PBIT(O1_HSTD);
			break;
		case WREG_O3:
			PBIT(O3_SELF_CLR);
			PBIT(O3_RST_TEST);
			PBIT(O3_LARMRST);
			PBIT(O3_LOLRST);
			PBIT(O3_N3_OVRST);
			PBIT(O3_N0_OVRST);
			PBIT(O3_N3_RST);
			PBIT(O3_N012_RST);
			break;
		default:
			break;
		}
		printf(">\n");
	}
#endif

	assert (addr == WREG_LDACCW || addr == WREG_LDACSTART || addr == WREG_LDACSTOP ||
		addr == WREG_O2 || addr == WREG_O1 || addr == WREG_O3);

#ifdef DEBUG
	if (trace_regs) {
		if ((addr == WREG_O3) && isActive(O3_N3_OVRST, data)) printf("R3 "); else
		if ((addr == WREG_O3) && isActive(O3_N0_OVRST, data)) printf("R0 "); else
		printf("%s=%02x ", arm_wreg[addr - ADDR_ARM(0)], data);
	}
#endif

	// let PRU count overflows while measuring
	// FIXME: is this test valid for all measurement modes? O2_ARM_EN instead? TI +/- vs +only mode?
	if ((addr == WREG_O2) && isActive(O2_MAN_ARM, data)) {
		num_meas++;
		if (meas_time == 0) meas_time = sys_now();

#ifdef DEBUG
		if (trace_regs) { printf("* "); fflush(stdout); }
#endif
		freq_record(0, REG_STR, 0, 0, 0, "* ");

		if (use_pru) {
			assert(pru->count == PRU_DONE);
			bus_write(addr, data);	// let arm command go through
			n3_ovfl_sent = n0_ovfl_sent = ovfl_none = 0;

			send_pru_cmd(PRU_CLEAR);

			hold_off = TRUE;
			pru->count = PRU_COUNT;
			return;
		}
	}

	// ignore WREG_O3 writes to clear overflow bits while PRU is doing it
	if (use_pru && hold_off && (addr == WREG_O3)) {
		return;
	}

#ifdef FREQ_DEBUG
	if (use_pru && !hold_off) {
		if (addr == WREG_O3) {
			if (isActive(O3_N3_OVRST, data) || isActive(O3_N0_OVRST, data))
				freq_record(0, REG_READ, addr, data, 0, 0);
		}
	}
#endif

	bus_write(addr, data);
}

u1_t sim_key, sim_key_intr;

u1_t handler_dev_display_read(u2_t addr)
{
	u1_t data;

	assert (addr == RREG_KEY_SCAN);

	data = bus_read(addr);

	if (data != KEY_IDLE) {
		process_key(data);
	} else

	if (sim_key) {
		data = sim_key;
		if (!sim_key_intr) sim_key = 0;		// allow it to be read twice, but generate interrupt only once
		sim_key_intr = 0;
		process_key(data);
	}

	return data;
}

#ifdef FREQ_DEBUG

void reg_dump_read(u2_t addr, u1_t data, u4_t dup, u4_t time, char *str)
{
	if (addr == RREG_N0ST) {
		printf("%02x", data);
		if (isActive(N0ST_EOM, data)) printf("EOM");
		if (isActive(N0ST_N3_OVFL, data)) printf("3V");
		if (isActive(N0ST_N0_OVFL, data)) printf("0V");
		printf(".");
	}

	if (addr == WREG_O3) {
		printf("%02x", data);
		if (isActive(O3_N3_OVRST, data)) printf("3C");
		if (isActive(O3_N0_OVRST, data)) printf("0C");
		printf(".");
	}
}

void reg_dump_str(u2_t addr, u1_t data, u4_t dup, u4_t time, char *str)
{
	printf("%s", str);
}

#endif

void handler_dev_display_write(u2_t addr, u1_t data)
{

#ifdef DEBUG
	if (trace_regs) printf("%c%x", (ADDR_DEV(addr) == ADDR_7SEG(0))? 'D':'L', addr&0xf);
#endif

	if (ADDR_DEV(addr) == ADDR_7SEG(0)) {
		dsp_7seg_write(addr - ADDR_7SEG(0), 0, data);
	} else
	if (ADDR_DEV(addr) == ADDR_LEDS(0xf)) {	// remember ADDR_LEDS() is reversed
		dsp_leds_write(0xf - (addr - ADDR_LEDS(0xf)), data);
	} else {
		bus_write(addr, data);
	}

#ifdef DEBUG
	// last digit of display has been written
	if ((display_all || deviation) && (addr == ADDR_7SEG(0xf))) {
		static char dbuf[128];
		static double dval2;
		static u4_t ddn, dskip;

		freq_record(0, REG_STR, 0, 0, 0, "D ");
		dsp_7seg_translate(dbuf, &dval2); ddn++;
		
		if (display_all || ((ddn & 0xf) == 0))
			printf("%s\n", dbuf);

		if (deviation && (fabs(dval-dval2) > 0.000001)) {
			printf("+%d %12.9f\n", ddn, dval2);
			ddn = 0;
#ifdef FREQ_DEBUG
			if (dskip > 50) reg_dump(0, reg_dump_read, NULL, reg_dump_str);
#endif
		}
		
		freq_record(0, REG_RESET, 0, 0, 0, 0);
		dskip++;
	}
#endif

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
	
	if (!background_mode) {
		tty = open("/dev/tty", O_RDONLY | O_NONBLOCK);
		if (tty < 0) sys_panic("open tty");
	}

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

#ifdef REG_RECORD
u1_t readDev(u2_t addr, u4_t iCount, u2_t rPC, u1_t n_irq, u4_t irq_masked)
#else
u1_t readDev(u2_t addr)
#endif
{

	reg_stamp(0, iCount, rPC, n_irq, irq_masked);

	// PHK: manual pg 8-57, flowchart box 11: A15 is a signature analyzer trigger
	if (addr == 0x804f)
		return 0;

#ifdef DEBUG
	if (addr >= DEV_SPACE_SIZE) {
		printf("addr 0x%x\n", addr);
		panic("too big");
	}
#endif

	return (dev_io[addr].dev_read)(addr);
}

#ifdef REG_RECORD
void writeDev(u2_t addr, u1_t data, u4_t iCount, u2_t rPC, u1_t n_irq, u4_t irq_masked)
#else
void writeDev(u2_t addr, u1_t data)
#endif
{
	
#ifdef REG_RECORD
	reg_stamp(1, iCount, rPC, n_irq, irq_masked);
	
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
enum front_pnl_loc_e skey_stat[] = { MEAN, STD_DEV, MIN, MAX, DSP_REF, CLR_REF, DSP_EVTS, SET_REF };
enum front_pnl_loc_e skey_samp[] = { PT1, _100, _1K, _10K, _100K };
enum front_pnl_loc_e skey_misc[] = { P_TI_ONLY, P_M_TI, EXT_HOFF, PER_COMPL, EXT_ARM, MAN_RATE };

static char ibuf[32], dbuf[256];

char *sim_input()
{
	int n=0, e;
	char *cp = ibuf;
	static int bg_delay = 0;
	
	if (background_mode)
		return 0;

#ifdef DEBUG
	if (bg_delay < 4000) bg_delay++;
#ifdef HPIB_SIM
	if (bug_stdev) {
		if (bg_delay == 1000) hpib_input("tr\n");
		if (bg_delay == 1500) hpib_input("ta+0.05\n");
		if (bg_delay == 2000) find_bug();
	}
#endif
	if (bug_freq) {
		if (bg_delay == 1000) { cp = "k fn3\n"; n=6; }
		if (bg_delay == 1500 && gate1) { cp = "k gt1\n"; n=6; }
		if (bg_delay == 1500 && gate2) { cp = "k gt2\n"; n=6; }
		if (bg_delay == 1500 && gate3) { cp = "k gt3\n"; n=6; }
		if (bg_delay == 1500 && gate4) { cp = "k gt4\n"; n=6; }
		//if (bg_delay == 2000) { cp = "t\n"; n=2; }
		//if (bg_delay == 2500) { cp = "z\n"; n=2; }
	}
#endif

	if (!n) {
		n = read(tty, cp, sizeof(ibuf));
		if (n >= 1) cp[n] = 0;
	}
	
	if (n >= 1) {

		if ((n == 1) || (*cp == '?') || (strcmp(cp, "help\n") == 0) || ((*cp == 'h') && (n == 2))) {
			printf("commands:\n"
				"d\t\tshow instrument display including unit and key LEDs\n"
				"h <HPIB cmd>\temulate HPIB command input, e.g. \"h md2\"\n"
				"h?\t\tprints reminder list of HPIB commands\n"
				"k <fn1 .. fn4>\temulate function key 1-4 press, e.g. \"k fn1\" is TI key\n"
				"k <gt1 .. gt4>\temulate gate time key 1-4 press\n"
				"k <st1 .. st8>\temulate statistics key 1-8 press\n"
				"k <ss1 .. ss5>\temulate sample size key 1-5 press\n"
				"k <m1 .. m6>\temulate \"misc\" key 1-6 press\n"
				"\t\t1 TI only, 2 +/- TI, 3 ext h.off, 4 per compl, 5 ext arm, 6 man rate\n"
				"m\t\trun measurement extension example code\n"
				"s\t\tshow measurement statistics\n"
				"rc\t\tshow values of count-chain registers (one sample)\n"
				"q\t\tquit\n"
				"\n");
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
			dsp_7seg_translate(dbuf, 0);
			printf("display: %s\n", dbuf);
			dsp_key_leds_translate(dbuf);		// which keys have their LEDs lit
			printf("keys: %s\n", dbuf);
			return 0;
		}

		// emulate a key press by causing an interrupt and returning the correct
		// scan code for the subsequent read of the RREG_KEY_SCAN register
		if (*cp == 'k') {
			e = 0;
			
			if (sscanf(cp, "k fn%d", &n) == 1) {	// function keys 1..4
				if (n >= 1 && n <= 4)
					e = skey_func[n-1];
			} else
			if (sscanf(cp, "k gt%d", &n) == 1) {	// gate time keys 1..4
				if (n >= 1 && n <= 4)
					e = skey_gate[n-1];
			} else
			if (sscanf(cp, "k st%d", &n) == 1) {	// statistics keys 1..8
				if (n >= 1 && n <= 8)
					e = skey_stat[n-1];
			} else
			if (sscanf(cp, "k ss%d", &n) == 1) {	// sample size keys 1..5
				if (n >= 1 && n <= 5)
					e = skey_samp[n-1];
			} else
			if (sscanf(cp, "k m%d", &n) == 1) {		// misc keys 1..6
				if (n >= 1 && n <= 6)
					e = skey_misc[n-1];
			}
			
			if (e) {
				sim_key = KEY(e);
				sim_key_intr = 1;
				printf("key press: %s (%d 0x%2x)\n", front_pnl_led[e].name, n, sim_key);
			} else {
				cp[strlen(cp)-1] = 0;
				printf("bad key command: \"%s\"\n", cp);
			}
			
			num_meas = 0;
			return 0;
		}

		// measure number of measurements-per-second
		if (*cp == 's') {
			if (num_meas) {
				printf("%.1f meas/s\n", (float)num_meas / ((float)(sys_now()-meas_time)/1000.0));
				num_meas = meas_time = 0;
			}

			return 0;
		}

#ifdef DEBUG
		if (*cp == 'r' && cp[1] == 'c') {
			dump_regs = BIT_AREG(N0ST) | BIT_AREG(N1N2H) | BIT_AREG(N1N2L) | BIT_AREG(N0H) | BIT_AREG(N0L);
			return 0;
		}

		if (*cp == 'z') {
			//trace_regs ^= 1;
			return 0;
		}
#endif

#ifdef HPIB_SIM
		// emulate input of an HPIB command
		// e.g. "h md2" "h mr" "h md1" "h tb1" "h tb0"
		if (*cp == 'h' && cp[1] == ' ') {
			hpib_input(cp+2);	// presumes that hpib input is processed before another sim input
			num_meas = 0;
			return 0;
		}
#endif

		if (*cp == 'h' && cp[1] == '?') {
			printf("HPIB command reminder list: (shown uppercase but may be typed as lowercase)\n"
			"function: FN1 TI, FN2 trig lvl, FN3 freq, FN4 period\n"
			"gate: GT1 single period, GT2 0.01s, GT3 0.1s, GT4 1s\n"
			"statistics: ST1 mean, ST2 std dev, ST3 min, ST4 max, ST5 dsp ref, ST6 clr ref, ST7 dsp evts, ST8 set ref, ST9 dsp all\n"
			"sample sizes: SS1 1, SS2 100, SS3 1k, SS4 10k, SS5 100k, SB <4 bytes> (set size for binary xfer)\n"
			"mode: MD1 front pnl, MD2 hold until \"MR\" cmd, MD3 fast (only if addressed), MD4 fast (wait until addressed)\n"
			"input: IN1 start+stop, IN2 stop only, IN3 start only, IN4 start+stop swap\n"
			"slope: SA1 start+, SA2 start-, S01 stop+, S02 stop-, SL slope local, SR slope remote\n"
			"arm select: AR1 +TI only, AR2 +/-TI\n"
			"ext arming: EA0 dis, EA1 ena, SE1 slope+, SE2 slope-, EH0 hold-off dis, EH1 hold-off ena\n"
			"int arming: IA1 auto, IA2 start ch arm, IA3 stop ch arm\n"
			"trigger: TL trig local, TR trig remote, TA <volts> start lvl, TO <volts> stop lvl\n"
			"binary mode: TB0 disable, TB1 enable, TB2 fast-mode enable (virtual cmd)\n"
			"other: MR man rate, MI man input, PC period compl, TE teach (store), LN learn (recall)\n"
			"\n");
			return 0;
		}

		return cp;	// pass to caller
	} else {
		return 0;
	}
}
