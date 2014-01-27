// Define the instruction set by combining core instruction, addressing mode and condition code processing.
//
// some of the syntactic complexity remaining here is leftover from when this code ran on a far less-capable
// microcontroller (Atmel sam7x) and used some fancy dynamic inline code generation to speed things up

	INSN(0x01, nop, ; );

	INSN(0x06, tap, NA( setCC(rA); ) );
	INSN(0x07, tpa, NA( rA = getCC(); ) );

	INSN(0x08, inx, NA( rX++; setZX(rX); ) );
	INSN(0x09, dex, NA( rX--; setZX(rX); ) );

	INSN(0x0a, clv, NA( clrV(); ) );
	INSN(0x0b, sev, NA( setV(); ) );
	INSN(0x0c, clc, NA( clrC(); ) );
	INSN(0x0d, sec, NA( setC(); ) );
	INSN(0x0e, cli, NA( clrI(); ) );
	INSN(0x0f, sei, NA( setI(); ) );

	INSN(0x16, tab, NA( xfer(rB, rA); ) );
	INSN(0x17, tba, NA( xfer(rA, rB); ) );

	INSN(0x19, daa, NA( rA = daa(rA); ) );

	INSN(0x20, bra, _08_REL(); NA( if (1) { branch(); } ) );
	INSN(0x21, brn, _08_REL(); );
	INSN(0x22, bhi, _08_REL(); NA( if (bhi()) { branch(); } ) );
	INSN(0x23, bls, _08_REL(); NA( if (bls()) { branch(); } ) );
	INSN(0x24, bcc, _08_REL(); NA( if (bcc()) { branch(); } ) );
	INSN(0x25, bcs, _08_REL(); NA( if (bcs()) { branch(); } ) );
	INSN(0x26, bne, _08_REL(); NA( if (bne()) { branch(); } ) );
	INSN(0x27, beq, _08_REL(); NA( if (beq()) { branch(); } ) );
	INSN(0x28, bvc, _08_REL(); NA( if (bvc()) { branch(); } ) );
	INSN(0x29, bvs, _08_REL(); NA( if (bvs()) { branch(); } ) );
	INSN(0x2a, bpl, _08_REL(); NA( if (bpl()) { branch(); } ) );
	INSN(0x2b, bmi, _08_REL(); NA( if (bmi()) { branch(); } ) );
	INSN(0x2c, bge, _08_REL(); NA( if (bge()) { branch(); } ) );
	INSN(0x2d, blt, _08_REL(); NA( if (blt()) { branch(); } ) );
	INSN(0x2e, bgt, _08_REL(); NA( if (bgt()) { branch(); } ) );
	INSN(0x2f, ble, _08_REL(); NA( if (ble()) { branch(); } ) );

	INSN(0x30, tsx, NA( rX = rSP + 1; ) );
	INSN(0x31, ins, NA( rSP++; ) );
	INSN(0x32, pula, NA( rA = pullB(); ) );
	INSN(0x33, pulb, NA( rB = pullB(); ) );
	INSN(0x34, des, NA( rSP--; ) );
	INSN(0x35, txs, NA( rSP = rX - 1; ) );
	INSN(0x36, psha, NA( pushB(rA); ) );
	INSN(0x37, pshb, NA( pushB(rB); ) );
	INSN(0x39, rts, CALL( rPC = pullW(); BRANCH_ALWAYS(); ) );
	INSN(0x3b, rti, RTN( setCC(pullB()); rB = pullB(); rA = pullB(); rX = pullW(); rPC = pullW(); BRANCH_ALWAYS(); ) );
	INSN(0x3e, wai, NA( if (IRQs) goto checkpending; else rPC--; BRANCH_ALWAYS(); ) );
	INSN(0x3f, swi, RTN( pushW(rPC); pushW(rX); pushB(rA); pushB(rB); pushB(getCC()); setI(); rPC = readWord(VEC_SWI); BRANCH_ALWAYS(); ) );

	INSN(0x6e, jmp, _08_IDX(); NA( computed_goto(tAddr); ) );
	INSN(0x7e, jmp, _16_EXT(); NA( computed_goto(tAddr); ) );

	INSN(0xad, jsr, _08_IDX(); CALL( pushW(rPC); computed_goto(tAddr); ) );
	INSN(0xbd, jsr, _16_EXT(); CALL( pushW(rPC); computed_goto(tAddr); ) );

	INSN(0x8d, bsr, _08_IMM(); CALL( tS16 = (s2_t) (s1_t) tU8; pushW(rPC); rPC += tS16; BRANCH_ALWAYS(); ) );

	INSN(0x1b, add, NA( rA = add(rA, rB); ) );
	INSN(0x8b, add, _08_IMM(); IMM(rA = add(rA, tU8)); );
	INSN(0x9b, add, _08_DIR(); R8(rA = add(rA, tU8)); );
	INSN(0xab, add, _08_IDX(); R8(rA = add(rA, tU8)); );
	INSN(0xbb, add, _16_EXT(); R8(rA = add(rA, tU8)); );
	INSN(0xcb, add, _08_IMM(); IMM(rB = add(rB, tU8)); );
	INSN(0xdb, add, _08_DIR(); R8(rB = add(rB, tU8)); );
	INSN(0xeb, add, _08_IDX(); R8(rB = add(rB, tU8)); );
	INSN(0xfb, add, _16_EXT(); R8(rB = add(rB, tU8)); );

	INSN(0x89, adc, _08_IMM(); IMM(rA = adc(rA, tU8)); );
	INSN(0x99, adc, _08_DIR(); R8(rA = adc(rA, tU8)); );
	INSN(0xa9, adc, _08_IDX(); R8(rA = adc(rA, tU8)); );
	INSN(0xb9, adc, _16_EXT(); R8(rA = adc(rA, tU8)); );
	INSN(0xc9, adc, _08_IMM(); IMM(rB = adc(rB, tU8)); );
	INSN(0xd9, adc, _08_DIR(); R8(rB = adc(rB, tU8)); );
	INSN(0xe9, adc, _08_IDX(); R8(rB = adc(rB, tU8)); );
	INSN(0xf9, adc, _16_EXT(); R8(rB = adc(rB, tU8)); );

	INSN(0x10, sub, NA( rA = sub(rA, rB); ) );
	INSN(0x80, sub, _08_IMM(); IMM(rA = sub(rA, tU8)); );
	INSN(0x90, sub, _08_DIR(); R8(rA = sub(rA, tU8)); );
	INSN(0xa0, sub, _08_IDX(); R8(rA = sub(rA, tU8)); );
	INSN(0xb0, sub, _16_EXT(); R8(rA = sub(rA, tU8)); );
	INSN(0xc0, sub, _08_IMM(); IMM(rB = sub(rB, tU8)); );
	INSN(0xd0, sub, _08_DIR(); R8(rB = sub(rB, tU8)); );
	INSN(0xe0, sub, _08_IDX(); R8(rB = sub(rB, tU8)); );
	INSN(0xf0, sub, _16_EXT(); R8(rB = sub(rB, tU8)); );

	INSN(0x82, sbc, _08_IMM(); IMM(rA = sbc(rA, tU8)); );
	INSN(0x92, sbc, _08_DIR(); R8(rA = sbc(rA, tU8)); );
	INSN(0xa2, sbc, _08_IDX(); R8(rA = sbc(rA, tU8)); );
	INSN(0xb2, sbc, _16_EXT(); R8(rA = sbc(rA, tU8)); );
	INSN(0xc2, sbc, _08_IMM(); IMM(rB = sbc(rB, tU8)); );
	INSN(0xd2, sbc, _08_DIR(); R8(rB = sbc(rB, tU8)); );
	INSN(0xe2, sbc, _08_IDX(); R8(rB = sbc(rB, tU8)); );
	INSN(0xf2, sbc, _16_EXT(); R8(rB = sbc(rB, tU8)); );

	INSN(0x84, and, _08_IMM(); IMM(rA = and(rA, tU8)); );
	INSN(0x94, and, _08_DIR(); R8(rA = and(rA, tU8)); );
	INSN(0xa4, and, _08_IDX(); R8(rA = and(rA, tU8)); );
	INSN(0xb4, and, _16_EXT(); R8(rA = and(rA, tU8)); );
	INSN(0xc4, and, _08_IMM(); IMM(rB = and(rB, tU8)); );
	INSN(0xd4, and, _08_DIR(); R8(rB = and(rB, tU8)); );
	INSN(0xe4, and, _08_IDX(); R8(rB = and(rB, tU8)); );
	INSN(0xf4, and, _16_EXT(); R8(rB = and(rB, tU8)); );

	INSN(0x8a, or, _08_IMM(); IMM(rA = or(rA, tU8)); );
	INSN(0x9a, or, _08_DIR(); R8(rA = or(rA, tU8)); );
	INSN(0xaa, or, _08_IDX(); R8(rA = or(rA, tU8)); );
	INSN(0xba, or, _16_EXT(); R8(rA = or(rA, tU8)); );
	INSN(0xca, or, _08_IMM(); IMM(rB = or(rB, tU8)); );
	INSN(0xda, or, _08_DIR(); R8(rB = or(rB, tU8)); );
	INSN(0xea, or, _08_IDX(); R8(rB = or(rB, tU8)); );
	INSN(0xfa, or, _16_EXT(); R8(rB = or(rB, tU8)); );

	INSN(0x88, eor, _08_IMM(); IMM(rA = eor(rA, tU8)); );
	INSN(0x98, eor, _08_DIR(); R8(rA = eor(rA, tU8)); );
	INSN(0xa8, eor, _08_IDX(); R8(rA = eor(rA, tU8)); );
	INSN(0xb8, eor, _16_EXT(); R8(rA = eor(rA, tU8)); );
	INSN(0xc8, eor, _08_IMM(); IMM(rB = eor(rB, tU8)); );
	INSN(0xd8, eor, _08_DIR(); R8(rB = eor(rB, tU8)); );
	INSN(0xe8, eor, _08_IDX(); R8(rB = eor(rB, tU8)); );
	INSN(0xf8, eor, _16_EXT(); R8(rB = eor(rB, tU8)); );

	INSN(0x85, bit, _08_IMM(); IMM(and(rA, tU8)); );
	INSN(0x95, bit, _08_DIR(); R8(and(rA, tU8)); );
	INSN(0xa5, bit, _08_IDX(); R8(and(rA, tU8)); );
	INSN(0xb5, bit, _16_EXT(); R8(and(rA, tU8)); );
	INSN(0xc5, bit, _08_IMM(); IMM(and(rB, tU8)); );
	INSN(0xd5, bit, _08_DIR(); R8(and(rB, tU8)); );
	INSN(0xe5, bit, _08_IDX(); R8(and(rB, tU8)); );
	INSN(0xf5, bit, _16_EXT(); R8(and(rB, tU8)); );

	INSN(0x11, cmp, NA( cmp(rA, rB); ) );
	INSN(0x81, cmp, _08_IMM(); IMM(cmp(rA, tU8)); );
	INSN(0x91, cmp, _08_DIR(); R8(cmp(rA, tU8)); );
	INSN(0xa1, cmp, _08_IDX(); R8(cmp(rA, tU8)); );
	INSN(0xb1, cmp, _16_EXT(); R8(cmp(rA, tU8)); );
	INSN(0xc1, cmp, _08_IMM(); IMM(cmp(rB, tU8)); );
	INSN(0xd1, cmp, _08_DIR(); R8(cmp(rB, tU8)); );
	INSN(0xe1, cmp, _08_IDX(); R8(cmp(rB, tU8)); );
	INSN(0xf1, cmp, _16_EXT(); R8(cmp(rB, tU8)); );

	INSN(0x8c, cmpx, _16_WIM(); IMM(cmp16(rX, tU16)); );
	INSN(0x9c, cmpx, _08_DIR(); R16(cmp16(rX, tU16)); );
	INSN(0xac, cmpx, _08_IDX(); R16(cmp16(rX, tU16)); );
	INSN(0xbc, cmpx, _16_EXT(); R16(cmp16(rX, tU16)); );

	INSN(0x86, lda, _08_IMM(); IMM(ld(rA)); );
	INSN(0x96, lda, _08_DIR(); R8(ld(rA)); );
	INSN(0xa6, lda, _08_IDX(); R8(ld(rA)); );
	INSN(0xb6, lda, _16_EXT(); R8(ld(rA)); );

	INSN(0xc6, ldb, _08_IMM(); IMM(ld(rB)); );
	INSN(0xd6, ldb, _08_DIR(); R8(ld(rB)); );
	INSN(0xe6, ldb, _08_IDX(); R8(ld(rB)); );
	INSN(0xf6, ldb, _16_EXT(); R8(ld(rB)); );

	INSN(0x97, sta, _08_DIR(); W8(st(rA)); );
	INSN(0xa7, sta, _08_IDX(); W8(st(rA)); );
	INSN(0xb7, sta, _16_EXT(); W8(st(rA)); );

	INSN(0xd7, stb, _08_DIR(); W8(st(rB)); );
	INSN(0xe7, stb, _08_IDX(); W8(st(rB)); );
	INSN(0xf7, stb, _16_EXT(); W8(st(rB)); );

	INSN(0x8e, lds, _16_WIM(); IMM(ld16(rSP)); );
	INSN(0x9e, lds, _08_DIR(); R16(ld16(rSP)); );
	INSN(0xae, lds, _08_IDX(); R16(ld16(rSP)); );
	INSN(0xbe, lds, _16_EXT(); R16(ld16(rSP)); );

	INSN(0xce, ldx, _16_WIM(); IMM(ld16(rX)); );
	INSN(0xde, ldx, _08_DIR(); R16(ld16(rX)); );
	INSN(0xee, ldx, _08_IDX(); R16(ld16(rX)); );
	INSN(0xfe, ldx, _16_EXT(); R16(ld16(rX)); );

	INSN(0x9f, sts, _08_DIR(); W16(st16(rSP)); );
	INSN(0xaf, sts, _08_IDX(); W16(st16(rSP)); );
	INSN(0xbf, sts, _16_EXT(); W16(st16(rSP)); );

	INSN(0xdf, stx, _08_DIR(); W16(st16(rX)); );
	INSN(0xef, stx, _08_IDX(); W16(st16(rX)); );
	INSN(0xff, stx, _16_EXT(); W16(st16(rX)); );

	INSN(0x4c, inc, NA( rA = inc(rA); ) );
	INSN(0x5c, inc, NA( rB = inc(rB); ) );
	INSN(0x6c, inc, _08_IDX(); RMW_8(tU8 = inc(tU8)); );
	INSN(0x7c, inc, _16_EXT(); RMW_8(tU8 = inc(tU8)); );

	INSN(0x4a, dec, NA( rA = dec(rA); ) );
	INSN(0x5a, dec, NA( rB = dec(rB); ) );
	INSN(0x6a, dec, _08_IDX(); RMW_8(tU8 = dec(tU8)); );
	INSN(0x7a, dec, _16_EXT(); RMW_8(tU8 = dec(tU8)); );

	INSN(0x40, neg, NA( rA = neg(rA); ) );
	INSN(0x50, neg, NA( rB = neg(rB); ) );
	INSN(0x60, neg, _08_IDX(); RMW_8(tU8 = neg(tU8)); );
	INSN(0x70, neg, _16_EXT(); RMW_8(tU8 = neg(tU8)); );

	INSN(0x43, com, NA( rA = com(rA); ) );
	INSN(0x53, com, NA( rB = com(rB); ) );
	INSN(0x63, com, _08_IDX(); RMW_8(tU8 = com(tU8)); );
	INSN(0x73, com, _16_EXT(); RMW_8(tU8 = com(tU8)); );

	INSN(0x44, lsr, NA( rA = lsr(rA); ) );
	INSN(0x54, lsr, NA( rB = lsr(rB); ) );
	INSN(0x64, lsr, _08_IDX(); RMW_8(tU8 = lsr(tU8)); );
	INSN(0x74, lsr, _16_EXT(); RMW_8(tU8 = lsr(tU8)); );

	INSN(0x46, ror, NA( rA = ror(rA); ) );
	INSN(0x56, ror, NA( rB = ror(rB); ) );
	INSN(0x66, ror, _08_IDX(); RMW_8(tU8 = ror(tU8)); );
	INSN(0x76, ror, _16_EXT(); RMW_8(tU8 = ror(tU8)); );

	INSN(0x49, rol, NA( rA = rol(rA); ) );
	INSN(0x59, rol, NA( rB = rol(rB); ) );
	INSN(0x69, rol, _08_IDX(); RMW_8(tU8 = rol(tU8)); );
	INSN(0x79, rol, _16_EXT(); RMW_8(tU8 = rol(tU8)); );

	INSN(0x47, asr, NA( rA = asr(rA); ) );
	INSN(0x57, asr, NA( rB = asr(rB); ) );
	INSN(0x67, asr, _08_IDX(); RMW_8(tU8 = asr(tU8)); );
	INSN(0x77, asr, _16_EXT(); RMW_8(tU8 = asr(tU8)); );

	INSN(0x48, asl, NA( rA = asl(rA); ) );
	INSN(0x58, asl, NA( rB = asl(rB); ) );
	INSN(0x68, asl, _08_IDX(); RMW_8(tU8 = asl(tU8)); );
	INSN(0x78, asl, _16_EXT(); RMW_8(tU8 = asl(tU8)); );

	INSN(0x4d, tst, NA( tst(rA); ) );
	INSN(0x5d, tst, NA( tst(rB); ) );
	INSN(0x6d, tst, _08_IDX(); RMW_8(tst(tU8)); );
	INSN(0x7d, tst, _16_EXT(); RMW_8(tst(tU8)); );

	INSN(0x4f, clr, NA( rA = clr(); ) );
	INSN(0x5f, clr, NA( rB = clr(); ) );
	INSN(0x6f, clr, _08_IDX(); W8(tU8 = clr()); );
	INSN(0x7f, clr, _16_EXT(); W8(tU8 = clr()); );
