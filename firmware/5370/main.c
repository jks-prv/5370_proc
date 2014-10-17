#include "boot.h"
#include "sim.h"
#include "5370.h"
#include "hpib.h"
#include "front_panel.h"
#include "misc.h"
#include "chip.h"
#include "net.h"
#include "web.h"
#include "pru_realtime.h"

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sched.h>

#include "printf.h"

// for IP address-mode part of front-panel settings UI
typedef enum { M_CANCEL, M_HALT, M_DHCP, M_IP, M_NM, M_GW, M_LAST } menu_e;
/*const*/ char *menu_str[] = { "cancel", "halt", "dhcp", "ip", "nm", "gw" };

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
	int menu;
	union {
		u1_t if_ipinfo[3][4];
		struct {
			u1_t ip[4], nm[4], gw[4];
		};
	};
} cfg_t;

cfg_t cfg_buf;

const u1_t default_ipinfo[3][4] = { /*ipaddr*/{10,0,0,10}, /*netmask*/{255,255,255,0}, /*gateway*/{10,0,0,1} };

#define LBUF 256
char lbuf[LBUF];

bool background_mode = FALSE;

static bool skip_first = FALSE;
static bool menu_action = TRUE;

typedef enum { S_MENU, S_MENU_POLL, S_MENU_DONE, S_START } app_state_t;
static app_state_t app_state;

int main(int argc, char *argv[])
{
	int i, n;
	
	bool wasRunning = FALSE;
	bool save_cfg = FALSE;
	bool change_settings_ui(menu_e *menu, u1_t key, bool *skip_first, cfg_t *cfg);
	bool show_ip = FALSE;
	bool config_valid, config_key = FALSE;
	bool config_ip = FALSE, config_nm = FALSE, config_gw = FALSE, config_am = FALSE;
	u4_t key;
	menu_e menu;
	int addr_mode;
	int ip[4], nm[4], gw[4], bc[4];
	
	FILE *cfp, *efp;
	char *config_file = ROOT_DIR "/.5370.config";

	cfg_t *cfg = &cfg_buf;
	
	dsp_7seg_init(FALSE);	// panic routine can't use display until bus is setup

	// silently ignores unrecognized arguments
	for (i=1; i<argc; i++) {
		if (strcmp(argv[i], "-bg") == 0) background_mode = TRUE;
		if (strcmp(argv[i], "-ip") == 0) show_ip = TRUE;
		if (strcmp(argv[i], "-no") == 0) menu_action = FALSE;

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
	web_server_init();
	
	if (!menu_action) printf("menu action disabled\n");

reset:
	// To support the action of the 'reset' key most code files have a reset routine that zeros static variables.
	// This is similar to the C runtime idea of zeroing the bss when a program is first run.
	
	if (wasRunning) {
		wasRunning = FALSE;
		net_reset(NET_HPIB);
		net_reset(NET_TELNET);
		web_server_stop();
		skip_first = save_cfg = config_key = config_ip = config_nm = config_gw = config_am = FALSE;
	}

	sim_reset();

	if (!(bus_read(RREG_LDACSR) & DSR_VOK)) {
		lprintf("waiting for 5370 power\n");
		usleep(1000000);
		while (!(bus_read(RREG_LDACSR) & DSR_VOK)) {
			sched_yield();
			usleep(250000);
		}
		lprintf("5370 power on\n");
		usleep(1000000);
	} else {
		lprintf("5370 is powered on\n");
	}
	
	// display firmware version
	dsp_7seg_init(TRUE);
	dsp_7seg_str(DSP_LEFT, INST_STR, DSP_CLEAR);
	dsp_7seg_chr(POS(10), 'v');
	dsp_7seg_num(POS(11), POS_IS_LSD, FIRMWARE_VER_MAJ, DEFAULT_WIDTH, SPACE_FILL);
	dsp_7seg_num(POS(12), POS_IS_MSD, FIRMWARE_VER_MIN, FIELD_WIDTH(0), ZERO_FILL);
	dsp_7seg_dp(POS(12));
	dsp_leds_clr_all();
	delay(2000);

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
		assert((addr_mode == 0) || (addr_mode == 1));
		menu = cfg->menu = (addr_mode == 0)? M_DHCP : M_IP;
		
		if (config_key && config_ip && config_nm && config_gw && config_am) {
			printf("valid config file %s\n", config_file);
			config_valid = TRUE;

			if (menu == M_IP) {
				printf("setting interface address\n");
				sprintf(lbuf, "ifconfig eth0 %d.%d.%d.%d netmask %d.%d.%d.%d",
					ip[0], ip[1], ip[2], ip[3], nm[0], nm[1], nm[2], nm[3]);
				if (menu_action) system(lbuf);
				sprintf(lbuf, "route add default %d.%d.%d.%d", gw[0], gw[1], gw[2], gw[3]);
				if (menu_action) system(lbuf);
			}
		} else {
			printf("invalid config file %s\n", config_file);
			config_valid = FALSE;
		}
		fclose(cfp);
	}

	if (!config_valid) {
		menu = cfg->menu = M_DHCP;		// try DHCP first if not valid config
	}

#define ENET_RETRY 20

	if (menu == M_DHCP) {	// see if interface configured by DHCP
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
		i=0;	// interface configured manually above
	}

	if (i != ENET_RETRY) {
		for (i=0; i<4; i++) {
			cfg->ip[i] = ip[i]; cfg->nm[i] = nm[i]; cfg->gw[i] = gw[i];
		}

		if (menu == M_DHCP) lprintf("via DHCP ");
		lprintf("eth0: ip %d.%d.%d.%d mask %d.%d.%d.%d ",
			ip[0], ip[1], ip[2], ip[3], nm[0], nm[1], nm[2], nm[3]);
		if (menu != M_DHCP) lprintf("gw %d.%d.%d.%d", gw[0], gw[1], gw[2], gw[3]);
		lprintf("\n");
		dsp_7seg_str(DSP_LEFT, "ip", DSP_CLEAR);
		display_ipaddr(cfg->ip);
	} else {
		lprintf("eth0: not configured from DHCP?");
		dsp_7seg_str(DSP_LEFT, "no dhcp?", DSP_CLEAR);
	}
	
	if (!config_valid && (i == ENET_RETRY)) {		// configuration not valid, DHCP failed, so set some defaults
		menu = cfg->menu = M_IP;
		bcopy(default_ipinfo, cfg->if_ipinfo, sizeof(default_ipinfo));
		save_cfg = TRUE;
	}

	if (show_ip) xit(0);
	delay(2000);	// show ip on display for a moment before continuing

	net_connect(NET_HPIB, SERVER, NULL, HPIB_TCP_PORT);
	net_connect(NET_TELNET, SERVER, NULL, TELNET_TCP_PORT);
	web_server_start();

	// place a call here to setup your measurement extension code
	meas_extend_example_init();

	// reset key held down during a reboot -- drop into menu mode
	preempt_reset_key(TRUE);

	// while in the boot routine the reset key either starts the app or saves the changed config
	app_state = S_MENU;

	while (1) {
		u1_t key;

		sim_input();	// for remote debugging of menu mode
		key = handler_dev_display_read(RREG_KEY_SCAN);	// called instead of bus_read() so simulated keys will be returned

		switch(app_state) {
	
		case S_MENU:
			if (key != KEY(RESET)) {
				app_state = S_START;
				break;
			}

			dsp_7seg_str(DSP_LEFT, "ready", DSP_CLEAR);
			printf("ready\n");
			dsp_led_set(RESET);
			wait_key_release();
			dsp_led_clr(RESET);
			dsp_7seg_str(DSP_LEFT, "chg settings", DSP_CLEAR);
			printf("menu mode\n");
			skip_first = TRUE;
			menu = M_HALT;		// first menu item displayed
			
			// light up the keys valid during menu mode
			for (i=0; settings_keys[i].key; i++) {
				dsp_led_set(settings_keys[i].key);
			}

			app_state = S_MENU_POLL;
			break;

		case S_MENU_POLL:
			if (key == KEY(RESET)) {
				dsp_led_set(RESET);
				wait_key_release();
				dsp_led_clr(RESET);
				app_state = S_MENU_DONE;
				break;
			}

			if (change_settings_ui(&menu, key, &skip_first, cfg)) save_cfg = TRUE;
			break;

		case S_MENU_DONE:
			if (!skip_first && (menu == M_HALT)) {
			
					// Debian takes a while to halt, but nicely clears the GPIOs so the
					// display goes blank right when halted.
					// Angstrom with Gnome disabled halts very fast, but doesn't
					// clear the GPIOs like Debian. So we get the PRU to poll the LEDs
					// until they go off, then blank the display.
					dsp_7seg_str(DSP_LEFT, " halting...", DSP_CLEAR);
					printf("halting...\n");
					
					#ifdef DIST_DEBIAN
						if (menu_action) system("halt");
						exit(0);
					#endif

					#ifdef DIST_ANGSTROM
						dsp_7seg_chr(POS(0), ' ');		 // preload address & data
						send_pru_cmd(PRU_HALT);
						if (menu_action) system("halt");
						exit(0);
					#endif
			} else
			if (menu == M_CANCEL || (skip_first && (menu == M_HALT))) {
				app_state = S_START;
				break;
			} else {
				if (menu != M_DHCP) menu = M_IP;
				if (menu != cfg->menu) save_cfg = TRUE;
			}

			if (save_cfg) {
				dsp_7seg_str(DSP_LEFT, "config changed", DSP_CLEAR);
				delay(2000);
				cfg->menu = menu;
			
				if (menu == M_DHCP) {
					dsp_7seg_str(DSP_LEFT, "using dhcp mode", DSP_CLEAR);
				} else {
					dsp_7seg_str(DSP_LEFT, "using ip mode", DSP_CLEAR);
				}

				delay(2000);
				dsp_7seg_str(DSP_LEFT, "saving config", DSP_CLEAR);
				if ((cfp = fopen(config_file, "w")) == NULL) sys_panic(config_file);
				printf("writing config file %s\n", config_file);
				fprintf(cfp, "key 0xcafe5370\n");
				fprintf(cfp, "am %d\n", (cfg->menu == M_DHCP)? 0:1);
				fprintf(cfp, "ip %d.%d.%d.%d\n", cfg->ip[0], cfg->ip[1], cfg->ip[2], cfg->ip[3]);
				fprintf(cfp, "nm %d.%d.%d.%d\n", cfg->nm[0], cfg->nm[1], cfg->nm[2], cfg->nm[3]);
				fprintf(cfp, "gw %d.%d.%d.%d\n", cfg->gw[0], cfg->gw[1], cfg->gw[2], cfg->gw[3]);
				fclose(cfp);

				delay(2000);
			}
			
			app_state = S_START;
			break;
		
		case S_START:
			if (wasRunning) {		// if previous sim was interrupted must reset before starting new one
				goto reset;
			}
			
			preempt_reset_key(FALSE);
			sim_main();
			preempt_reset_key(TRUE);
			handler_dev_display_read(RREG_KEY_SCAN);	// flush extra sim reset key, if any
			delay(1000);
			
			// this sim was interrupted, so can't restart a new sim without doing a reset first
			wasRunning = TRUE;

			// if key still down after one second delay enter menu mode, else treat as simple reset
			if (handler_dev_display_read(RREG_KEY_SCAN) == KEY(RESET)) {
				app_state = S_MENU;
			} else {
				goto reset;
			}
			break;
		}
	}

	return 0;
}

bool change_settings_ui(menu_e *menu, u1_t key, bool *skip_first, cfg_t *cfg)
{
	int i;
	bool save_cfg = FALSE;
	s1_t ki;

	// scroll the menu list forward
	if (key == KEY(TI)) {
		if (*skip_first) {
			*skip_first = FALSE;
		} else {
			*menu = *menu+1;
			if (*menu == M_LAST) *menu = 0;
		}
menu_up_down:
		dsp_7seg_str(DSP_LEFT, menu_str[*menu], DSP_CLEAR);
		if (((*menu == M_IP) || (*menu == M_NM) || (*menu == M_GW))) {
			display_ipaddr(cfg->if_ipinfo[*menu-M_IP]);
		}
		wait_key_release();
	} else

	// scroll the menu list backward
	if (key == KEY(FREQ)) {
		if (*skip_first) {
			*skip_first = FALSE;
		} else {
			if (*menu == 0) *menu = M_LAST;
			*menu = *menu-1;
		}
		goto menu_up_down;
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
		u4_t td, tref = timer_ms(), tcmp = 256, tinc = 32;
		do {
			td = time_diff(timer_ms(), tref);
			if (((*menu == M_IP) || (*menu == M_NM) || (*menu == M_GW)) && (first_time || (td >= tcmp))) {
				if (ki > 0) {
					cfg->if_ipinfo[*menu-M_IP][ki-1]++;
				} else {
					cfg->if_ipinfo[*menu-M_IP][-ki-1]--;
				}
				display_ipaddr(cfg->if_ipinfo[*menu-M_IP]);
				first_time = FALSE;
				tcmp += tinc;
			}
		} while (key_down());
		save_cfg = TRUE;	// change has been made
	}
	
	return save_cfg;
}
