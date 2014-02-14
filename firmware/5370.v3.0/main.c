#include "boot.h"
#include "sim.h"
#include "5370.h"
#include "hpib.h"
#include "front_panel.h"
#include "misc.h"
#include "chip.h"
#include "net.h"

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <stdlib.h>
#include <fcntl.h>

// for IP address-mode part of front-panel settings UI
typedef enum { AM_DHCP, AM_IP, AM_NM, AM_GW, AM_LAST } addr_mode_e;
/*const*/ char *addr_mode_str[] = { "dhcp", "ip", "nm", "gw" };

// keys used when changing front-panel settings
const struct {
	u4_t key;
	u1_t key_idx;
} settings_keys[] = {
	{RESET,0},
	{TI,0},		{MEAN,1},	{STD_DEV,2},	{PT1,3},	{_100,4},
	{FREQ,0},	{MIN,-1},	{MAX,-2},		{_1K,-3},	{_10K,-4},
	{0,0}
};

// config info stored at the highest address in flash
typedef struct {
	addr_mode_e addr_mode;
	union {
		u1_t if_ipinfo[3][4];
		struct {
			u1_t ip[4], nm[4], gw[4];
		};
	};
} cfg_t;

cfg_t cfg_buf;

const u1_t default_ipinfo[3][4] = { /*ipaddr*/{10,0,0,10}, /*netmask*/{255,255,255,0}, /*gateway*/{10,0,0,1} };

typedef enum { APP_START, APP_BAD, APP_WAIT } app_state_e;

#define LBUF 256
char lbuf[LBUF];

bool background_mode = FALSE;

static bool skip_first = FALSE;

int main(int argc, char *argv[])
{
	int i, n;
	
	bool isReset = FALSE;
	app_state_e app_state;
	bool save_cfg = FALSE;
	bool change_settings_ui(addr_mode_e *addr_mode, u1_t key, bool *skip_first, cfg_t *cfg);
	bool show_ip = FALSE;
	bool config_valid, config_key = FALSE;
	bool config_ip = FALSE, config_nm = FALSE, config_gw = FALSE, config_am = FALSE;
	u4_t key;
	addr_mode_e addr_mode;
	int ip[4], nm[4], gw[4], bc[4];
	
	FILE *cfp, *efp;
	char *config_file = "/home/root/.5370.config";

	cfg_t *cfg = &cfg_buf;
	
	dsp_7seg_init(FALSE);	// panic routine can't use display until bus is setup

	// silently ignores unrecognized arguments
	for (i=1; i<argc; i++) {
		if (strcmp(argv[i], "-bg") == 0) background_mode = TRUE;
		if (strcmp(argv[i], "-ip") == 0) show_ip = TRUE;

		if (strcmp(argv[i], "?")==0 || strcmp(argv[i], "-?")==0 || strcmp(argv[i], "--?")==0 || strcmp(argv[i], "-h")==0 ||
			strcmp(argv[i], "h")==0 || strcmp(argv[i], "-help")==0 || strcmp(argv[i], "--h")==0 || strcmp(argv[i], "--help")==0) {
			printf( "-rcl|-recall [name]    load key settings from named profile\n"
					"-hpib-hard    use the original HPIB hardware interface, assuming installed\n"
					"-hpib-sim     simulate the HPIB interface in software (debug mode)\n"
					"-hpib-net     simulate and re-direct transfers over an Ethernet connection\n"
					"-ip           show IP address of Ethernet interface and exit\n"
			);
			xit(0);
		}
	}
	
	lprintf("HP%s v%d.%d\n", INST_STR, FIRMWARE_VER_MAJ, FIRMWARE_VER_MIN);
    lprintf("compiled: %s %s\n", __DATE__, __TIME__);

	sim_args(TRUE, argc, argv);
	hpib_args(TRUE, argc, argv);

	sim_init();

reset:
	// to support the action of the 'reset' key most code files have a reset routine that zeros static variables.
	// This is similar to the C runtime idea of zeroing the bss when a program is first run.
	
	if (isReset) {
		net_disconnect();
		skip_first = FALSE;
	}

	app_state = APP_START;

	sim_reset();
	dsp_7seg_init(TRUE);

	if ((bus_read(RREG_LDACSR) | bus_read(RREG_KEY_SCAN) | bus_read(RREG_N0ST)) == 0) {
		lprintf("no 5370 detected?\n");
		panic("no 5370");
	}
	
	// display firmware version
	dsp_7seg_str(0, INST_STR, TRUE);
	dsp_7seg_chr(10, 'v');
	dsp_7seg_num(11, FIRMWARE_VER_MAJ, 0, TRUE, FALSE);
	dsp_7seg_num(12, FIRMWARE_VER_MIN, 0, TRUE, FALSE);
	dsp_7seg_dp(12);
	dsp_leds_clr_all();
	delay(1000);

	if ((cfp = fopen(config_file, "r")) == NULL) {
		if (errno != ENOENT) sys_panic(config_file);
		config_valid = FALSE;
	} else {
		while (fgets(lbuf, LBUF, cfp)) {
			if ((sscanf(lbuf, "key 0x%x", &key) == 1) && (key == 0xcafe5370)) config_key = TRUE; else
			if (sscanf(lbuf, "am %d", &addr_mode) == 1) config_am = TRUE; else
			if (sscanf(lbuf, "ip %d.%d.%d.%d", &ip[0], &ip[1], &ip[2], &ip[3]) == 4) config_ip = TRUE; else
			if (sscanf(lbuf, "nm %d.%d.%d.%d", &nm[0], &nm[1], &nm[2], &nm[3]) == 4) config_nm = TRUE; else
			if (sscanf(lbuf, "gw %d.%d.%d.%d", &gw[0], &gw[1], &gw[2], &gw[3]) == 4) config_gw = TRUE; else
				;
		}
		if (config_key && config_ip && config_nm && config_gw && config_am) {
			printf("valid config file %s\n", config_file);
			config_valid = TRUE;

			if (addr_mode != AM_DHCP) {
				printf("setting interface address\n");
				sprintf(lbuf, "ifconfig eth0 %d.%d.%d.%d netmask %d.%d.%d.%d",
					ip[0], ip[1], ip[2], ip[3], nm[0], nm[1], nm[2], nm[3]);
				system(lbuf);
				sprintf(lbuf, "route add default %d.%d.%d.%d", gw[0], gw[1], gw[2], gw[3]);
				system(lbuf);
			}
		} else {
			printf("invalid config file %s\n", config_file);
			config_valid = FALSE;
		}
		fclose(cfp);
	}

	if (!config_valid) {		// configuration not valid, so set some defaults
		addr_mode = cfg->addr_mode = AM_DHCP;
		bcopy(default_ipinfo, cfg->if_ipinfo, sizeof(default_ipinfo));
		save_cfg = TRUE;
	}

#define ENET_RETRY 10

	if (addr_mode == AM_DHCP) {
		gw[3]=gw[2]=gw[1]=gw[0]=0;	// ifconfig doesn't tell us the gateway, only the broadcast which we don't care about
		
		// sometimes the link is slow to come up, so retry a few times
		for (i=0; i<ENET_RETRY; i++) {
			if ((efp = popen("ifconfig eth0", "r")) == NULL) sys_panic("ifconfig eth0");
			char *lp = lbuf;
		
			n=0;
			while (fgets(lp, LBUF, efp)) {
				if ((n = sscanf(lp, "%*[ ]inet addr:%d.%d.%d.%d Bcast:%d.%d.%d.%d Mask:%d.%d.%d.%d",
					&ip[0], &ip[1], &ip[2], &ip[3],
					&bc[0], &bc[1], &bc[2], &bc[3],
					&nm[0], &nm[1], &nm[2], &nm[3])) == 12)
					break;
			}
			pclose(efp);
			if (n == 12) break;
			delay(1000);
		}
	} else {
		i=0;	// configured manually
	}

	if (i != ENET_RETRY) {
		for (i=0; i<4; i++) {
			cfg->ip[i] = ip[i]; cfg->nm[i] = nm[i]; cfg->gw[i] = gw[i];
		}

		if (addr_mode == AM_DHCP) lprintf("via DHCP ");
		lprintf("eth0: ip %d.%d.%d.%d mask %d.%d.%d.%d ",
			ip[0], ip[1], ip[2], ip[3], nm[0], nm[1], nm[2], nm[3]);
		if (addr_mode != AM_DHCP) lprintf("gw %d.%d.%d.%d", gw[0], gw[1], gw[2], gw[3]);
		lprintf("\n");
		dsp_7seg_str(0, "ip", TRUE);
		display_ipaddr(cfg->ip);
	} else {
		lprintf("eth0: not configured from DHCP?");
		dsp_7seg_str(0, "no dhcp?", TRUE);
	}
	
	if (show_ip) xit(0);
	delay(2000);	// show ip on display for a moment before continuing

	net_connect(SERVER, NULL, HPIB_TCP_PORT);

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

				if ((cfp = fopen(config_file, "w")) == NULL) sys_panic(config_file);
				printf("writing config file %s\n", config_file);
				fprintf(cfp, "key 0xcafe5370\n");
				fprintf(cfp, "am %d\n", cfg->addr_mode);
				fprintf(cfp, "ip %d.%d.%d.%d\n", cfg->ip[0], cfg->ip[1], cfg->ip[2], cfg->ip[3]);
				fprintf(cfp, "nm %d.%d.%d.%d\n", cfg->nm[0], cfg->nm[1], cfg->nm[2], cfg->nm[3]);
				fprintf(cfp, "gw %d.%d.%d.%d\n", cfg->gw[0], cfg->gw[1], cfg->gw[2], cfg->gw[3]);
				fclose(cfp);

				delay(2000);
				isReset = TRUE;
				goto reset;
			}
		
			if (app_state != APP_BAD) {
				dsp_7seg_str(0, "start", TRUE);
				wait_key_release();
				app_state = APP_START;
			}
		} else {
			if (change_settings_ui(&addr_mode, key, &skip_first, cfg)) save_cfg = TRUE;
		}

		if (app_state == APP_START) {
			preempt_reset_key(FALSE);
			sim_main();
			printf("RESET\n");
			isReset = TRUE;
			goto reset;
		}
	}

	return 0;
}


bool change_settings_ui(addr_mode_e *addr_mode, u1_t key, bool *skip_first, cfg_t *cfg)
{
	int i;
	bool save_cfg = FALSE;
	s1_t ki;

	// scroll up the ip address mode list
	if (key == KEY(TI)) {
		if (*skip_first) {
			*skip_first = FALSE;
		} else {
			*addr_mode = *addr_mode+1;
			if (*addr_mode == AM_LAST) *addr_mode = 0;
		}
addr_mode_up_down:
		dsp_7seg_str(0, addr_mode_str[*addr_mode], TRUE);
		if (*addr_mode != AM_DHCP) {
			display_ipaddr(cfg->if_ipinfo[*addr_mode-AM_IP]);
		}
		wait_key_release();
	} else

	// scroll down the ip address mode list
	if (key == KEY(FREQ)) {
		if (*skip_first) {
			*skip_first = FALSE;
		} else {
			if (*addr_mode == 0) *addr_mode = AM_LAST;
			*addr_mode = *addr_mode-1;
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
			if ((*addr_mode != AM_DHCP) && (first_time || (td >= tcmp))) {
				if (ki > 0) {
					cfg->if_ipinfo[*addr_mode-AM_IP][ki-1]++;
				} else {
					cfg->if_ipinfo[*addr_mode-AM_IP][-ki-1]--;
				}
				display_ipaddr(cfg->if_ipinfo[*addr_mode-AM_IP]);
				first_time = FALSE;
				tcmp += tinc;
			}
		} while (key_down());
		save_cfg = TRUE;
	}
	
	return save_cfg;
}
