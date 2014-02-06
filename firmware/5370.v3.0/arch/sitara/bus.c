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
	{807, 0x090, PMUX_OUTPUT | PMUX_M2},
	{810, 0x098, PMUX_INOUT | PMUX_M7},
	{811, 0x034, PMUX_INOUT | PMUX_M7},
	{812, 0x030, PMUX_INOUT | PMUX_M7},
	{813, 0x024, PMUX_INOUT | PMUX_M7},
	{814, 0x028, PMUX_INOUT | PMUX_M7},
	{815, 0x03c, PMUX_INOUT | PMUX_M7},
	{816, 0x038, PMUX_INOUT | PMUX_M7},
	{817, 0x02c, PMUX_INOUT | PMUX_M7},
	{818, 0x08c, PMUX_OUTPUT | PMUX_M7},
	{819, 0x020, PMUX_OUTPUT | PMUX_M7},
	{826, 0x07c, PMUX_OUTPUT | PMUX_M7},

	{911, 0x070, PMUX_OUTPUT | PMUX_M7},
	{916, 0x04c, PMUX_INPUT | PMUX_M7},
	{917, 0x15c, PMUX_INPUT | PMUX_M7},
	{918, 0x158, PMUX_OUTPUT | PMUX_M7},
	{921, 0x154, PMUX_OUTPUT | PMUX_M7},
	{922, 0x150, PMUX_OUTPUT | PMUX_M7},
	{923, 0x044, PMUX_OUTPUT | PMUX_M7},
	{924, 0x184, PMUX_OUTPUT | PMUX_M7},
	{926, 0x180, PMUX_OUTPUT | PMUX_M7},
	{927, 0x1a4, PMUX_OUTPUT | PMUX_M7},
	{930, 0x198, PMUX_OUTPUT | PMUX_M7},
	
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
			printf("pin %d pinmux is 0x%x should be 0x%x\n", p->pin, pm, p->pinmux);
			fail = 1;
		}
	}
	if (fail) panic("check if cape-bone-5370 device tree loaded properly");
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

void bus_gpio_init()
{
	// for hpib_fast_binary()
	CONV_ADDR(RREG_N0ST, a_n0st);
	CONV_ADDR(WREG_O1, a_o1);
	CONV_ADDR(WREG_O2, a_o2);
	CONV_ADDR(WREG_O3, a_o3);
	CONV_ADDR(RREG_N1N2H, a_n1n2h);
	CONV_ADDR(RREG_N1N2L, a_n1n2l);
	CONV_ADDR(RREG_N0H, a_n0h);
	CONV_ADDR(RREG_N0L, a_n0l);
	
	CONV_ADDR_DATA(WREG_O3, WREG_O3_RST, ad_rst);
	CONV_ADDR_DATA(WREG_O2, WREG_O2_ENA, ad_ena);
	CONV_ADDR_DATA(WREG_O2, WREG_O2_ARM, ad_arm);
	CONV_ADDR_DATA(WREG_O2, WREG_O2_IDLE, ad_idle);

#if 0
	// shows redundant gpio set/clr writes for use in setting qual mask in FAST_WRITE_GPIO_QUAL_CYCLE() below
	CONV_ADDR_DATA_PRF(ad_rst);
	CONV_ADDR_DATA_PRF(ad_ena);
	CONV_ADDR_DATA_PRF(ad_arm);
#endif

	// for PRU
	CONV_COPY_ADDR(a_n0st, pru);
	CONV_COPY_ADDR(a_o3, pru);
	CONV_WRITE_DATA(WREG_O3_N0_CLR_OVFL, d_n0_clr_ovfl);
	CONV_COPY_WRITE_DATA(d_n0_clr_ovfl, pru);
	CONV_WRITE_DATA(WREG_O3_N3_CLR_OVFL, d_n3_clr_ovfl);
	CONV_COPY_WRITE_DATA(d_n3_clr_ovfl, pru);
}

// FIXME: rewrite to run on the PRU?
u4_t hpib_fast_binary(s2_t *ibp, u4_t nloop)
{
	int i;
	u4_t t;
	s2_t *bp = ibp;
	u4_t n0st, n0st2, ad_h, ad_l;
	s4_t n1n2, n0;
	
	BUS_CLK_STOP();
	TEST1_SET();

	for (i = 0; i < nloop; i++) {
		
		FAST_WRITE_ENTER();
		FAST_WRITE_GPIO_QUAL_CYCLE(ad_rst, 1,1,1,1,1,1,1,1);
		FAST_WRITE_GPIO_QUAL_CYCLE(ad_ena, 1,1,1,1,1,1,1,1);
		FAST_WRITE_GPIO_QUAL_CYCLE(ad_arm, 0,0,1,1,0,0,0,0);
		FAST_WRITE_EXIT();

		do {
			FAST_READ_GPIO1_CYCLE(1, n0st, a_n0st);
		} while (n0st & N0ST_EOM_GPIO);

		FAST_WRITE_ENTER();
		FAST_WRITE_GPIO_CYCLE(ad_idle);
		FAST_WRITE_EXIT();

		FAST_READ_GPIO_CYCLE(a_n0st, n0st);
		FAST_READ2_GPIO_CYCLE(a_n1n2h, ad_h, ad_l);
		//n1n2 = ((n0st & N0ST_N1_GPIO) << 3) | ((n0st2 & N0ST_N2_GPIO) >> 11) | (ad_h << 8) | ad_l;
		n1n2 = ((n0st & N0ST_N1N2) << 16) | (ad_h << 8) | ad_l;

		if (isActive(N0ST_N0_OVFL, n0st)) printf("N0 OVFL\n");
		
		// convert from 18-bit 2s complement
		if (n1n2 >= 0x20000) {
			n1n2 = n1n2 - 0x40000;
		}
		
		FAST_READ2_GPIO_CYCLE(a_n0h, ad_h, ad_l);
		n0 = (ad_h << 8) | ad_l;

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

	TEST1_CLR();
	BUS_CLK_START();
	
	return nloop * sizeof (*bp);
}
