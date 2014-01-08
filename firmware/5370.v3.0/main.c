#include "boot.h"
#include "sim.h"
#include "5370.h"
#include "hpib.h"
#include "front_panel.h"
#include "misc.h"
#include "chip.h"

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <stdlib.h>
#include <fcntl.h>

// for IP address-mode part of front-panel settings UI
typedef enum { AM_DHCP, AM_IP, AM_NM, AM_GW, AM_LAST } addr_mode_e;
addr_mode_e addr_mode;
char *addr_mode_str[] = { "dhcp", "ip", "nm", "gw" };
static bool skip_first;

// keys used when changing front-panel settings
struct {
	u4_t key;
	u1_t key_idx;
} settings_keys[] = {
	{RESET,0},	{TI,0},			{TRG_LVL,0},	{FREQ,0},	{PERIOD,0},
	{MEAN,1},	{STD_DEV,2},	{PT1,3},		{_100,4},
	{MIN,-1},	{MAX,-2},		{_1K,-3},		{_10K,-4},
	{0,0}
};

// config info stored at the highest address in flash
typedef struct {
	u4_t key;
	addr_mode_e addr_mode;
	union {
		u1_t if_ipinfo[3][4];
		struct {
			u1_t ip[4], nm[4], gw[4];
		};
	};
} cfg_t;

cfg_t cfg_buf;

u1_t default_ipinfo[3][4] = { /*ipaddr*/{10,0,0,110}, /*netmask*/{255,255,255,0}, /*gateway*/{10,0,0,1} };

typedef enum { APP_START, APP_BAD, APP_WAIT } app_state_e;

#define LBUF 256
char lbuf[LBUF];

bool background_mode = FALSE;

int main(int argc, char *argv[])
{
	int i;
	bool bug = FALSE;
	app_state_e app_state;
	bool save_cfg = FALSE;
	bool change_settings_ui(u1_t key, bool *skip_first, cfg_t *cfg);
	
	FILE *cfp, *efp;
	char *config_file = "~/.instr.config";

	cfg_t *f_cfg = 0;	// location in flash where config is stored
	cfg_t *cfg = &cfg_buf;
	
	for (i=1; i<argc; i++) {
		if (strcmp(argv[i], "-bg") == 0) background_mode = TRUE;
		if (strcmp(argv[i], "-bug") == 0) bug = TRUE;
	}
	
	lprintf("HP%s v%d.%d\n", INST_STR, FIRMWARE_VER_MAJ, FIRMWARE_VER_MIN);
    lprintf("compiled: %s %s\n", __DATE__, __TIME__);

reset:
	app_state = APP_START;
	bus_init();
    dsp_7seg_init();

	// display firmware version
	dsp_7seg_str(0, INST_STR, TRUE);
	dsp_7seg_chr(10, 'v');
	dsp_7seg_num(11, FIRMWARE_VER_MAJ, 0, TRUE, FALSE);
	dsp_7seg_num(12, FIRMWARE_VER_MIN, 0, TRUE, FALSE);
	dsp_7seg_dp(12);
	dsp_leds_clr_all();
	delay(1000);

#if 0
	if ((cfp = fopen(config_file, "r+")) == NULL) {
		if (errno != ENOENT) sys_panic(config_file);
		if ((cfp = fopen(config_file, "w+")) == NULL) sys_panic(config_file);
		printf("creating config file %s\n", config_file);
	} else {
	
	}

	if (f_cfg->key != 0xcafebabe) {		// configuration part of flash not valid, so set some defaults
		cfg->key = 0xcafebabe;
		cfg->addr_mode = AM_DHCP;
		bcopy(default_ipinfo, cfg->if_ipinfo, sizeof(default_ipinfo));
		save_cfg = TRUE;
	} else {
		bcopy(f_cfg, cfg, sizeof(*cfg));
		addr_mode = cfg->addr_mode;
	}
#endif

	// for now just accept whatever address eth0 has
	addr_mode = AM_DHCP;
	if ((efp = popen("ifconfig eth0", "r")) == NULL) sys_panic("ifconfig eth0");
	char *lp = lbuf;
	int ip[4], nm[4], bc[4];

	i=0;
	while (fgets(lp, LBUF, efp)) {
		if ((i = sscanf(lp, "%*[ ]inet addr:%d.%d.%d.%d Bcast:%d.%d.%d.%d Mask:%d.%d.%d.%d",
			&ip[3], &ip[2], &ip[1], &ip[0],
			&bc[3], &bc[2], &bc[1], &bc[0],
			&nm[3], &nm[2], &nm[1], &nm[0])) == 12)
			break;
	}
	if (i == 12) {
		for (i=0; i<4; i++) {
			cfg->ip[3-i] = ip[i]; cfg->nm[3-i] = nm[i];
		}
		lprintf("Ethernet: ip %d.%d.%d.%d mask %d.%d.%d.%d\n",
			ip[3], ip[2], ip[1], ip[0], nm[3], nm[2], nm[1], nm[0]);
		dsp_7seg_str(0, "ip", TRUE);
		display_ipaddr(cfg->ip);
	} else {
		lprintf("eth0 not configured?");
		dsp_7seg_str(0, "enet config?", TRUE);
	}
	pclose(efp);
	delay(2000);

	// place a call here to setup your measurement extension code
	meas_extend_example_init();

	// reset key held down during a reboot -- drop into "change settings" mode
	preempt_reset_key(TRUE);
	
	if (bus_read(RREG_KEY_SCAN) == KEY(RESET)) {
		app_state = APP_WAIT;
		dsp_7seg_str(0, "ready", TRUE);
		wait_key_release();
		dsp_7seg_str(0, "chg settings", TRUE);
		skip_first = TRUE;
		
		// light up the keys valid during "change settings"
		for (i=0; settings_keys[i].key; i++) {
			dsp_led_set(settings_keys[i].key);
		}
	}

	while (1) {
		u1_t key;
		
		// while in the boot routine the reset key either starts the app or saves the changed config
		key = bus_read(RREG_KEY_SCAN);
		
		if (key == KEY(RESET)) {

			if (addr_mode != AM_DHCP) addr_mode = AM_IP;
			if (addr_mode != cfg->addr_mode) save_cfg = TRUE;

			if (save_cfg) {
				dsp_7seg_str(0, "config changed", TRUE);
				delay(2000);
				wait_key_release();
				cfg->addr_mode = addr_mode;
			
				if (addr_mode == AM_DHCP) {
					dsp_7seg_str(0, "using dhcp mode", TRUE);
				} else {
					dsp_7seg_str(0, "using ip mode", TRUE);
				}

				delay(2000);
				dsp_7seg_str(0, "saving config", TRUE);
				printf("FIXME save config file\n");
				delay(2000);
				goto reset;
			}
		
			if (app_state != APP_BAD) {
				dsp_7seg_str(0, "start", TRUE);
				wait_key_release();
				app_state = APP_START;
			}
		} else {
			if (change_settings_ui(key, &skip_first, cfg)) save_cfg = TRUE;
		}

		if (app_state == APP_START) {
			preempt_reset_key(FALSE);
			sim_main(bug);
			printf("main returned\n");
			panic("can't reset until bss zeroed\n");
			goto reset;
		}
	}

	return 0;
}


bool change_settings_ui(u1_t key, bool *skip_first, cfg_t *cfg)
{
	int i;
	bool save_cfg = FALSE;
	s1_t ki;

	// scroll up the ip address mode list
	if ((key == KEY(TI)) || (key == KEY(TRG_LVL))) {
		if (*skip_first) {
			*skip_first = FALSE;
		} else {
			addr_mode++;
			if (addr_mode == AM_LAST) addr_mode = 0;
		}
addr_mode_up_down:
		dsp_7seg_str(0, addr_mode_str[addr_mode], TRUE);
		if (addr_mode != AM_DHCP) {
			display_ipaddr(cfg->if_ipinfo[addr_mode-AM_IP]);
		}
		wait_key_release();
	} else

	// scroll down the ip address mode list
	if ((key == KEY(FREQ)) || (key == KEY(PERIOD))) {
		if (*skip_first) {
			*skip_first = FALSE;
		} else {
			if (addr_mode == 0) addr_mode = AM_LAST;
			addr_mode--;
		}
		goto addr_mode_up_down;
	}
	
	// inc/dec a byte of the ip/nm/gw address
	// hold key down for auto-repeat
	ki = 0;

	for (i=0; settings_keys[i].key; i++) {
		if (key == KEY(settings_keys[i].key)) {
			ki = settings_keys[i].key_idx;
			break;
		}
	}
	
	if (ki) {
		bool first_time = TRUE;
		u4_t td, tref = sys_now(), tcmp = 256, tinc = 32;
		do {
			td = time_diff(sys_now(), tref);
			if ((addr_mode != AM_DHCP) && (first_time || (td >= tcmp))) {
				if (ki > 0) {
					cfg->if_ipinfo[addr_mode-AM_IP][ki-1]++;
				} else {
					cfg->if_ipinfo[addr_mode-AM_IP][-ki-1]--;
				}
				display_ipaddr(cfg->if_ipinfo[addr_mode-AM_IP]);
				first_time = FALSE;
				tcmp += tinc;
			}
		} while (key_down());
		save_cfg = TRUE;
	}
	
	return save_cfg;
}
