#ifndef _6800_H_
#define _6800_H_

#include "5370.h"

// 6800 opcodes
#define N_OP		256
#define N_LEGIT_OP	197


// Macros controlling the read & write of memory/device space.
// Written this way to facilitate insertion of debugging code.
// The -O3 optimizer does a good job of making all of this fast.

#if defined(HPIB_RECORD) || defined(HPIB_SIM_DEBUG)
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
	if ((rb_addr & ROM_MASK) == ROM_START) rb_data = rom[rb_addr]; else \
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

extern u1_t rom_image[ROM_SIZE];

#if defined(DEBUG) || defined(INSN_TRACE)
 extern char *deco[N_OP];
#endif

#endif
