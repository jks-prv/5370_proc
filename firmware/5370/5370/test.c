#include <sys/types.h>

#include "boot.h"
#include "misc.h"
#include "5370.h"
#include ARCH_INCLUDE

#include <sys/file.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <errno.h>
#include <setjmp.h>
#include <ctype.h>

#ifdef BUS_TEST

void  _w(u2_t a, u1_t d)
{
	printf("write @ %3x = 0x%02x\n", a, d);
	bus_write(a, d);
}

u1_t _r(u2_t a)
{
	u1_t d;
	d = bus_read(a);
	//printf("read @ %3x = 0x%02x\n", a, d);
	//printf("read @ %3x = %02x %08x %08x %08x\n", a, d,
	//	GPIO_OUT(0) & G0_ADDR, GPIO_OUT(1) & G1_ADDR, GPIO_OUT(3) & G3_ADDR);
	return d;
}

void bus_test()
{
	u4_t i, a, a2, d, d2, dr, dw, c, m, m2;
	volatile u4_t v;

//#define TEST_BUS
#ifdef TEST_BUS

	a = d = 0;
	while (1) {
		a++;
		if ((a&0x3fff)==0) d++;
		//v = GPIO_IN(0);
		//_r(0);
		_w(a&0x7f, a&0xff);
		//printf("0x%02x\n", bus_read(RREG_A16_SWITCHES));
	#if 0
		bus_write(0x70, 0xff);
		bus_write(0x71, 0xf0);
		//bus_read(0x70);
		//bus_write(0x71, 0xff);
		//bus_write(0x71, 0xf0);
	#endif
	#if 0
		printf("delay delay delay delay ");
		//bus_write(0x00a0, 0x00);
		//bus_read(0x0000);
		//bus_read(0x0001);
		//bus_read(0x0002);
		//bus_read(0x0003);
	#endif
	#if 0
		bus_write(0x00a0, 0x55);
		bus_write(0x0001, 0x01);
		bus_write(0x0002, 0x02);
		bus_write(0x0003, 0x03);
		bus_write(0x0003, 0x84);
		bus_read(0x0000);
	#endif
	}

	#if 0
	// test for this in the bus_read() routine and select read method
	int direct_cmp = 1;
	u4_t t0, t1;
	
	a = 0x7d00;
	direct_cmp = 0;
	t0 = bus_read(0x00);
	direct_cmp = 1;
	t1 = bus_read(0x00);
	printf("test bus read @0x%02x 0x%08x 0x%08x\n", a, t0, t1);
	
	a = 0x7d01;
	direct_cmp = 0;
	t0 = bus_read(0x00);
	direct_cmp = 1;
	t1 = bus_read(0x00);
	printf("test bus read @0x%02x 0x%08x 0x%08x\n", a, t0, t1);
	
	a = 0x7d02;
	direct_cmp = 0;
	t0 = bus_read(0x00);
	direct_cmp = 1;
	t1 = bus_read(0x00);
	printf("test bus read @0x%02x 0x%08x 0x%08x\n", a, t0, t1);
	
	a = 0x7d03;
	direct_cmp = 0;
	t0 = bus_read(0x00);
	direct_cmp = 1;
	t1 = bus_read(0x00);
	printf("test bus read @0x%02x 0x%08x 0x%08x\n", a, t0, t1);
	
	//bus_write(0xa0, 0x03);
	xit(0);
	#endif
	
	#if 0
	for (i=0; i<100000000; i++) {
		//printf("."); fflush(stdout);
		bus_write(0x00a0, 0x00);
		bus_read(0x00a0);
	#if 0
		bus_write(0x00a0, 0x00);
		bus_write(0x00a1, 0x01);
		bus_write(0x00a2, 0x02);
		bus_write(0x00a3, 0x03);
		bus_write(0x00a3, 0x84);
	#endif
	}
	#endif
	
	//w(0x80, 0x55);
	//w(0x10, 0);
	//r(0x80);
	//r(0x80);
	//r(0x80);
	//r(0x80);
	//r(0x60);
	//w(0x10, 0x1);
	//w(0x10, 0x0);

	xit(0);
#endif

//#define TEST_HPIB
#ifdef TEST_HPIB
	int i;
	
	for (i=0; i<100000000; i++) {
		bus_read(0x02);
		bus_write(0x02, 0x00);
		bus_write(0x02, 0xff);
		bus_write(0x02, 0x00);
		bus_write(0x02, 0xff);
	}
	xit(0);
#endif
	
//#define HI_PHK
#ifdef HI_PHK
	{
	int i, j;
	u1_t k;
	#define H	0x20|0x02|0x40|0x10|0x04
	#define I	0x20|0x10
	#define P	0x20|0x10|0x01|0x02|0x40
	#define O	0x01|0x02|0x04|0x08|0x10|0x20
	#define U	0x02|0x04|0x08|0x10|0x20
	#define L	0x08|0x10|0x20
	#define E	0x01|0x08|0x10|0x20|0x40
	#define N	0x01|0x02|0x04|0x10|0x20
	#define G	0x01|0x04|0x08|0x10|0x20
	#define DASH	0x40
	#define PERIOD	0x80
	bus_write(0x70, H);
	bus_write(0x71, I);
	bus_write(0x72, P);
	bus_write(0x73, O);
	bus_write(0x74, U);
	bus_write(0x75, L);
	bus_write(0x76, DASH);
	bus_write(0x77, H);
	bus_write(0x78, E);
	bus_write(0x79, N);
	bus_write(0x7a, N);
	bus_write(0x7b, I);
	bus_write(0x7c, N);
	bus_write(0x7d, G);
	bus_write(0x7e, PERIOD);
	bus_write(0x7f, PERIOD);
	xit(0);
	
	for (j = 0; j < 1000000; j++) {
		bus_write(0x7f, 1 << (j&0x7));
		usleep(100000);
	}
	xit(0);
	}
#endif

//#define TEST_DISPLAY
#ifdef TEST_DISPLAY
	printf("test display..\n");
	
	{
	int i, j;
	i = 0;

	#if 0
	do {
		printf("x"); fflush(stdout);
		bus_write(0x66, 0x01);
		bus_write(0x67, 0x00);
		usleep(1000000);
		printf("."); fflush(stdout);
		bus_write(0x66, 0x00);
		bus_write(0x67, 0x00);
		usleep(1000000);
	} while (1);
	#endif

	#if 0
	do {
		delay(1000);
		bus_write(0x69, 0x08);
		delay(1000);
		bus_write(0x69, 0x00);
	} while (1);
	#endif

	#if 1
	do {
		delay(100);
	#if 1
		for (j = 0; j < 16; j++) {
			bus_write(0x70 + j, 1 << (i&0x7));
		}
	#endif
		for (j = 0; j < 16; j++) {
			bus_write(0x60 + j, (i&1)? 0xf:0);
		}
		i++;
	} while (1);
	#endif

	xit(0);
	}
#endif

#define TEST_KEYS
#ifdef TEST_KEYS
	printf("test keys..\n");

	{
	int i, j, col;
	u1_t k;
	i = 0;
	col = 0;
	do {
		k = bus_read(RREG_KEY_SCAN);
		if (((k&0xf) != 0xf)) {
		//{
			printf("%02x ", k);
			fflush(stdout);
			//printf("%02x-%02x ", bus_read(RREG_A16_SWITCHES), k);
			col += 3; if (col > 100) { printf("\n"); col = 0; }
		}
		delay(100);
	#if 1
		for (j = 0; j < 16; j++) {
			bus_write(0x70 + j, 1 << (i&0x7));
		}
	#endif
	#if 1
		for (j = 0; j < 16; j++) {
			bus_write(0x60 + j, (i&1)? 0xf:0);
		}
		i++;
	#endif
	} while (1);
	xit(0);
	}
#endif

//#define TEST_RAM_CLK10
#ifdef TEST_RAM_CLK10
	{
	int i;
	#if 0
	w(0x80, 0x00);
	for (i = 0; i < 0x100; i++) {
	//	w(0x80, (i & 1)? 0xff:0);
		r(0x80);
		r(0x80);
		r(0x80);
		r(0x80);
		sleep(1);
	}
	#else
	w(0x80, 0x00);
	for (i = 0; i < 0xfffffff; i++) {
		d = bus_read(0x80) & 0xff;
		if ((d&1) != 0) {
			printf("%x ", d); fflush(stdout);
		}
	}
	#endif
	xit(0);
	}
#endif

//#define TEST_ROM
#ifdef TEST_ROM
	//for (a = 0x6000; a < 0x6004; a++) {
	r(0x6000);
	r(0x6001);
	r(0xfffe);
	r(0xffff);
	//w(0x6000, 0xff);
	xit(0);
#endif

//#define TEST_RAM
#ifdef TEST_RAM
	a2 = 0x200;
	m = 0x00;
	for (a = 0x80; a < a2; a++) {
		bus_write(a, a & 0xff);
	}
	{
	u1_t dsave[0x200];
	for (a = 0x80; a < a2; a++) {
		dsave[a] = bus_read(a);
	}
	for (a = 0x80; a < a2; a++) {
		c = a & 0xff;
		if ((dsave[a]|m) != (c|m)) {
			printf("@0x%x 0x%x != %x\n", a, c|m, dsave[a]|m);
		}
	}
	}
	
	//#define CHECK_BACKWARDS
	#ifdef CHECK_BACKWARDS
	for (a = a2-1; a >= 0x80; a--) {
		d = bus_read(a);
		c = a & 0xff;
		if ((d|m) != (c|m))
			printf("@0x%x 0x%x != %x\n", a, c|m, d|m);
	}
	#endif
#endif

//#define TEST_RAM2
#ifdef TEST_RAM2
	for (a = 0x80; a <= 0x1ff; a++) {
		bus_write(a, a);
	}
	for (a = 0x80; a < 0x1ff; a++) {
		d = bus_read(a);
		if (d != a)
			printf("@0x%x != %x\n", a, d);
	}
#endif

	xit(0);
}

#endif
