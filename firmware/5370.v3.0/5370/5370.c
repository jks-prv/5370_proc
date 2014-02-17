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

dev_io_t dev_io[DEV_SIZE];

static int tty;

static bool recall_file = FALSE, need_recall_file;
char conf_profile[N_PROFILE] = "default";

static bool sim_running = FALSE;

void sim_args(bool cmd_line, int argc, char *argv[])
{
	int i;

	for (i=1; i<argc; i++) {
	
		if (strcmp(argv[i], "-rcl") == 0 || strcmp(argv[i], "-recall") == 0) {
			recall_file = need_recall_file = TRUE;
			
			if (((i+1) < argc) && (argv[i+1][0] != '-')) {
				i++;
				strncpy(conf_profile, argv[i], N_PROFILE);
				printf("recall key settings from profile \"%s\"\n", conf_profile);
			}
		}

#ifdef DEBUG
		if (strcmp(argv[i], "-bug-stdev") == 0) bug_stdev = TRUE;
		if (strcmp(argv[i], "-bug-freq") == 0) bug_freq = TRUE;
		if (strcmp(argv[i], "-gt1") == 0) gate1 = TRUE;
		if (strcmp(argv[i], "-gt2") == 0) gate2 = TRUE;
		if (strcmp(argv[i], "-gt3") == 0) gate3 = TRUE;
		if (strcmp(argv[i], "-gt4") == 0) gate4 = TRUE;
		if (strcmp(argv[i], "-dev") == 0) deviation = TRUE;
		if (strcmp(argv[i], "-dval") == 0) { dval = atof(argv[++i]); printf("dval %f\n", dval); }
		if (strcmp(argv[i], "-all") == 0) display_all = TRUE;
		if (strcmp(argv[i], "-tr") == 0) trace_regs = TRUE;
		if (strcmp(argv[i], "-npru") == 0) use_pru = FALSE;
		if (strcmp(argv[i], "-rst") == 0) sim_key = KEY(RESET);
#endif
	}
}

void send_pru_cmd(u4_t cmd)
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
	CONV_ADDR_DCL(a_gen);
	CONV_DATA_DCL(d_gen);
	u1_t data;
	u4_t t;
	
	if (use_pru) {
		CONV_ADDR(pru->_, a_gen, addr);

		send_pru_cmd(PRU_READ);

		CONV_READ_DATA(t, data, pru2->_, d_gen);
	} else {
		data = bus_fast_read(addr);
	}

	return data;
}

void bus_write(u2_t addr, u1_t data)
{
	CONV_ADDR_DCL(a_gen);
	CONV_DATA_DCL(d_gen);
	
	if (use_pru) {
		CONV_ADDR(pru->_, a_gen, addr);
		CONV_WRITE_DATA(pru2->_, d_gen, data);

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
		if (sim_running) {
			if (!sim_key_intr) sim_key = 0;		// allow it to be read twice, but generate interrupt only once
		} else {
			sim_key = 0;	// only once for non-simulator uses
		}
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

// for printing (pr) use isActive() for actual value
// for decode (!pr) use bit mask value with AH/AL not taken into account (for now)
#define DBIT(pr, s, data, bit) if ((!pr && (data & bit)) || (pr && isActive(bit, data))) *s++ = #bit;

void dev_decode_regs(insn_attrs_t *ia, u1_t rw, u2_t addr, u1_t data)
{
	int pr = (ia == 0);
	u4_t f;
	char *strs[8] = {0,0,0,0,0,0,0,0}, **s = strs, **p = strs;

	//printf("ARM read %s @ 0x%x 0x%x <", arm_rreg[addr - ADDR_ARM(0)], addr, data);

	if (rw) switch (addr) {

	case RREG_LDACSR:
		DBIT(pr, s, data, DSR_TRIG_LVL_STOP);
		DBIT(pr, s, data, DSR_TRIG_LVL_START);
		DBIT(pr, s, data, DSR_SPARE);
		DBIT(pr, s, data, DSR_VOK);
		break;

	case RREG_I1:
		DBIT(pr, s, data, I1_IRQ);
		DBIT(pr, s, data, I1_RTI_MASK);
		DBIT(pr, s, data, I1_RST_TEST);
		DBIT(pr, s, data, I1_SRATE);
		DBIT(pr, s, data, I1_LRMT);
		DBIT(pr, s, data, I1_LRTL);
		DBIT(pr, s, data, I1_MAN_ARM);
		DBIT(pr, s, data, I1_IO_FLO);
		break;

	case RREG_ST:
		DBIT(pr, s, data, ST_OVEN);
		DBIT(pr, s, data, ST_EXT);
		break;

	case RREG_N0ST:
		DBIT(pr, s, data, N0ST_N3_OVFL);
		DBIT(pr, s, data, N0ST_EOM);
		DBIT(pr, s, data, N0ST_N0_POS);
		DBIT(pr, s, data, N0ST_ARMED);
		DBIT(pr, s, data, N0ST_PLL_OOL);
		DBIT(pr, s, data, N0ST_N0_OVFL);
		DBIT(pr, s, data, N0ST_N1N2_B17);
		DBIT(pr, s, data, N0ST_N1N2_B16);
		break;

	default:
		break;
	}

	//printf("ARM write %s @ 0x%x 0x%x <", arm_wreg[addr - ADDR_ARM(0)], addr, data);

	if (!rw) switch (addr) {

	case WREG_LDACCW:
		DBIT(pr, s, data, DCW_START_DAC_OE_L);
		DBIT(pr, s, data, DCW_STOP_DAC_OE_L);
		DBIT(pr, s, data, DCW_RELAY);
		DBIT(pr, s, data, DCW_LOCK_FIX);
		DBIT(pr, s, data, DCW_SPARE);
		DBIT(pr, s, data, DCW_HRMT_SLOPE);
		DBIT(pr, s, data, DCW_FWD_REV_START);
		DBIT(pr, s, data, DCW_FWD_REV_STOP);
		break;

	case WREG_O2:
		DBIT(pr, s, data, O2_FLAG);
		DBIT(pr, s, data, O2_SRATE_EN);
		DBIT(pr, s, data, O2_HTOGL);
		DBIT(pr, s, data, O2_GATE_MODE);
		DBIT(pr, s, data, O2_HARMCT3);
		DBIT(pr, s, data, O2_ARM_MODE);
		DBIT(pr, s, data, O2_MAN_ARM);
		DBIT(pr, s, data, O2_ARM_EN);
		break;

	case WREG_O1:
		DBIT(pr, s, data, O1_LRM_MASK);
		DBIT(pr, s, data, O1_RTI_MASK);
		DBIT(pr, s, data, O1_LHLDEN);
		DBIT(pr, s, data, O1_STOPSW);
		DBIT(pr, s, data, O1_STARTSW);
		DBIT(pr, s, data, O1_HSET2);
		DBIT(pr, s, data, O1_HSET1);
		DBIT(pr, s, data, O1_HSTD);
		break;

	case WREG_O3:
		DBIT(pr, s, data, O3_SELF_CLR);
		DBIT(pr, s, data, O3_RST_TEST);
		DBIT(pr, s, data, O3_LARMRST);
		DBIT(pr, s, data, O3_LOLRST);
		DBIT(pr, s, data, O3_N3_OVRST);
		DBIT(pr, s, data, O3_N0_OVRST);
		DBIT(pr, s, data, O3_N3_RST);
		DBIT(pr, s, data, O3_N012_RST);
		break;

	default:
		break;
	}
	
	if (!*p) return;
	if (!pr) f = ia->flags;
	
	//if (!pr && (f & _BIT)) PF("<%s> ", *p); else
	if (pr || (!pr && (f & (_BIT | _AND | _OR | _XOR)))) {
		PF("%s<", pr? "" : "[msk]   ");
		int first=1;
		for (p=strs; p!=s; first=0, p++) PF("%s%s", first? "":", ", *p);
		PF("> ");
	} else
	if (!pr && *p) PF("FIXME %08x <%s> ", f, *p);
}

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
	
#ifdef BUS_TEST
	printf("TESTING...\n\n");
	void bus_test();
	bus_test();
#endif

	for (i = 0; i < DEV_SIZE; i++) {
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
	
	sim_running = TRUE;
	sim_processor();
	sim_running = FALSE;
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
	if (addr >= DEV_SIZE) {
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
	if (addr >= DEV_SIZE) {
		panic("too big");
	}
#endif

	(dev_io[addr].dev_write)(addr, data);
}

static u4_t key_epoch, key_threshold;

// mechanism to spread out recall key presses over time
static bool kdelay(delay)
{
	static u4_t key_delay, total_delay;

	key_delay = time_diff(sys_now(), key_epoch);
	total_delay = delay + 100;
	
	if ((key_delay >= total_delay) && (total_delay > key_threshold)) {
		key_threshold = total_delay;
		return TRUE;
	}
	
	return FALSE;
}

enum front_pnl_loc_e skey_func[] = { TI, TRG_LVL, FREQ, PERIOD };
enum front_pnl_loc_e skey_gate[] = { _1_PER, _PT01_SEC, _PT1_SEC, _1_SEC };
enum front_pnl_loc_e skey_stat[] = { MEAN, STD_DEV, MIN, MAX, DSP_REF, CLR_REF, DSP_EVTS, SET_REF };
enum front_pnl_loc_e skey_samp[] = { PT1, _100, _1K, _10K, _100K };
enum front_pnl_loc_e skey_misc[] = { P_TI_ONLY, P_M_TI, EXT_HOFF, PER_COMPL, EXT_ARM, MAN_RATE };

#define	N_DBUF	256
static char ibuf[32], dbuf[N_DBUF];

#define	SELF_TEST_DELAY		2048

static bool self_test;
static bool recall_active;
static FILE *kfp;
static int rcl_key;
static u4_t boot_time;

// return zero if command processed locally
char *sim_input()
{
	int n=0, k, key;
	char *cp = ibuf;
	char key_name[16];
	
	if (background_mode)
		return 0;
	
	if (!boot_time) boot_time = sys_now();

	// can't recall any keys until self-test is finished
	if (self_test && boot_time && (time_diff(sys_now(), boot_time) > SELF_TEST_DELAY)) {
		self_test = FALSE;
	}
	
	if (!self_test && !need_recall_file && !recall_active) {
		config_file_update();
	}
	
	if (!self_test && need_recall_file) {
		sprintf(dbuf, "/home/root/.5370.%s.keys", conf_profile);
		if ((kfp = fopen(dbuf, "r")) != NULL) {
			key_epoch = sys_now(); key_threshold = rcl_key = 0;
			recall_active = TRUE;
		} else {
			printf("no key profile named \"%s\", will create one\n", conf_profile);
		}
		need_recall_file = FALSE;
	}
	
	if (recall_active && kfp && !self_test && kdelay(rcl_key * 256)) {
		if (fgets(dbuf, N_DBUF, kfp)) {
			if (sscanf(dbuf, "rcl key 0x%02x %16s", &key, key_name) == 2) {
				cp = "k-\n"; n=3;	// virtual command to get key press thru code below
				rcl_key++;
			}
		} else {
			fclose(kfp);
			kfp = 0;
			recall_active = FALSE;
		}
	}

#ifdef DEBUG
 #ifdef HPIB_SIM
	if (bug_stdev) {
		if (kdelay(0)) hpib_input("tr\n");
		if (kdelay(500)) hpib_input("ta+0.05\n");
		if (kdelay(1000)) find_bug();
	}
 #endif

	if (bug_freq) {
		if (kdelay(0)) { cp = "k fn3\n"; n=6; }
		if (kdelay(500) && gate1) { cp = "k gt1\n"; n=6; }
		if (kdelay(500) && gate2) { cp = "k gt2\n"; n=6; }
		if (kdelay(500) && gate3) { cp = "k gt3\n"; n=6; }
		if (kdelay(500) && gate4) { cp = "k gt4\n"; n=6; }
		//if (kdelay(1000)) { cp = "t\n"; n=2; }
		//if (kdelay(1500)) { cp = "z\n"; n=2; }
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
				"rcl|recall [name]   load key settings from current or named profile\n"
				"sto|store name      save key settings to named profile\n"
				"r\t\treset instrument\n"
				"q\t\tquit\n"
				"\n");
			return 0;
		}
		
		if ((*cp == 'r') && (n == 2)) {
			sys_reset = TRUE;
			return 0;
		}

		if (*cp == 'q') {
			exit(0);
		}
		
		// process command starting with '-' as args
		if (*cp == '-') {
			#define NARGS 16
			int argc; char *argv[NARGS];
			
			argc = 1 + split(cp, &argc, &argv[1], NARGS);
			sim_args(FALSE, argc, argv);
			hpib_args(FALSE, argc, argv);
			return 0;
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
			k = 0;
			
			if (*(cp+1) == '-') {					// key press from recall above
				k = -1;
			} else
			if (sscanf(cp, "k fn%d", &n) == 1) {	// function keys 1..4
				if (n >= 1 && n <= 4)
					k = skey_func[n-1];
			} else
			if (sscanf(cp, "k gt%d", &n) == 1) {	// gate time keys 1..4
				if (n >= 1 && n <= 4)
					k = skey_gate[n-1];
			} else
			if (sscanf(cp, "k st%d", &n) == 1) {	// statistics keys 1..8
				if (n >= 1 && n <= 8)
					k = skey_stat[n-1];
			} else
			if (sscanf(cp, "k ss%d", &n) == 1) {	// sample size keys 1..5
				if (n >= 1 && n <= 5)
					k = skey_samp[n-1];
			} else
			if (sscanf(cp, "k m%d", &n) == 1) {		// misc keys 1..6
				if (n >= 1 && n <= 6)
					k = skey_misc[n-1];
			} else
#ifdef DEBUG
			if (strcmp(cp, "k r\n") == 0) {			// for remote debugging of menu mode
				k = RESET;
			} else
			if (strcmp(cp, "k d\n") == 0) {
				k = TI;
			} else
			if (strcmp(cp, "k u\n") == 0) {
				k = FREQ;
			} else
#endif
			;
			
			if (k > 0) {
				sim_key = KEY(k);
				sim_key_intr = 1;
				printf("key press: %s (%d 0x%02x)\n", front_pnl_led[k].name, n, sim_key);
			} else
			if (k == -1) {
				sim_key = key;
				sim_key_intr = 1;
				printf("recall: key 0x%02x %s\n", key, key_name);
			} else {
				cp[strlen(cp)-1] = 0;
				printf("bad key command: \"%s\"\n", cp);
			}
			
			num_meas = 0;
			return 0;
		}
		
		if (strcmp(cp, "rcl\n")==0 || strcmp(cp, "recall\n")==0) {
			printf("recall key settings from current profile \"%s\"\n", conf_profile);
			need_recall_file = TRUE;
			return 0;
		}

		if (sscanf(cp, "rcl %16s", conf_profile)==1 || sscanf(cp, "recall %16s", conf_profile)==1) {
			printf("recall key settings from profile \"%s\"\n", conf_profile);
			need_recall_file = TRUE;
			return 0;
		}
		
		if (strcmp(cp, "sto\n")==0 || strcmp(cp, "store\n")==0) {
			printf("usage: sto|store name\n");
			return 0;
		}

		if (sscanf(cp, "store %16s", conf_profile)==1 || sscanf(cp, "sto %16s", conf_profile)==1) {
			printf("store key settings to profile \"%s\"\n", conf_profile);
			return 0;
		}

		// measure number of measurements-per-second
		if (*cp == 's' && n==2) {
			if (num_meas) {
				printf("%.1f meas/s\n", (float)num_meas / ((float)(sys_now()-meas_time)/1000.0));
				num_meas = meas_time = 0;
			}

			return 0;
		}

#ifdef DEBUG
		if (*cp == 'r' && cp[1] == 'c' && n==3) {
			dump_regs = BIT_AREG(N0ST) | BIT_AREG(N1N2H) | BIT_AREG(N1N2L) | BIT_AREG(N0H) | BIT_AREG(N0L);
			return 0;
		}

		if (*cp == 'z') {
			//trace_regs ^= 1;
			//trace_iDump(1);
			hps ^= 1;
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

// done on each push of 'reset' key (or reset command)
void sim_reset()
{
	hold_off = FALSE;
	boot_time = rcl_key = num_meas = meas_time = shifted_key;
	self_test = TRUE;
	need_recall_file = recall_file;
	recall_active = FALSE;
	kfp = 0;
	
	bus_reset();
	front_panel_reset();
	sim_proc_reset();
	hpib_reset();
}

// only done once per app run
void sim_init()
{
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
	bus_pru_gpio_init();

	if (!background_mode) {
		tty = open("/dev/tty", O_RDONLY | O_NONBLOCK);
		if (tty < 0) sys_panic("open tty");
	}
}
