// routines to handle 7 segment display, LEDs and keys

#include "boot.h"
#include "sim.h"
#include "5370.h"
#include "front_panel.h"

#include <string.h>
#include <stdio.h>
#include <ctype.h>

scan_code front_pnl_key[] = {
//	LCL_RMT,	TI,			TRG_LVL,	MEAN,		STD_DEV,	PT1,		_100,		P_TI_ONLY,	P_M_TI,
	{GND,D4},	{LDS0,D0},	{LDS1,D0},	{LDS2,D0},	{LDS3,D0},	{LDS4,D0},	{LDS5,D0},	{LDS6,D0},	{LDS7,D0},

//	RESET,		FREQ,		PERIOD,		MIN,		MAX,		_1K,		_10K,		EXT_HOFF,	PER_COMPL,
	{LDS6,D3},	{LDS0,D1},	{LDS1,D1},	{LDS2,D1},	{LDS3,D1},	{LDS4,D1},	{LDS5,D1},	{LDS6,D1},	{LDS7,D1},

//				_1_PER,		_PT01_SEC,	DSP_REF,	CLR_REF,	_100K,								EXT_ARM,
				{LDS0,D2},	{LDS1,D2},	{LDS2,D2},	{LDS3,D2},	{LDS4,D2},							{LDS7,D2},
				
//				_PT1_SEC,	_1_SEC,		DSP_EVTS,	SET_REF,	MAN_RATE,							MAN_INPUT (dedicated input not in scan chain),
				{LDS0,D3},	{LDS1,D3},	{LDS2,D3},	{LDS3,D3},	{LDS4,D3},							{GND,D5},
};

scan_code front_pnl_led[] = {
//	LCL_RMT,			TI,					TRG_LVL,				MEAN,				STD_DEV,			PT1,				_100,				P_TI_ONLY,				P_M_TI,
	{DSA,L3,"rmt",0},	{DSF,L0,"ti",1},	{DSE,L0,"trg_lvl",1},	{DSD,L0,"mean",3},	{DSC,L0,"sdev",3},	{DSB,L0,"1",4},		{DSA,L0,"100",4},	{DS9,L0,"ti_only",5},	{DS8,L0,"+/-_ti",5},

//	RESET,				FREQ,				PERIOD,					MIN,				MAX,				_1K,				_10K,				EXT_HOFF,				PER_COMPL,
	{DS9,L3,"rst",0},	{DSF,L1,"freq",1},	{DSE,L1,"period",1},	{DSD,L1,"min",3},	{DSC,L1,"max",3},	{DSB,L1,"1k",4},	{DSA,L1,"10k",4},	{DS9,L1,"ext_hoff",5},	{XXX,XXX},

//						_1_PER,				_PT01_SEC,				DSP_REF,			CLR_REF,			_100K,															EXT_ARM,
						{DSF,L2,"1_per",2},	{DSE,L2,".01s",2},		{DSD,L2,"dref",6},	{XXX,XXX},			{DSB,L2,"100k",4},												{DS8,L2,"ext_arm",5},
				
//						_PT1_SEC,			_1_SEC,					DSP_EVTS,			SET_REF,			MAN_RATE,														MAN_INPUT,
						{DSF,L3,".1s",2},	{DSE,L3,"1s",2},		{DSD,L3,"devt",6},	{DSC,L3,"sref",6},	{DSB,L3,"man_rate",6},											{DS8,L3,"man_inp",5},
};

scan_code front_pnt_units[] = {
//	ASTERISK,			KILO,				EVT,				LSTN
	{DS7,L0," *",2},	{DS6,L0,"k",0},		{DS5,L0," evt",2},	{DS4,L2," lstn",2},

//	MEGA,				MILLI,				HZ,					TALK,
	{DS7,L1,"M",0},		{DS6,L1,"m",0},		{DS5,L1,"Hz",1},	{DS3,L2," talk",2},

//	MICRO,				NANO,				SECS,				LT_BOOST1,
	{DS7,L2,"u",0},		{DS6,L2,"n",0},		{DS5,L2,"s",1},		{DS3,L0,"",2},

//	OF,					PICO,				VOLTS,				LT_BOOST2
	{DS7,L3," of",2},	{DS6,L3,"p",0},		{DS5,L3,"V",1},		{DS3,L1,"",2},
};

#undef B
#undef M

// 7 segments
#define T	0x01	// top
#define TL	0x20	// top-left
#define TR	0x02	// ...
#define M	0x40
#define B	0x08
#define BL	0x10
#define BR	0x04
#define DP	0x80	// decimal point

#define SEG7_ALL	(T|TL|TR|M|B|BL|BR|DP)

typedef struct {
	char c;
	u1_t segs;
} seg7_t;

seg7_t seg7[] = {
	{ '0',		(T|TL|TR|B|BL|BR) },
	{ '1',		(TR|BR) },
	{ '2',		(T|TR|M|B|BL) },
	{ '3',		(T|TR|M|B|BR) },
	{ '4',		(TL|TR|M|BR) },
	{ '5',		(T|TL|M|B|BR) },
	{ '6',		(T|TL|M|B|BL|BR) },
	{ '7',		(T|TR|BR) },
	{ '8',		(T|TL|TR|M|B|BL|BR) },
	{ '9',		(T|TL|TR|M|B|BR) },

	{ 'a',		(T|TL|TR|M|BL|BR) },
	{ 'b',		(TL|M|B|BL|BR) },
	{ 'c',		(M|B|BL) },
	{ 'd',		(TR|M|B|BL|BR) },
	{ 'e',		(T|TL|M|B|BL) },
	{ 'f',		(T|TL|BL|M) },
	{ 'g',		(T|TL|TR|M|B|BR) },
	{ 'h',		(TL|M|BL|BR) },
	{ 'i',		(TR|BR) },
	{ 'j',		(B|BR) },
	{ 'k',		(T|TL|M|BL|BR) },
	{ 'l',		(TL|B|BL) },
	{ 'm',		(T|TL|TR|B) },
	{ 'n',		(M|BL|BR) },
	{ 'o',		(M|B|BL|BR) },
	{ 'p',		(T|TL|TR|M|BL) },
	{ 'q',		(T|TL|TR|M|BR) },
	{ 'r',		(M|BL) },
	{ 's',		(T|TL|M|B|BR) },
	{ 't',		(TL|M|B|BL) },
	{ 'u',		(B|BL|BR) },
	{ 'v',		(TL|TR|B) },
	{ 'w',		(TL|TR|M|B) },
	{ 'x',		(T|M|B) },
	{ 'y',		(TL|TR|M|B|BR) },
	{ 'z',		(T|TR|M|B|BL) },
	
	{ '=',		(M|B) },
	{ '-',		(M) },
	{ '_',		(B) },
	{ '?',		(T|TR|M|BL) },
	{ ' ',		0 },
	{ '.',		DP },
	{ CHAR_MU,	(TL|TR|M|BL) },
};

#define TS_SEG7	(sizeof seg7 / sizeof seg7[0])

static u1_t dsp_7seg_cache[N_7SEG];
static char dsp_char_cache[N_7SEG];
static u1_t dsp_leds_cache[N_LEDS];

bool dsp_7seg_ok;

void dsp_7seg_init(void)
{
	dsp_7seg_ok = TRUE;
}

#define N_UNITS_SHOW sizeof(front_pnt_units) / sizeof(front_pnt_units[0])

// return string of 7 segment display contents plus units LEDs
u1_t dsp_7seg_2_char(char *s)
{
	int i, j, k, n = N_7SEG;
	u1_t sc;
	char cc;
	bool numeric = TRUE;
	scan_code *u;
	seg7_t *s7;

	for (i=0; i<N_7SEG; i++) {
		cc = dsp_char_cache[i];
		if (cc && (cc != ' ')) numeric = FALSE;
	}

	for (i=0; i<N_7SEG; i++) {
		// simulate spacing of 7-segment leds when displaying non-strings
		if (numeric && (i==2 || i==5 || i==8 || i==11 || i==14)) { *s++ = ' '; n++; }
	
		sc = dsp_7seg_cache[i];
		cc = dsp_char_cache[i];
		
		if (cc) {
			if (cc & DP) { *s++ = '.'; n++; }
			*s = cc & ~DP;
		} else {
			if (sc & DP) { *s++ = '.'; n++; }
			sc &= ~DP;
			for (j=0; j < TS_SEG7; j++) {
				s7 = &seg7[j];
				if (sc == s7->segs) {
					*s = s7->c;
					break;
				}
			}
			if (j == TS_SEG7) {
				s += sprintf(s, " ?%02x? ", sc);
			}
		}
		s++;
	}

	for (j=0; j<=2; j++) {
		for (i=0; i<N_UNITS_SHOW; i++) {
			u = &front_pnt_units[i];
			if ((u->order == j) && (dsp_leds_cache[u->drive] & u->sense)) {
				strcpy(s, u->name);
				k = strlen(u->name);
				s += k;
				n += k;
			}
		}
	}
	
	return n;
}

#define N_KLEDS_SHOW sizeof(front_pnl_led) / sizeof(front_pnl_led[0])

// return names of keys that have lit LEDs
u1_t dsp_key_leds_2_char(char *s)
{
	int i, j, k, n;
	scan_code *u;

	for (j=0; j<=6; j++) {
		for (i=0; i<N_KLEDS_SHOW; i++) {
			u = &front_pnl_led[i];
			if ((u->order == j) && (dsp_leds_cache[u->drive] & u->sense)) {
				strcpy(s, u->name);
				strcat(s, " ");
				k = strlen(u->name);
				s += k+1;
				n += k+1;
			}
		}
	}
	
	return n;
}

u4_t dsp_7seg_dp(u4_t pos)
{
	u1_t d = dsp_7seg_cache[pos];
	d |= DP;
	dsp_7seg_cache[pos] = d;
	bus_write(ADDR_7SEG(pos), d);

	if (dsp_char_cache[pos]) {
		dsp_char_cache[pos] |= DP;
	}

	return 0;
}

// c == 0 means there is no underlying string char generating this 7-segment digit
// i.e. we're being called from interpreted code
u4_t dsp_7seg_write(u4_t pos, char c, u1_t d)
{
	dsp_char_cache[pos] = c;
	dsp_7seg_cache[pos] = d;
	bus_write(ADDR_7SEG(pos), d);
	
	return 0;
}

u4_t dsp_leds_read(u4_t a)
{
	return (dsp_leds_cache[a]);
}

u4_t dsp_leds_write(u4_t a, u1_t d)
{
	dsp_leds_cache[a] = d;
	bus_write(ADDR_LEDS(a), d);
	
	return 0;
}

void dsp_7seg_chr(u4_t pos, char c)
{
	int i;
	u1_t d;
	seg7_t *s7;
	
	c = tolower(c);
	
	for (i=0; i < TS_SEG7; i++) {
		s7 = &seg7[i];
		if (c == s7->c) { d = s7->segs; break; }
	}
	
	if (i == TS_SEG7) d = SEG7_ALL;
	
	dsp_7seg_write(pos, c, d);
}

void dsp_7seg_clr()
{
	int i;
	
	for (i=0; i<N_7SEG; i++) {
		dsp_7seg_chr(i, ' ');
	}
}

void dsp_7seg_str(u4_t pos, char *str, bool clear)
{
	int i;
	char *s = str;
	
	if (clear) dsp_7seg_clr();
	
	for (i=0; i<strlen(str); i++) {
		dsp_7seg_chr(pos+i, *s++);
	}
}

void dsp_7seg_num(u4_t lsd_pos, u4_t n, u4_t field_width, bool msd_first, bool zero_fill)
{
	u4_t i, r, di=0;
	char d[16];
	
	for (i=0; (i < field_width) || (field_width == 0); i++) {
		r = n%10;
		d[di++] = '0'+r;
		n = n/10;
		if (n == 0) break;
	}

	for (i++; i<field_width; i++) {
		d[di++] = zero_fill? '0':' ';	// leading blanking
	}
	
	if (msd_first) lsd_pos += di-1;
	for (i=0; i<di; i++) {
		dsp_7seg_chr(lsd_pos-i, d[i]);
	}
}

void dsp_leds_clr_all()
{
	int i;
	
	for (i=0; i<N_LEDS; i++) {
		dsp_leds_write(i, 0);
	}
}

void dsp_led_set(u4_t loc)
{
	u4_t a = LED_ADDR(loc);
	
	dsp_leds_write(a, dsp_leds_read(a) | LED_DATA(loc));
}

void dsp_led_clr(u4_t loc)
{
	u1_t a = LED_ADDR(loc);
	
	dsp_leds_write(a, dsp_leds_read(a) & ~LED_DATA(loc));
}

void dsp_led_invert(u4_t loc)
{
	u1_t a = LED_ADDR(loc);
	
	dsp_leds_write(a, dsp_leds_read(a) ^ LED_DATA(loc));
}

void dsp_unit_set(u4_t loc)
{
	u1_t a = UNIT_ADDR(loc);
	
	dsp_leds_write(a, dsp_leds_read(a) | UNIT_DATA(loc));
}

void dsp_unit_clr(u4_t loc)
{
	u1_t a = UNIT_ADDR(loc);
	
	dsp_leds_write(a, dsp_leds_read(a) & ~UNIT_DATA(loc));
}

// display an IP address
void display_ipaddr(u1_t *ipaddr)
{
	dsp_7seg_num(4, ipaddr[0], 3, FALSE, FALSE);
	dsp_7seg_num(7, ipaddr[1], 3, FALSE, FALSE);
	dsp_7seg_dp(5);
	dsp_7seg_num(10, ipaddr[2], 3, FALSE, FALSE);
	dsp_7seg_dp(8);
	dsp_7seg_num(13, ipaddr[3], 3, FALSE, FALSE);
	dsp_7seg_dp(11);
}


bool key_down()
{
	u1_t key = bus_read(RREG_KEY_SCAN);
	return (key != KEY_IDLE);
}

void wait_key_release()
{
	do {
		;
	} while (key_down());
}

static bool reset_key_preempted;

void preempt_reset_key(bool preempt)
{
	reset_key_preempted = preempt;
}


#define N_KCB	16

typedef struct {
	u1_t key;
	k_cb_fn k_cb;
} key_cb_t;

key_cb_t key_cb[N_KCB];

static int n_kcb;
static bool cb_running;

// Register a callback when a particular key sequence is pressed.
// Intended to be used by routines that extend the measurement functionality of the instrument.
//
// A key sequence is defined as multiple keys contained in a single column on the front panel
// being pressed. This works because each row of keys on the front panel corresponds to a different
// low-order bit returned by bus_read(RREG_KEY_SCAN).
// So pressing several keys in a column results in multiple bits being or'd together.
//
// If keys across multiple columns are pressed this interferes with the display scan multiplexing and
// you get odd display effects (try it while the instrument is operating normally).
// Furthermore, a key sequence like this cannot be represented by the information returned from a single
// call to bus_read(RREG_KEY_SCAN). So such key sequences cannot be used for a callback.
//
// Alternatively the local/remote key can be used as a "shift" key (as in calculators) to
// select extended functions. This works because the value returned by bus_read(RREG_KEY_SCAN)
// when local/remote has previously been held down has the high-order bit or'd in.
//
// When the local/remote key is first pressed, but before the second key to select the extended function
// is pressed, the usual "return to local" HPIB action is performed, but this should be of no consequence.
// Similarly, when the first key of a multi-column key sequence is pressed it will operate in the usual way
// and only when the complete key sequence is pressed will the callback to the extended function be made.
// 

u4_t register_key_callback(u1_t key, k_cb_fn k_cb)
{
	key_cb_t *kcb = &key_cb[n_kcb];
	
	kcb->key = key;
	kcb->k_cb = k_cb;
	n_kcb++;
	
	return 0;
}

void process_key(u1_t key)
{
	char *cp;
	
	if (!reset_key_preempted) {
		if (key == KEY(RESET)) {
			dsp_7seg_str(0, "reset", TRUE);
			dsp_led_set(RESET);
			while (key_down())
				;
			printf("reset\n\n");
			sys_reset = TRUE;
			return;
		}

		// check for callback key sequences
		if (!cb_running && (key != KEY_IDLE)) {
			int i;
			key_cb_t *kcb;

			for (i=0; i<n_kcb; i++) {
				kcb = &key_cb[i];
				if (key == kcb->key) {
					cb_running = TRUE;
					sim_key = 0;
					kcb->k_cb(key);
					cb_running = FALSE;
					break;
				}
			}
		}
	}
}
