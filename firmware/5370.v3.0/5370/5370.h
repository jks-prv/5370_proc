#ifndef _5370_H_
#define _5370_H_

#include "5370_regs.h"

#define	DEV_HPIB	0
#define	DEV_ARM		1
#define	DEV_DSP		2
#define	N_DEV		3

#define	DEV_SPACE_SIZE	0x80

#define	ROM_START	0x6000
#define	ROM_MASK	0x6000
#define	ROM_SIZE	0x2000

#define	RAM_START	0x0080
#define	RAM_MASK	0x0180
#define	RAM_SIZE	384

#define	ADDR_HPIB(o)	(0x0000 + (o))		// r/w 0x00..0x03
#define	ADDR_SVC(o)		(0x0008 + (o))		// 0x08..?
#define	ADDR_ARM(o)		(0x0050 + (o))		// 0x50..0x5f
#define	ADDR_DSP(o)		(0x0060 + (o))		// 0x60..0x7f
#define	ADDR_LEDS(o)	(0x006f - (o))		// w/o 0x6f..0x63	note reversed addressing and only 0-12
#define	ADDR_7SEG(o)	(0x0070 + (o))		// w/o 0x70..0x7f
#define	ADDR_RAM(o)		(0x0080 + (o))		// 0x80..0x1ff
#define	ADDR_ROM(o)		(0x6000 + (o))		// 0x6000..0x7fff
#define	ADDR_ROM2(o)	(0xC000 + (o))		// 0xc000..0xdfff

u1_t bus_read(u2_t addr);
void bus_write(u2_t addr, u1_t data);

#if defined(HPIB_RECORD) || defined(HPIB_SIM_DEBUG)
	u1_t readDev(u2_t addr, u4_t iCount, u2_t rPC, u1_t n_irq, u4_t irq_masked);
	void writeDev(u2_t addr, u1_t data, u4_t iCount, u2_t rPC, u1_t n_irq, u4_t irq_masked);
#else
	u1_t readDev(u2_t addr);
	void writeDev(u2_t addr, u1_t data);
#endif

void meas_extend_example_init();
void meas_extend_example(u1_t key);

void find_bug();

#endif
