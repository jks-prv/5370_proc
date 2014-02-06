#ifndef _5370_H_
#define _5370_H_

#include "5370_regs.h"
#include "6800.h"

#define	DEV_HPIB	0
#define	DEV_ARM		1
#define	DEV_DSP		2
#define	N_DEV		3

#define	DEV_START	0x0000
#define	DEV_SIZE	0x0080

#define	ROM_START	0x6000
#define	ROM_MASK	0x6000
#define	ROM_SIZE	0x2000

#define	RAM_START	0x0080
#define	RAM_MASK	0x0180
#define	RAM_SIZE	384

#define	ADDR_DEV(o)		((o) & 0xf0)
#define	ADDR_HPIB(o)	(0x0000 + (o))		// r/w 0x00..0x03
#define	ADDR_SVC(o)		(0x0008 + (o))		// 0x08..?
#define	ADDR_ARM(o)		(0x0050 + (o))		// 0x50..0x5f
#define	ADDR_DSP(o)		(0x0060 + (o))		// 0x60..0x7f
#define	ADDR_LEDS(o)	(0x006f - (o))		// w/o 0x6f..0x63	note reversed addressing and only 0-12
#define	ADDR_7SEG(o)	(0x0070 + (o))		// w/o 0x70..0x7f
#define	ADDR_RAM(o)		(0x0080 + (o))		// 0x80..0x1ff
#define	ADDR_ROM(o)		(0x6000 + (o))		// 0x6000..0x7fff
#define	ADDR_ROM2(o)	(0xC000 + (o))		// 0xc000..0xdfff

#ifdef DEBUG
	#define BIT_AREG(reg)	(1 << ((RREG_ ## reg) - ADDR_ARM(0)))

	#define DUMP_AREG(areg, addr, data) \
	({ \
		u2_t a = RREG_ ## areg; \
		u2_t bit = BIT_AREG(areg); \
		if ((addr == a) && (dump_regs & bit)) { printf("%s 0x%x %d  ", # areg, data, data); dump_regs &= ~bit; if (!dump_regs) printf("\n"); } \
	})

	void dev_decode_regs(insn_attrs_t *ia, u1_t rw, u2_t addr, u1_t data);
#else
	#define DUMP_AREG(areg, addr, data)
#endif

u1_t bus_read(u2_t addr);
void bus_write(u2_t addr, u1_t data);

#ifdef REG_RECORD
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
