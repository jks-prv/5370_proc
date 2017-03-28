#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdio.h>
#include <unistd.h>

#include "boot.h"
#include "5370.h"
#include "hpib.h"
#include "misc.h"
#include "chip.h"
#include "bus.h"
#include "pru_realtime.h"

typedef struct {
	u4_t pin;
	u4_t offset;
	u4_t pinmux;
} pinmux_t;

pinmux_t pins[] = {
	{807, 0x090, PMUX_OUTPUT | PMUX_M2},	// clk
	{810, 0x098, PMUX_INOUT  | PMUX_M7},	// d7
	{811, 0x034, PMUX_INOUT  | PMUX_M7},	// d6
	{812, 0x030, PMUX_INOUT  | PMUX_M7},	// d5
	{813, 0x024, PMUX_INOUT  | PMUX_M7},	// d4
	{814, 0x028, PMUX_INOUT  | PMUX_M7},	// d3
	{815, 0x03c, PMUX_INOUT  | PMUX_M7},	// d2
	{816, 0x038, PMUX_INOUT  | PMUX_M7},	// d1
	{817, 0x02c, PMUX_INOUT  | PMUX_M7},	// d0
	{818, 0x08c, PMUX_OUTPUT | PMUX_M7},	// oe
	{819, 0x020, PMUX_OUTPUT | PMUX_M7},	// dir
	{826, 0x07c, PMUX_OUTPUT | PMUX_M7},	// vma

	{911, 0x070, PMUX_OUTPUT | PMUX_M7},	// rst
	{916, 0x04c, PMUX_INPUT  | PMUX_M7},	// nmi
	{917, 0x15c, PMUX_INPUT  | PMUX_M7},	// irq
	{918, 0x158, PMUX_OUTPUT | PMUX_M7},	// rw
	{921, 0x154, PMUX_OUTPUT | PMUX_M7},	// a6
	{922, 0x150, PMUX_OUTPUT | PMUX_M7},	// a5
	{923, 0x044, PMUX_OUTPUT | PMUX_M7},	// a4
	{924, 0x184, PMUX_OUTPUT | PMUX_M7},	// a3
	{926, 0x180, PMUX_OUTPUT | PMUX_M7},	// a2
	{927, 0x1a4, PMUX_OUTPUT | PMUX_M7},	// a1
	{930, 0x198, PMUX_OUTPUT | PMUX_M7},	// a0
	
	{0, 0, 0}
};

u4_t g0_addr[DEV_SIZE], g1_addr[DEV_SIZE], g3_addr[DEV_SIZE];
u4_t g0_write[256], g1_write[256], g2_write[256];
u4_t gpio_read[0x400];

volatile u4_t *prcm, *pmux, *timer4;

void check_pmux()
{
	pinmux_t *p;
	u4_t pm;
	int fail=0;
	for (p=pins; p->pin; p++) {
		pm = pmux[(0x800 + p->offset) >> 2];
		if (pm != p->pinmux) {
			lprintf("pin %d pinmux is 0x%02x should be 0x%02x\n", p->pin, pm, p->pinmux);
			fail = 1;
		}
	}
	if (fail) panic("check if cape-bone-5370 device tree loaded properly");
}

void bus_reset()
{
	// FIXME: is BUS_OE default high when BBB is reset?
	GPIO_SET(2) = BUS_OE;	// Z
	GPIO_OUTPUT(2, BUS_OE);

    GPIO_INPUT(0, G0_DATA);
    GPIO_INPUT(1, G1_DATA);
    GPIO_INPUT(2, G2_DATA);

	GPIO_CLR(0) = BUS_LRW | BUS_LRST;		// assert reset
	GPIO_SET(0) = BUS_DIR;	// read
	GPIO_OUTPUT(0, BUS_LRW | BUS_DIR | BUS_LRST);
	
	GPIO_SET(1) = BUS_LVMA;
	GPIO_OUTPUT(1, BUS_LVMA);
	
    GPIO_OUTPUT(0, G0_ADDR);
    GPIO_OUTPUT(1, G1_ADDR);
    GPIO_OUTPUT(3, G3_ADDR);

    GPIO_INPUT(0, BUS_LIRQ);
    GPIO_INPUT(1, BUS_LNMI);

	GPIO_CLR(2) = BUS_OE;	// enable
	
	// setup 1.25 MHz 5370 default bus clock using a timer
	// assumes CLK_M_OSC = 6 MHz (24/4)
	TIMER_TCLR(4) = T_MODE;
	TIMER_TCRR(4) = -9;		// 6 MHz = 166.67 nS, 1.25 MHz / 2 (toggle) = 1600 nS
	TIMER_TLDR(4) = -9;		// 1600/166.67 = 9.6, using 9 gives 1.33 MHz
	TIMER_TCLR(4) = T_MODE | T_START;

	// FIXME need some delay?
	GPIO_SET(0) = BUS_LRST;
}

void bus_init()
{
	int i;
	
	// simplify scrambling somewhat by using lookup tables
	for (i=0; i < DEV_SIZE; i++) {
		g0_addr[i] = ((i & B2) << 12) | ((i & B3) << 12) | ((i & B5) >> 3) | ((i & B6) >> 3);
		g1_addr[i] = ((i & B4) << 13);
		g3_addr[i] = ((i & B0) << 16) | ((i & B1) << 18);
	}
	
	for (i=0; i < 256; i++) {
		g0_write[i] = ((i & B0) << 27) | ((i & B3) << 23) | ((i & B4) << 19);
		g1_write[i] = ((i & B1) << 13) | ((i & B2) << 13) | ((i & B5) << 7) | ((i & B6) << 7);
		g2_write[i] = ((i & B7) >> 3);
	}

#if 0
GPIO_IN(0) >> 18
#define BUS_LD4		0x00000020
#define BUS_LD3		0x00000100
#define BUS_LD0		0x00000200

GPIO_IN(1) >> 12
#define BUS_LD5		0x00000001
#define BUS_LD6		0x00000002
#define BUS_LD1		0x00000004
#define BUS_LD2		0x00000008

GPIO_IN(2) >> 0
#define BUS_LD7		0x00000010
#endif

	for (i=0; i < 0x400; i++) gpio_read[i] = 0;
	for (i=0; i < 256; i++) {
		gpio_read[((i & B0) << (9-0)) | ((i & B1) << (2-1)) | ((i & B2) << (3-2)) | ((i & B3) << (8-3)) |
			 ((i & B4) << (5-4)) | ((i & B5) >> (5-0)) | ((i & B6) >> (6-1)) | ((i & B7) >> (7-4))] = ~i & 0xff;	// remember: have to invert LDn
	}
	
    int mem_fd;

    mem_fd = open("/dev/mem", O_RDWR|O_SYNC);
    if (mem_fd < 0) sys_panic("/dev/mem");

    prcm = (volatile unsigned *) mmap(
        NULL,
        MMAP_SIZE,
        PROT_READ|PROT_WRITE,
        MAP_SHARED,
        mem_fd,
        PRCM_BASE
    );

    pmux = (volatile unsigned *) mmap(
        NULL,
        MMAP_SIZE,
        PROT_READ|PROT_WRITE,
        MAP_SHARED,
        mem_fd,
        PMUX_BASE
    );

    timer4 = (volatile unsigned *) mmap(
        NULL,
        MMAP_SIZE,
        PROT_READ|PROT_WRITE,
        MAP_SHARED,
        mem_fd,
        TIMER4_BASE
    );

    gpio0 = (volatile unsigned *) mmap(
        NULL,
        MMAP_SIZE,
        PROT_READ|PROT_WRITE,
        MAP_SHARED,
        mem_fd,
        GPIO0_BASE
    );

    gpio1 = (volatile unsigned *) mmap(
        NULL,
        MMAP_SIZE,
        PROT_READ|PROT_WRITE,
        MAP_SHARED,
        mem_fd,
        GPIO1_BASE
    );

    gpio2 = (volatile unsigned *) mmap(
        NULL,
        MMAP_SIZE,
        PROT_READ|PROT_WRITE,
        MAP_SHARED,
        mem_fd,
        GPIO2_BASE
    );

    gpio3 = (volatile unsigned *) mmap(
        NULL,
        MMAP_SIZE,
        PROT_READ|PROT_WRITE,
        MAP_SHARED,
        mem_fd,
        GPIO3_BASE
    );

    if (!prcm) sys_panic("mmap prcm");
    if (!pmux) sys_panic("mmap pmux");
    if (!timer4) sys_panic("mmap timer4");
    if (!gpio0) sys_panic("mmap gpio0");
    if (!gpio1) sys_panic("mmap gpio1");
    if (!gpio2) sys_panic("mmap gpio2");
    if (!gpio3) sys_panic("mmap gpio3");

	// enable GPIO module clocks
	PRCM_GPIO0 = MODMODE_ENA;
	PRCM_GPIO1 = MODMODE_ENA;
	PRCM_GPIO2 = MODMODE_ENA;
	PRCM_GPIO3 = MODMODE_ENA;
	//printf("CLKS: pmux 0x%x g0 0x%x spi0 0x%x g1 0x%x g2 0x%x g3 0x%x\n",
	//	PRCM_PMUX, PRCM_GPIO0, PRCM_SPI0, PRCM_GPIO1, PRCM_GPIO2, PRCM_GPIO3);

	PRCM_TIMER4 = MODMODE_ENA;
	//printf("CLKS: t4 0x%x t5 0x%x t6 0x%x t7 0x%x\n",
	//	PRCM_TIMER4, PRCM_TIMER5, PRCM_TIMER6, PRCM_TIMER7);

	check_pmux();
}

u4_t bus_fast_read(u4_t addr)
{
	u4_t t, data;
	
	BUS_CLK_STOP();
	FAST_READ_CYCLE(addr, data);
	BUS_CLK_START();
	
	return data;
}

void bus_fast_write(u4_t addr, u4_t data)
{
	BUS_CLK_STOP();

	FAST_WRITE_ENTER();
	FAST_WRITE_CYCLE(addr, data);
	FAST_WRITE_EXIT();

	BUS_CLK_START();
}

void bus_delay()
{
	PRCM_GPIO0 = MODMODE_ENA;
}


// to support PRU and hpib_fast_binary()

CONV_ADDR_DCL(a_n0st);
CONV_ADDR_DCL(a_o1);
CONV_ADDR_DCL(a_o2);
CONV_ADDR_DCL(a_o3);
CONV_ADDR_DCL(a_n1n2h);
CONV_ADDR_DCL(a_n1n2l);
CONV_ADDR_DCL(a_n0h);
CONV_ADDR_DCL(a_n0l);

CONV_ADDR_DATA_DCL(ad_rst);
CONV_ADDR_DATA_DCL(ad_ena);
CONV_ADDR_DATA_DCL(ad_arm);
CONV_ADDR_DATA_DCL(ad_idle);

CONV_DATA_DCL(d_n0_clr_ovfl);
CONV_DATA_DCL(d_n3_clr_ovfl);

void bus_pru_gpio_init()
{
	// for hpib_fast_binary()
#ifndef HPIB_FAST_BINARY_PRU
	CONV_ADDR(_, a_n0st, RREG_N0ST);
	CONV_ADDR(_, a_o1, WREG_O1);
	CONV_ADDR(_, a_o2, WREG_O2);
	CONV_ADDR(_, a_o3, WREG_O3);
	CONV_ADDR(_, a_n1n2h, RREG_N1N2H);
	CONV_ADDR(_, a_n1n2l, RREG_N1N2L);
	CONV_ADDR(_, a_n0h, RREG_N0H);
	CONV_ADDR(_, a_n0l, RREG_N0L);
	
	CONV_ADDR_DATA(_, ad_rst, WREG_O3, WREG_O3_RST);
	CONV_ADDR_DATA(_, ad_ena, WREG_O2, WREG_O2_ENA);
	CONV_ADDR_DATA(_, ad_arm, WREG_O2, WREG_O2_ARM);
	CONV_ADDR_DATA(_, ad_idle, WREG_O2, WREG_O2_IDLE);
#else
	CONV_ADDR(pru->_, a_n0st, RREG_N0ST);
	CONV_ADDR(pru->_, a_n0_h, RREG_N0H);
	CONV_ADDR(pru->_, a_n1n2_h, RREG_N1N2H);
	CONV_ADDR(pru->_, a_o3, WREG_O3);
	CONV_WRITE_DATA(pru->_, d_n0_clr_ovfl, WREG_O3_N0_CLR_OVFL);
	CONV_WRITE_DATA(pru->_, d_n3_clr_ovfl, WREG_O3_N3_CLR_OVFL);

	CONV_ADDR_DATA(pru2->_, ad_rst, WREG_O3, WREG_O3_RST);
	CONV_ADDR_DATA(pru2->_, ad_ena, WREG_O2, WREG_O2_ENA);
	CONV_ADDR_DATA(pru2->_, ad_arm, WREG_O2, WREG_O2_ARM);
	CONV_ADDR_DATA(pru2->_, ad_idle, WREG_O2, WREG_O2_IDLE);
#endif

#if 0
	// shows redundant gpio set/clr writes for use in setting qual mask in FAST_WRITE_GPIO_QUAL_CYCLE() below
	CONV_ADDR_DATA_PRF(ad_rst);
	CONV_ADDR_DATA_PRF(ad_ena);
	CONV_ADDR_DATA_PRF(ad_arm);
#endif
}

u4_t hpib_fast_binary(s2_t *ibp, u4_t nloop)
{
	int i;
	u4_t t;
	s2_t *bp = ibp;
	u4_t n0st, n0st2, n1n2_h, n1n2_l, n0_h, n0_l;
	s4_t n1n2, n0;

#ifdef HPIB_FAST_BINARY_PRU
	send_pru_cmd(PRU_BUS_CLK_STOP);
#else
	BUS_CLK_STOP();
#endif

	for (i = 0; i < nloop; i++) {

#ifdef HPIB_FAST_BINARY_PRU
		send_pru_cmd(PRU_TI_MEAS);

		CONV_READ_DATA(t, n0st, pru2->_, d_n0st);
		CONV_READ_DATA(t, n1n2_h, pru2->_, d_n1n2_h);
		CONV_READ_DATA(t, n1n2_l, pru2->_, d_n1n2_l);
		CONV_READ_DATA(t, n0_h, pru2->_, d_n0_h);
		CONV_READ_DATA(t, n0_l, pru2->_, d_n0_l);
#else
		FAST_WRITE_ENTER();
		FAST_WRITE_GPIO_QUAL_CYCLE(ad_rst, 1,1,1,1,1,1,1,1);
		FAST_WRITE_GPIO_QUAL_CYCLE(ad_ena, 1,1,1,1,1,1,1,1);
		// i.e. only gpio1 changes, gpio[023] same as previous write
		FAST_WRITE_GPIO_QUAL_CYCLE(ad_arm, 0,0,1,1,0,0,0,0);
		FAST_WRITE_EXIT();

		do {
			FAST_READ_GPIO1_CYCLE(1, n0st, a_n0st);
		} while (n0st & N0ST_EOM_GPIO);

		FAST_WRITE_ENTER();
		FAST_WRITE_GPIO_CYCLE(ad_idle);
		FAST_WRITE_EXIT();

		FAST_READ_GPIO_CYCLE(a_n0st, n0st);
		FAST_READ2_GPIO_CYCLE(a_n1n2h, n1n2_h, n1n2_l);
		FAST_READ2_GPIO_CYCLE(a_n0h, n0_h, n0_l);
#endif

		//n1n2 = ((n0st & N0ST_N1_GPIO) << 3) | ((n0st2 & N0ST_N2_GPIO) >> 11) | (n1n2_h << 8) | n1n2_l;
		n1n2 = ((n0st & N0ST_N1N2) << 16) | (n1n2_h << 8) | n1n2_l;

		if (isActive(N0ST_N0_OVFL, n0st)) printf("N0 OVFL\n");
		
		// convert from 18-bit 2s complement
		if (n1n2 >= 0x20000) {
			n1n2 = n1n2 - 0x40000;
		}
		
		n0 = (n0_h << 8) | n0_l;

		// convert from 16-bit sign-magnitude
		if ((n0st & N0ST_N0_POS_GPIO) == 0) {
			n0 = -n0;
		}

		// Just send the 2-byte n1n2 + n0 sum instead of the 5-bytes of n0st, n1n2 & n0 separately.
		// Can do this if n0 is multiplied by 256 first so it matches the 5.02 ns units of n1n2 (n0 is in 5 ns units).
		// Recall that 5/5.02 = 256/257, so n1n2/5.02 = n0/5 * 5/5.02 (= 256/257).
		// But n1n2 is already * 257 by the hardware, so we don't have to divide n0 by 257 here.
		// Just have to compute n0 * 256 to match the n1n2 units and be able to add them.
		// This sum is divided by 256 on the host side to convert the final result to 5 ns units.
		n0 = n0 << 8;
		n1n2 = n1n2 + n0;

		// TBD: hope that the sum fits in 16-bits (n1n2 & n0 are 18 & 16-bits, but typically of opposite sign).
		// If not, this condition could be detected at the start of measurement and the results rescaled to fit
		// (the host would have to be notified of this rescaled interpretation of the data).
//#define CHECK_N1N2_OVERFLOW
#ifdef CHECK_N1N2_OVERFLOW
		if (((n1n2 & 0xffff8000) != 0xffff8000) && ((n1n2 & 0xffff8000) != 0))
			printf("overflow n1n2 0x%x\r\n", (u4_t) n1n2);
#endif
		*bp++ = (s2_t) (u2_t) (n1n2 & 0xffff);
	}

#ifdef HPIB_FAST_BINARY_PRU
	send_pru_cmd(PRU_BUS_CLK_START);
#else
	BUS_CLK_START();
#endif
	
	return nloop * sizeof (*bp);
}
