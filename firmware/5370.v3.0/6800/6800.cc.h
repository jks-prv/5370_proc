
// this particular way of handling condition codes was measured to be the fastest amongst several alternatives tested

#define F_CARRY     0x01
#define F_OVERFLOW  0x02
#define F_ZERO      0x04
#define F_NEGATIVE  0x08
#define F_IRQMASK   0x10
#define F_HALFCARRY 0x20
#define F_ONES		0xc0

#define	VtoN(v)			((v) << 2)	// i.e. shift F_OVERFLOW bit to F_NEGATIVE position
#define	CtoN(v)			((v) << 3)	// i.e. shift F_CARRY bit to F_NEGATIVE position
#define bit7toV(b)		(((b) & 0x80) >> 6)
#define bit15toV(b)		(((b) & 0x8000) >> 14)

#ifdef _6800_DOT_C_
	// surprisingly, negative and zero flags can be set more quickly by doing a lookup with an array
	
	#define Z	F_ZERO
	#define N	F_NEGATIVE
	
	const u1_t SET_NZ[256] = {
		  Z,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,          /* 00-0F */
		  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,          /* 10-1F */
		  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,          /* 20-2F */
		  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,          /* 30-3F */
		  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,          /* 40-4F */
		  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,          /* 50-5F */
		  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,          /* 60-6F */
		  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,          /* 70-7F */
		  N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,          /* 80-8F */
		  N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,          /* 90-9F */
		  N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,          /* A0-AF */
		  N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,          /* B0-BF */
		  N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,          /* C0-CF */
		  N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,          /* D0-DF */
		  N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,          /* E0-EF */
		  N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N           /* F0-FF */
	};
	
	#undef Z
	#undef N
#else
	extern u1_t SET_NZ[];
#endif

// this is the heart of the speed optimization:
// best to treat VNZ in a single variable, and IRQ, H and C separately
#define declareCC()	u1_t IRQ, H, C, VNZ;

#define	clrZ()		VNZ &= ~F_ZERO;
#define	setZ()		VNZ |= F_ZERO;
#define	getZ()		(VNZ & F_ZERO)

#define	clrZN()		VNZ &= ~(F_ZERO | F_NEGATIVE);
#define	setZC()		VNZ |= F_ZERO; C = F_CARRY;

#define	setN()		VNZ |= F_NEGATIVE;
#define	getN()		(VNZ & F_NEGATIVE)

#define	clrV()		VNZ &= ~F_OVERFLOW;
#define	setV()		VNZ |= F_OVERFLOW;
#define	getV()		(VNZ & F_OVERFLOW)

#define	clrC()		C = 0;
#define	setC()		C = F_CARRY;
#define	getC()		(C)

#define	clrCVNZ()	C = 0; VNZ = 0;

#define	setH()		H = F_HALFCARRY;
#define	getH()		(H)

#define	clrCH()		C = 0; H = 0;

#define	clrI()		IRQ = 0;
#define	setI()		IRQ = F_IRQMASK;
#define	getI()		(IRQ)

#define bhi()		(!(C || (VNZ & F_ZERO)))
#define bls()		(C || (VNZ & F_ZERO))
#define bcc()		(!C)
#define bcs()		(C)
#define bne()		(!(VNZ & F_ZERO))
#define beq()		(VNZ & F_ZERO)
#define bvc()		(!(VNZ & F_OVERFLOW))
#define bvs()		(VNZ & F_OVERFLOW)
#define bpl()		(!(VNZ & F_NEGATIVE))
#define bmi()		(VNZ & F_NEGATIVE)
#define bge()		(!((VNZ & F_NEGATIVE) ^ VtoN(VNZ & F_OVERFLOW)))
#define blt()		((VNZ & F_NEGATIVE) ^ VtoN(VNZ & F_OVERFLOW))
#define bgt()		(!(((VNZ & F_NEGATIVE) ^ VtoN(VNZ & F_OVERFLOW)) || (VNZ & F_ZERO)))
#define ble()		(((VNZ & F_NEGATIVE) ^ VtoN(VNZ & F_OVERFLOW)) || (VNZ & F_ZERO))

#define SET_C8(r)			if ((r) & 0x100) { C = F_CARRY; }
#define SET_Vcond(cond)		if (cond) { VNZ |= F_OVERFLOW; }
#define SET_VCcond(cond)	if (cond) { VNZ |= F_OVERFLOW; C = F_CARRY; }
#define SET_Vshift()		if (getN() ^ CtoN(getC())) { VNZ |= F_OVERFLOW; }
#define SET_Ccond(cond)		if (cond) { C = F_CARRY; }
#define SET_Zcond(cond)		if (cond) { VNZ |= F_ZERO; }
#define SET_V8(a, b, r)		VNZ |= bit7toV(a ^ b ^ r ^ (r >> 1))
#define SET_V16(a, b, r)	VNZ |= bit15toV(a ^ b ^ r ^ (r >> 1))
#define SET_H8(a, b, r)		if ((a ^ b ^ r) & 0x10) { H |= F_HALFCARRY; }

#define	setCC(cc8) \
({ \
	u1_t cc = cc8; \
	\
	IRQ = cc & F_IRQMASK; \
	H = cc & F_HALFCARRY; \
	C = cc & F_CARRY; \
	VNZ = cc & (F_OVERFLOW | F_NEGATIVE | F_ZERO); \
})

#define	getCC() \
({ \
	F_ONES | IRQ | H | C | VNZ; \
})

#define setNZ16(p16) \
({ \
	u2_t d16 = p16; \
	\
	clrZN(); \
	if (d16 == 0) \
		setZ(); \
	if (d16 & 0x8000) \
		setN(); \
})

// used by inx/dex only
#define setZX(reg) \
({ \
	clrZ(); \
	if ((reg) == 0) \
	   setZ(); \
})

#define clrVsetNZ(reg) \
({ \
	VNZ = SET_NZ[reg];	/* V set zero as side-effect */ \
})
