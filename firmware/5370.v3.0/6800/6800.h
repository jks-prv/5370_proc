#ifndef _6800_H_
#define _6800_H_

#include "5370.h"

// 6800 opcodes
#define N_OP		256
#define N_LEGIT_OP	197

#define INT_NMI  1
#define INT_FIRQ 2
#define INT_IRQ  4

// Macros controlling the read & write of memory/device space.
// Written this way to facilitate insertion of debugging code.
// The -O3 optimizer does a good job of making all of this fast.

#ifdef REG_RECORD
 #define READDEV(a) readDev(a, iCount, rPC, 0, getI())
 #define WRITEDEV(a,d) writeDev(a, d, iCount, rPC, 0, getI())
#else
 #define READDEV(a) readDev(a)
 #define WRITEDEV(a,d) writeDev(a, d)
#endif

#define readByte(rb_pAddr) \
({ \
	u2_t rb_addr = rb_pAddr; \
	u1_t rb_data; \
	\
	/* & 0x7fff because 5370B std dev firmware references ROM with bit 15 set */ \
	if ((rb_addr & ROM_MASK) == ROM_START) rb_data = rom[rb_addr & 0x7fff]; else \
	if ((rb_addr & RAM_MASK) != 0) rb_data = ram[rb_addr]; else \
	rb_data = READDEV(rb_addr); \
	rb_data; \
})

// readByteNoROM helps optimize the following case: INSN(0x6c, inc, _08_IDX(0x0000); RMW_8(tU8 = inc(tU8)) ); et al.
// i.e. _08_IDX means tAddr = rX + rom_const, which could point anywhere, but RMW means it can't be ROM due to the write.
// So readByteNoROM is used in the implementation of RMW_8.
// The -O3 optimizer can't catch this because tAddr isn't constant in the _08_IDX case.
//
// Normally -O3 can optimize readByte because tAddr is constant for the inline ROM code.

#define readByteNoROM(rb_pAddr) \
({ \
	u2_t rb_addr = rb_pAddr; \
	u1_t rb_data; \
	\
	if ((rb_addr & RAM_MASK) != 0) rb_data = ram[rb_addr]; else \
	rb_data = READDEV(rb_addr); \
	rb_data; \
})

#define readWord(pAddr) \
({ \
	u2_t addr = pAddr; \
	u1_t hi, lo; \
	\
	hi = readByte(addr); \
	addr++; \
	lo = readByte(addr); \
	(hi << 8) | lo; \
})

// assumes push/pull always goes to RAM and never device space
#define pullByte(pb_addr) \
	ram[pb_addr]

#define pullWord(pAddr) \
({ \
	u2_t addr = pAddr; \
	u1_t hi, lo; \
	\
	hi = pullByte(addr); \
	addr++; \
	lo = pullByte(addr); \
	(hi << 8) | lo; \
})

#define writeByte(rb_pAddr, rb_pData) \
({ \
	u2_t rb_addr = rb_pAddr; \
	u1_t rb_data = rb_pData; \
	if (((rb_addr) & RAM_MASK) != 0) { \
		ram[rb_addr] = rb_data; \
	} else { \
		WRITEDEV(rb_addr, rb_data); \
	} \
})

#define writeWord(pAddr, p16) \
({ \
	u2_t addr = pAddr; \
	u2_t d16 = p16; \
	u1_t d8; \
	\
	d8 = d16 >> 8; \
	writeByte(addr, d8); \
	addr++; \
	d8 = d16 & 0xff; \
	writeByte(addr, d8); \
})

#define pushByte(pb_addr, pb_data) \
	ram[pb_addr] = pb_data;

#define pushWord(pAddr, p16) \
({ \
	u2_t addr = pAddr; \
	u2_t d16 = p16; \
	u1_t d8; \
	\
	d8 = d16 >> 8; \
	pushByte(addr, d8); \
	addr++; \
	d8 = d16 & 0xff; \
	pushByte(addr, d8); \
})

extern u1_t rom_image[];
extern u1_t ram_image[];

#if defined(DEBUG) || defined(INSN_TRACE)

extern char *deco[N_OP];

#define	_R			0x00000001
#define	_W			0x00000002
#define	_RMW		0x00000004
#define	_8			0x00000008
#define	_16			0x00000010
#define	_R8			0x00000020
#define	_W8			0x00000040
#define	_R16		0x00000080
#define	_W16		0x00000100

#define	_BRANCH		0x00000200
#define	_COND		0x00000400
#define	_CALL		0x00000800
#define	_RTN		0x00001000

#define	_DFLOW		0x00002000
#define	_rA			0x00004000
#define	_rB			0x00008000
#define	_LD			0x00010000
#define	_AND		0x00020000
#define	_OR			0x00040000
#define	_XOR		0x00080000
#define	_BIT		0x00100000

#define	_SKIP_START	0x00200000
#define	_SKIP_STOP	0x00400000

#define	_REL		0x04000000
#define	_IMM		0x08000000
#define	_WIM		0x10000000
#define	_DIR		0x20000000
#define	_EXT		0x40000000
#define	_IDX		0x80000000

typedef struct {
	u4_t flags;
} insn_attrs_t;

#endif

#endif
