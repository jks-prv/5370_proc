#ifndef _DISPLAY_H_
#define _DISPLAY_H_

#define N_7SEG	16

// override of ascii character set
#define CHAR_MU		0x01		// e.g. as used in uSec

u4_t dsp_7seg_dp(u4_t pos);
u4_t dsp_7seg_write(u4_t pos, char c, u1_t d);
u4_t dsp_leds_read(u4_t a);
u4_t dsp_leds_write(u4_t a, u1_t d);

void dsp_7seg_chr(u4_t pos, char c);
void dsp_7seg_clr();
void dsp_7seg_str(u4_t pos, char *str, bool clear);
void dsp_7seg_num(u4_t lsd_pos, u4_t n, u4_t field_width, bool msd_first, bool zero_fill);

void dsp_7seg_init(void);
u1_t dsp_7seg_2_char(char *s);

u1_t dsp_key_leds_2_char(char *s);

void display_ipaddr(u1_t *ipaddr);

#define N_LEDS	16

void dsp_leds_clr_all();
void dsp_led_set(u4_t loc);
void dsp_led_clr(u4_t loc);
void dsp_led_invert(u4_t loc);
void dsp_unit_set(u4_t loc);
void dsp_unit_clr(u4_t loc);

// keys: D4|LDS{2:0}|D3|D2|D1|D0
#define KEY(loc)	(front_pnl_key[loc].drive | front_pnl_key[loc].sense)

#define KEY_IDLE	0xff
#define KEY_IDLE2	0x7f

#define LDS0	(7<<4)
#define LDS1	(6<<4)
#define LDS2	(5<<4)
#define LDS3	(4<<4)
#define LDS4	(3<<4)
#define LDS5	(2<<4)
#define LDS6	(1<<4)
#define LDS7	(0<<4)

#define D0		0x01
#define D1		0x02
#define D2		0x04
#define D3		0x08
#define D4		0x80
#define D5		0x00	// documentation only: not in scan chain

#define	GND		0

// leds
#define LED_ADDR(loc)	front_pnl_led[loc].drive
#define LED_DATA(loc)	front_pnl_led[loc].sense

#define UNIT_ADDR(loc)	front_pnt_units[loc].drive
#define UNIT_DATA(loc)	front_pnt_units[loc].sense

#define DSF		0x0
#define DSE		0x1
#define DSD		0x2
#define DSC		0x3
#define DSB		0x4
#define DSA		0x5
#define DS9		0x6
#define DS8		0x7
#define DS7		0x8
#define DS6		0x9
#define DS5		0xa
#define DS4		0xb
#define DS3		0xc

#define	L0	0x1
#define	L1	0x2
#define	L2	0x4
#define	L3	0x8

#define	XXX		0

enum front_pnl_loc_e {
	LCL_RMT,	TI,			TRG_LVL,	MEAN,		STD_DEV,	PT1,		_100,		P_TI_ONLY,	P_M_TI,
	RESET,		FREQ,		PERIOD,		MIN,		MAX,		_1K,		_10K,		EXT_HOFF,	PER_COMPL,
				_1_PER,		_PT01_SEC,	DSP_REF,	CLR_REF,	_100K,								EXT_ARM,
				_PT1_SEC,	_1_SEC,		DSP_EVTS,	SET_REF,	MAN_RATE,							MAN_INPUT
} front_pnl_loc;

enum front_pnl_loc2_e {
	ASTERISK,	KILO,		EVT,		LSTN,
	MEGA,		MILLI,		HZ,			TALK,
	MICRO,		NANO,		SECS,		LT_BOOST1,
	OF,			PICO,		VOLTS,		LT_BOOST2
} front_pnl_loc2;

typedef struct {
	u1_t drive, sense;
	char *name;
	u1_t order;
} scan_code;

extern scan_code front_pnl_key[], front_pnl_led[], front_pnt_units[];

extern bool dsp_7seg_ok;

u1_t check_char();
bool key_down();
void wait_key_release();
void preempt_reset_key(bool preempt);
void process_key(u1_t key);

// mechanism to allow a routine to get a callback on a front panel button push
typedef void (*k_cb_fn)(u1_t key);
u4_t register_key_callback(u1_t key, k_cb_fn k_cb);

#endif
