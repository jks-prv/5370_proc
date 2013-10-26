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

void xit(int err)
{
	void exit(int);
	
	fflush(stdout);
	exit(err);
}

void panic(char *str)
{
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
	perror(str);
	panic("sys_panic");
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
