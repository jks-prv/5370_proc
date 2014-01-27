#include "boot.h"
#include "sim.h"
#include "5370.h"
#include "front_panel.h"

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

void xit(int err)
{
	void exit(int);
	
	fflush(stdout);
	delay(1000);	// needed for syslog messages to be properly recorded
	exit(err);
}

void panic(char *str)
{
	if (background_mode) {
		syslog(LOG_ERR, "PANIC: %s\n", str);
	}
	
	printf("PANIC: %s\n", str);

#ifndef CLIENT_SIDE
	if (dsp_7seg_ok) {
		dsp_7seg_str(0, "panic", TRUE);
		dsp_7seg_str(6, str, FALSE);
	}
#endif

	xit(-1);
}

void sys_panic(char *str)
{
	if (background_mode) {
		syslog(LOG_ERR, "%s: %m\n", str);
	}
	
	perror(str);
	panic("sys_panic");
}

void lprintf(char *fmt, ...)
{
	char *s;
	va_list ap;
	char b[256];
	
	if ((s = malloc(256)) == NULL)
		panic("log malloc");

	va_start(ap, fmt);
	vsnprintf(s, 256, fmt, ap);
	va_end(ap);

	if (background_mode) {
		syslog(LOG_INFO, "hp5370d: %s", s);
	}

	printf("%s", s);
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
	u4_t tref = sys_now(), diff;
	
	do {
		diff = time_diff(sys_now(), tref);
	} while (diff < msec);
}

#ifdef REG_RECORD

typedef struct {
	t_hr_stamp stamp;
	u2_t addr;
	u1_t data;
	u1_t rwn;
	u4_t dup;
	u4_t time;
} hr_buf_t;

#define NCHAN 2
#define NHP (256*1024)

static hr_buf_t hr_buf[NCHAN][NHP];
static int nhp[NCHAN] = {0, 0};
static t_hr_stamp hr_stamp;
static u4_t last_iCount = 0;

void reg_record(int chan, u1_t rwn, u2_t addr, u1_t d, u4_t time)
{
	hr_buf_t *hp = &hr_buf[chan][nhp[chan]];
	
	if (chan < 0) { nhp[(-chan)-1] = 0; return; }

	if (nhp[chan] < NHP) {
		//if ((nhp[chan] > 0) && (addr == (hp-1)->addr) && (d == (hp-1)->data) && ((hp-1)->rwn == rwn)) {
		if (0) {
			(hp-1)->dup++;
		} else {
			hp->stamp = hr_stamp;
			hp->addr = addr;
			hp->rwn = rwn;
			hp->data = d;
			hp->dup = 1;
			hp->time = time;
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

void reg_dump(u1_t chan, callback_t rdecode, callback_t wdecode)
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
			if (p->rwn == REG_READ) {
				rdecode(p->addr, p->data, p->dup, p->time);
			} else {
				wdecode(p->addr, p->data, p->dup, p->time);
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
		if ((n < min) && ((p->addr != q->addr) || (p->data != q->data) || (p->rwn != q->rwn))) {
			printf("%d: ", n);
			if (p->addr != q->addr) {
				printf("addr "); adecode(p->addr, 0, p->rwn, p->time);
				printf(":"); adecode(q->addr, 0, q->rwn, p->time); printf(" ");
			} else {
				printf("addr "); adecode(p->addr, 0, p->rwn, p->time); printf(" ");
			}
			if (p->data != q->data) {
				printf("data %02x:%02x ", p->data, q->data);
			} else {
				printf("data %02x ", p->data);
			}
			if (p->rwn != q->rwn) {
				printf("rwn %d:%d ", p->rwn, q->rwn);
			} else {
				printf("rwn %d ", p->rwn);
			}
			printf("\n");
		}
		if (n >= min) {
			r = nhp[0]>nhp[1]? p:q;
			printf("%d: ", n);
			printf("addr "); adecode(r->addr, 0, r->rwn, p->time); printf(" ");
			printf("data %02x ", r->data);
			printf("rwn %d ", r->rwn);
			printf("(no cmp)\n");
		}
	}
}

#endif
