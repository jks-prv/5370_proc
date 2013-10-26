
// implementation of the core instructions
// these are combined with the various addressing modes to generate the specific opcodes

// instruction operand shorthand
#define IMM(x)		x;
#define R8(x)		tU8 = readByte(tAddr); x;
#define W8(x)		x; writeByte(tAddr, tU8);
#define RMW_8(x)	tU8 = readByteNoROM(tAddr); x; writeByte(tAddr, tU8);
#define R16(x)		tU16 = readWord(tAddr); x;
#define W16(x)		x; writeWord(tAddr, tU16);

#define inc(p8) \
({ \
	u1_t d8 = p8; \
	\
	d8++; \
	\
	modcVNZ(); \
	modsVNZ(d8); \
	SET_Vcond(d8 == 0x80); \
	\
	d8; \
})

#define dec(p8) \
({ \
	u1_t d8 = p8; \
	\
	d8--; \
	\
	modcVNZ(); \
	modsVNZ(d8); \
	SET_Vcond(d8 == 0x7f); \
	\
	d8; \
})

#define add(p8_1, p8_2) \
({ \
	u1_t d8_1 = p8_1, d8_2 = p8_2; \
	u2_t tS16; \
	\
	tS16 = (u2_t) d8_1 + (u2_t) d8_2; \
	\
	modcHCVNZ(); \
	modsVNZ(tS16 & 0xff); \
	SET_C8(tS16); \
	SET_V8(d8_1, d8_2, tS16); \
	SET_H8(d8_1, d8_2, tS16); \
	\
	(u1_t) (tS16 & 0xff); \
})

#define adc(p8_1, p8_2) \
({ \
	u1_t d8_1 = p8_1, d8_2 = p8_2; \
	u2_t tS16; \
	\
	tS16 = (u2_t) d8_1 + (u2_t) d8_2 + getC(); \
	\
	modcHCVNZ(); \
	modsVNZ(tS16 & 0xff); \
	SET_C8(tS16); \
	SET_V8(d8_1, d8_2, tS16); \
	SET_H8(d8_1, d8_2, tS16); \
	\
	(u1_t) (tS16 & 0xff); \
})

#define sub(p8_1, p8_2) \
({ \
	u1_t d8_1 = p8_1, d8_2 = p8_2; \
	u2_t tS16; \
	\
	tS16 = (u2_t) d8_1 - (u2_t) d8_2; \
	\
	modcCVNZ(); \
	modsVNZ(tS16 & 0xff); \
	SET_C8(tS16); \
	SET_V8(d8_1, d8_2, tS16); \
	\
	(u1_t) (tS16 & 0xff); \
})

#define sbc(p8_1, p8_2) \
({ \
	u1_t d8_1 = p8_1, d8_2 = p8_2; \
	u2_t tS16; \
	\
	tS16 = (u2_t) d8_1 - (u2_t) d8_2 - getC(); \
	\
	modcCVNZ(); \
	modsVNZ(tS16 & 0xff); \
	SET_C8(tS16); \
	SET_V8(d8_1, d8_2, tS16); \
	\
	(u1_t) (tS16 & 0xff); \
})

#define daa(reg) \
({ \
	u1_t msn, lsn; \
	u2_t cf = 0; \
	\
	msn = reg & 0xf0; lsn = reg & 0x0f; \
	if (lsn > 0x09 || getH()) cf |= 0x06; \
	if (msn > 0x80 && lsn > 0x09 ) cf |= 0x60; \
	if (msn > 0x90 || getC()) cf |= 0x60; \
	tU16 = cf + reg; \
	\
	modcCVNZ(); \
	modsVNZ(tU16 & 0xff); \
	SET_C8(tU16); \
	SET_V8(reg, cf, tU16); \
	\
	(u1_t) tU16; \
})

#define cmp(p8_1, p8_2) \
({ \
	u1_t d8_1 = p8_1, d8_2 = p8_2; \
	u2_t tS16; \
	\
	tS16 = (u2_t) d8_1 - (u2_t) d8_2; \
	\
	modcCVNZ(); \
	modsVNZ(tS16 & 0xff); \
	SET_C8(tS16); \
	SET_V8(d8_1, d8_2, tS16); \
})

// FIXME could optimize cc setting with assignments
#define cmp16(p16_1, p16_2) \
({ \
	u2_t d16_1 = p16_1, d16_2 = p16_2; \
	u4_t lTemp; \
	\
	lTemp = (u4_t) d16_1 - (u4_t) d16_2; \
	\
	clrCVNZ(); \
	if (lTemp == 0) { \
		setZ(); \
	} \
	if (lTemp & 0x8000) { \
		setN(); \
	} \
	if (lTemp & 0x10000) { \
		setC(); \
	} \
	SET_V16(d16_1, d16_2, lTemp); \
})

#define lsr(p8) \
({ \
	u1_t d8 = p8; \
	\
	clrNlsrCVZ(); \
	/* if d0 = 1 then carry */ \
	/* but since d7 (N) always = 0 due to shift-in then d0 = 1 means overflow too since V = N^C = 0^1 = 1 */ \
	/* FIXME check that this is really cheaper to do! */ \
	SET_VCcond(d8 & 0x01); \
	\
	d8 >>= 1; \
	\
	SET_Zcond(d8 == 0); \
	\
	d8; \
})

#define asr(p8) \
({ \
	u1_t d8 = p8; \
	\
	modcCVNZ(); \
	SET_Ccond(d8 & 0x01); \
	\
	d8 = (d8 & 0x80) | (d8 >> 1); \
	\
	modsVNZ(d8); \
	SET_Vshift(); \
	\
	d8; \
})

#define asl(p8) \
({ \
	u1_t d8 = p8; \
	\
	modcCVNZ(); \
	SET_Ccond(d8 & 0x80); \
	\
	d8 <<= 1; \
	\
	modsVNZ(d8); \
	SET_Vshift(); \
	\
	d8; \
})

// uses carry-in
#define ror(p8) \
({ \
	u1_t d8 = p8; \
	\
	u1_t uc; \
	uc = getC(); /* preserve old carry flag */ \
	\
	modcCVNZ(); \
	SET_Ccond(d8 & 0x01); \
	\
	d8 = d8 >> 1 | uc << 7; \
	\
	modsVNZ(d8); \
	SET_Vshift(); \
	\
	d8; \
})

// uses carry-in
#define rol(p8) \
({ \
	u1_t d8 = p8; \
	u1_t uc; \
	\
	uc = getC(); /* preserve old carry flag */ \
	\
	modcCVNZ(); \
	SET_Ccond(d8 & 0x80); \
	\
	d8 = d8 << 1 | uc; \
	\
	modsVNZ(d8); \
	SET_Vshift(); \
	\
	d8; \
})

#define eor(p8_1, p8_2) \
({ \
	u1_t d8_1 = p8_1, d8_2 = p8_2; \
	u1_t tU8; \
	\
	clrV_modcNZ(); \
	\
	tU8 = d8_1 ^ d8_2; \
	\
	modsVNZ(tU8); \
	\
	tU8; \
})

#define or(p8_1, p8_2) \
({ \
	u1_t d8_1 = p8_1, d8_2 = p8_2; \
	u1_t tU8; \
	\
	clrV_modcNZ(); \
	\
	tU8 = d8_1 | d8_2; \
	\
	modsVNZ(tU8); \
	\
	tU8; \
})

#define and(p8_1, p8_2) \
({ \
	u1_t d8_1 = p8_1, d8_2 = p8_2; \
	u1_t tU8; \
	\
	clrV_modcNZ(); \
	\
	tU8 = d8_1 & d8_2; \
	\
	modsVNZ(tU8); \
	\
	tU8; \
})

#define com(p8) \
({ \
	u1_t d8 = p8; \
	\
	clrV_modcNZ(); \
	setC(); \
	\
	d8 = ~d8; \
	\
	modsVNZ(d8); \
	\
	d8; \
})

#define neg(p8) \
({ \
	u1_t d8 = p8; \
	\
	clrCVNZ(); \
	\
	d8 = ~d8 + 1; \
	\
	if (d8 == 0x80) { /* FIXME */ \
		setV(); \
	} \
	if (d8 == 0) { \
		setZC(); \
	} \
	if (d8 & 0x80) { \
		setN(); \
	} \
	\
	d8; \
})

#define tst(p8) \
({ \
	u1_t d8 = p8; \
	\
	clrCV_modcNZ(); \
	modsVNZ(d8); \
})

#define clr() \
({ \
	clrCVNZ(); \
	setZ(); \
	\
	0; \
})

#define xfer(dst, src) \
({ \
	dst = src; \
	clrV_modcNZ(); \
	modsVNZ(dst); \
})

#define ld(reg) \
({ \
	reg = tU8; \
	\
	clrV_modcNZ(); \
	modsVNZ(reg); \
})

#define ld16(reg) \
({ \
	reg = tU16; \
	\
	setNZ16(reg); \
	clrV(); \
})

#define st(reg) \
({ \
	tU8 = reg; \
	\
	clrV_modcNZ(); \
	modsVNZ(tU8); \
})

#define st16(reg) \
({ \
	tU16 = reg; \
	\
	setNZ16(tU16); \
	clrV(); \
})

#define pushB(p8) \
({ \
	u1_t d8 = p8; \
	u2_t tAddr = --rSP; \
	\
	pushByte(tAddr, d8);	\
})

#define pushW(p16) \
({ \
	u2_t d16 = p16; \
	u2_t tAddr; \
	u1_t hi, lo; \
	\
	hi = d16 >> 8; \
	lo = d16 & 0xff; \
	tAddr = --rSP; \
	pushByte(tAddr, lo); \
	tAddr = --rSP; \
	pushByte(tAddr, hi); \
})

#define pullB() \
({ \
	u1_t d8; \
	\
	d8 = pullByte(rSP); \
	rSP++; \
	d8; \
})

#define pullW() \
({ \
	u2_t tAddr; \
	u1_t hi, lo; \
	\
	tAddr = rSP++; \
	hi = pullByte(tAddr); \
	tAddr = rSP++; \
	lo = pullByte(tAddr); \
	((hi << 8) | lo); \
})

#define _3branch(dummy_dest_addr) \
({ \
	rPC += tS16; \
	BRANCH_TAKEN(); \
})

#define computed_goto(addr) \
({ \
	rPC = addr; \
	BRANCH_ALWAYS(); \
})
#define _5goto(dummy_dest_addr, addr) \
({ \
	rPC = addr; \
	BRANCH_ALWAYS(); \
})
