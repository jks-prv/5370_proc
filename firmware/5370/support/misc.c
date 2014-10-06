#include "boot.h"
#include "sim.h"
#include "5370.h"
#include "front_panel.h"
#include "net.h"
#include "web.h"

#include <sys/file.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <setjmp.h>
#include <ctype.h>
#include <syslog.h>
#include <stdlib.h>
#include <stdarg.h>


#ifdef NET_PRINTF
 #undef printf
#endif

void xit(int err)
{
	void exit(int);
	
	fflush(stdout);
	delay(1000);	// needed for syslog messages to be properly recorded
	exit(err);
}

void _panic(char *str, char *file, int line)
{
	if (background_mode) {
		syslog(LOG_ERR, "PANIC: \"%s\" %s line %d\n", str, file, line);
	}
	
	printf("PANIC: \"%s\" %s line %d\n", str, file, line);

#ifndef CLIENT_SIDE
	dsp_7seg_str(DSP_LEFT, "panic", DSP_CLEAR);
	dsp_7seg_str(POS(6), str, DSP_NO_CLEAR);
#endif

	xit(-1);
}

void _sys_panic(char *str, char *file, int line)
{
	if (background_mode) {
		syslog(LOG_ERR, "%s: %m\n", str);
	}
	
	perror(str);
	_panic("sys_panic", file, line);
}

#define VBUF 4096

void lprintf(char *fmt, ...)
{
	int sl;
	char *s;
	va_list ap;
	
	if ((s = malloc(VBUF)) == NULL)
		panic("log malloc");

	va_start(ap, fmt);
	vsnprintf(s, VBUF, fmt, ap);
	va_end(ap);
	sl = strlen(s);		// because vsnprintf returns length disregarding limit, not the actual

	if (background_mode) {
		syslog(LOG_INFO, "hp5370d: %s", s);
	}

#ifndef CLIENT_SIDE
	net_send(NET_TELNET, s, sl, NO_COPY(TRUE), FLUSH(TRUE));
	app_to_webserver(s, sl);
#endif

	printf("%s", s);
	free(s);
}

int split(char *cp, int *argc, char *argv[], int nargs)
{
	int n=0;
	char **ap;
	
	for (ap = argv; (*ap = strsep(&cp, " \t\n")) != NULL;) {
		if (**ap != '\0') {
			n++;
			if (++ap >= &argv[nargs])
				break;
		}
	}
	
	return n;
}

// assumes no phase wrap between t1 & t2
u4_t time_diff(u4_t t1, u4_t t2)
{
	u4_t diff;
	
	if (t1 >= t2)
		diff = t1 - t2;
	else
		diff = 0xffffffff - t2 + t1;
	
	return diff;
}

void delay(u4_t msec)
{
	u4_t tref = timer_ms(), diff;
	
	do {
		diff = time_diff(timer_ms(), tref);
	} while (diff < msec);
}

#ifdef REG_RECORD

typedef struct {
	t_hr_stamp stamp;
	u2_t addr;
	u1_t data;
	reg_type_t type;
	u4_t dup;
	u4_t time;
	char *str;
} hr_buf_t;

#define NCHAN 2
#define NHP (256*1024)

static hr_buf_t hr_buf[NCHAN][NHP];
static u4_t nhp[NCHAN] = {0, 0};
static t_hr_stamp hr_stamp;
static u4_t last_iCount = 0;

void reg_record(int chan, reg_type_t type, u2_t addr, u1_t d, u4_t time, char *str)
{
	assert(chan < NCHAN);
	hr_buf_t *hp = &hr_buf[chan][nhp[chan]];
	
	if (type == REG_RESET) { nhp[chan] = 0; return; }

	if (nhp[chan] < NHP) {
		//if ((nhp[chan] > 0) && (addr == (hp-1)->addr) && (d == (hp-1)->data) &&
		//	((type != REG_STR) && ((hp-1)->type == type))) {
		if (0) {
			(hp-1)->dup++;
		} else {
			hp->stamp = hr_stamp;
			hp->addr = addr;
			hp->type = type;
			hp->data = d;
			hp->dup = 1;
			hp->time = time;
			hp->str = str;
			hp++;
			nhp[chan]++;
		}
	}
	if (nhp[chan] == NHP) {
		printf("NHP %d *************************************************\n", NHP);
		nhp[chan]++;
	}
}

// stamp the recorded hpib bus cycles with additional info
void reg_stamp(int write, u4_t iCount, u2_t rPC, u1_t n_irq, u4_t irq_masked)
{
#if 0
	hr_buf_t *hp = &hr_buf[0][nhp[chan]];

	hr_stamp.iCount = iCount;
	hr_stamp.rPC = rPC;
	hr_stamp.n_irq = n_irq;
	hr_stamp.irq_masked = irq_masked;
	
	// record interrupt events
	if (write && n_irq) {
		if (nhp[chan] < NHP) {
			hp->stamp = hr_stamp;
			hp++;
			nhp[chan]++;
		}
	}
#endif
}

t_hr_stamp *reg_get_stamp()
{
	return &hr_stamp;
}

void reg_dump(u1_t chan, callback_t rdecode, callback_t wdecode, callback_t sdecode)
{
	hr_buf_t *p;
	
	printf("NHP %d\n", nhp[chan]);
	for (p = &hr_buf[chan][0]; p != &hr_buf[chan][nhp[chan]]; p++) {
#if 0
		printf("%07d +%5d @%04x%c #%4d ", p->stamp.iCount, p->stamp.iCount - last_iCount, p->stamp.rPC,
			p->stamp.irq_masked? '*':' ', p->dup);
		last_iCount = p->stamp.iCount;
#endif
		if (p->stamp.n_irq) {
			//printf("---- IRQ ----\n");
		} else {
			switch (p->type) {
				case REG_READ:  rdecode(p->addr, p->data, p->dup, p->time, p->str); break;
				case REG_WRITE: wdecode(p->addr, p->data, p->dup, p->time, p->str); break;
				case REG_STR:   sdecode(p->addr, p->data, p->dup, p->time, p->str); break;
				default: break;
			}
			//printf("\n");
		}
	}
}

void reg_cmp(callback_t adecode)
{
	int max, min, n=0;
	hr_buf_t *p, *q, *r;
	
	printf("NHP %d %d %d\n", nhp[0], nhp[1], nhp[0]-nhp[1]);
	max = nhp[0]>nhp[1]? nhp[0]:nhp[1];
	min = nhp[0]<nhp[1]? nhp[0]:nhp[1];
	for (p = &hr_buf[0][0], q = &hr_buf[1][0]; p != &hr_buf[0][max]; p++, q++, n++) {
		if ((n < min) && ((p->addr != q->addr) || (p->data != q->data) || (p->type != q->type))) {
			printf("%d: ", n);
			if (p->addr != q->addr) {
				printf("addr "); adecode(p->addr, 0, p->type, p->time, p->str);
				printf(":"); adecode(q->addr, 0, q->type, p->time, p->str); printf(" ");
			} else {
				printf("addr "); adecode(p->addr, 0, p->type, p->time, p->str); printf(" ");
			}
			if (p->data != q->data) {
				printf("data %02x:%02x ", p->data, q->data);
			} else {
				printf("data %02x ", p->data);
			}
			if (p->type != q->type) {
				printf("type %d:%d ", p->type, q->type);
			} else {
				printf("type %d ", p->type);
			}
			printf("\n");
		}
		if (n >= min) {
			r = nhp[0]>nhp[1]? p:q;
			printf("%d: ", n);
			printf("addr "); adecode(r->addr, 0, r->type, p->time, p->str); printf(" ");
			printf("data %02x ", r->data);
			printf("type %d ", r->type);
			printf("(no cmp)\n");
		}
	}
}

#endif
