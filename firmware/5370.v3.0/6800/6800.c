// 6800 microprocessor interpreter
//
// some of the syntactic complexity remaining here is leftover from when this code ran on a far less-capable
// microcontroller (Atmel sam7x) and used some fancy dynamic inline code generation to speed things up

#include <sys/types.h>

#include "boot.h"
#include "misc.h"
#include "net.h"
#include "sim.h"
#include "5370.h"
#include "bus.h"
#include "hpib.h"
#include "front_panel.h"
#include "chip.h"

#include "6800.h"

#include <stdio.h>
#include <errno.h>
#include <ctype.h>
#include <string.h>

// these are set in the makefile:
// DEBUG
// INSN_TRACE

#define INT_NMI  1
#define INT_FIRQ 2
#define INT_IRQ  4

// redefined from 6800 standard 0xfffx to speed memory access routines via easier address decoding
#define	VEC_RESET	0x7ffe
#define	VEC_NMI		0x7ffc
#define	VEC_IRQ		0x7ff8
#define	VEC_SWI		0x7ffa

u1_t rom_image[ROM_SIZE] = {
	#include ROM_CODE_H
};

u1_t ram_image[RAM_SIZE];

// used to measure histogram of instruction counts between i/o cycles
#ifdef RECORD_IO_HIST

	#define IO_HIST 1024
	static u4_t io_hist[IO_HIST], io_hist_last;
	
	void io_hist_upd(iCount)
	{
		u4_t t = iCount - io_hist_last;
		if (t > IO_HIST) t = 0;
		io_hist[t]++;
		io_hist_last = iCount;
	}
	
	#undef readByte
	
	#define readByte(addr) \
		(((addr & ROM_MASK) != 0)? (rom[addr]) : \
		(((addr & RAM_MASK) != 0)? (ram[addr]) : \
		(io_hist_upd(), readDev(addr))))
	
	#undef readByteNoROM
	#define readByteNoROM(addr) readByte(addr)
	
	#undef writeByte
	
	#define writeByte(addr, data) \
		if ((addr & RAM_MASK) != 0) { \
			ram[addr] = data; \
		} else { \
			io_hist_upd(iCount); \
			writeDev(addr, data); \
		}

#endif


//#define ROM_RAM_COVER
#ifdef ROM_RAM_COVER
	u1_t rom_coverage[ROM_SIZE];
	u1_t ram_coverage[RAM_SIZE];
	
	int cover_sampling = 0;
	
	#undef readByte
	
	u1_t readByte(u2_t addr)
	{
		if ((addr & ROM_MASK) != 0) {
			if (cover_sampling) rom_coverage[addr-ROM_START] = 0xee;
			return rom_image[addr-ROM_START];
		} else
		if ((addr & RAM_MASK) != 0) {
			if (cover_sampling) ram_coverage[addr-RAM_START] = 0xee;
			return ram_image[addr-RAM_START];
		}
	
		return readDev(addr);
	}
	
	#undef readByteNoROM
	#define readByteNoROM(addr) readByte(addr)

#endif


#define _6800_DOT_C_
 #include "6800.cc.h"
#undef _6800_DOT_C_


// core instruction code, without all the address mode variants
#include "6800.core.h"


#if defined(DEBUG) || defined(INSN_TRACE)

int iTrace = 0;
int itr = 0;
int iSnap = 0;

#endif

static int IRQs = 0;

#ifdef DEBUG
	bool iDump;
	int irq_trace = 0;
	int ispeed = 0;
	
	u4_t op_coverage[N_OP];
	
	struct {
		u4_t count;
		u1_t idx;
	} sorted[N_OP];
#endif

#if defined(RECORD_IO_HIST) || defined(DEBUG) || defined(HPIB_RECORD) || defined(HPIB_SIM_DEBUG) || defined(MEAS_IPS) || defined(MEAS_IP_HPIB_MEAS)
	#define ICOUNT
	#define INT_ICOUNT
	#define iCount (ciCount + iiCount)
	#define INLINE_ICOUNT()	ciCount++
	#define INTERP_ICOUNT() iiCount++
#else
	#define iCount 0
	#define INLINE_ICOUNT()
	#define INTERP_ICOUNT()
#endif

#if defined(ICOUNT) && (defined(MEAS_IP_HPIB_MEAS) || defined(HPIB_SIM_DEBUG))
	#undef INT_ICOUNT
	#define EXT_ICOUNT
#endif

#ifdef EXT_ICOUNT
	static u4_t ciCount, iiCount;
	
	u4_t sim_insn_count()
	{
		return ciCount + iiCount;
	}
#endif

#define PEND_PRINT	8

void sim_processor()
{
	u1_t rA, rB;
	u2_t rX, rSP, rPC;

	declareCC();

	u2_t tAddr = 0;
	u1_t tU8 = 0;
	u2_t tU16 = 0;

	// biased with e.g. ROM_START so that rom[addr] can be indexed with actual runtime address
	u1_t *rom = &rom_image[0] - ROM_START;
	u1_t *ram = &ram_image[0] - RAM_START;
	
	s2_t tS16 = 0;

	u1_t opcode;

#ifdef INT_ICOUNT
	u4_t ciCount, iiCount;
#endif

#if defined(DEBUG) || defined(DBG_INTRS)
	u4_t iPendCt = 0;
	u4_t iPendPrint = PEND_PRINT;
#endif

#ifdef DEBUG
	u1_t inh;
	u4_t inIRQ = 0;
#endif

#ifdef DBG_INTRS
	u4_t i_level = 0, i_latched = 0;
	u4_t dintr = 1;
#endif

#ifdef FASTER_INTERRUPTS
	extern volatile bool got_irq;
#endif

D_STMT(reset:)
	D_STMT(inh = 0);
	
#ifdef INT_ICOUNT
	ciCount = iiCount = 0;
#endif

#ifdef ROM_RAM_COVER
	{
		int i;
		
		for (i = 0; i < ROM_SIZE; i++) {
			rom_coverage[i] = 0;
		}
		for (i = 0; i < RAM_SIZE; i++) {
			ram_coverage[i] = 0;
		}
	}
#endif
	
	rA = rB = 0;
	rX = rSP = 0;
	
#if defined(HPIB_RECORD) || defined(HPIB_SIM_DEBUG)
	rPC = IRQ = 0;		// avoid compiler uninitialized warning
#endif

	rPC = readWord(VEC_RESET);
	D_PRF(("DEBUG MODE ON -- START @ %04x\n", rPC));

	setCC(0);
	setI();	// no IRQ in initially

next_insn:
branch_taken:

	if ((iCount & SIM_POLL_NET_COUNT) == 0) net_poll();

	char *cp;
	if (((iCount & SIM_POLL_INPUT_COUNT) == 0) && ((cp = sim_input()) != 0)) {

#include "6800.debug.c"

	}

//#define SHOW_INTERP
#ifdef SHOW_INTERP
	// toggle the "test pin" to show when the interp is running
	// helps us see on the logic analyzer when we're in the network stack
	if (iCount & 1) {
		TEST2_SET();
	} else {
		TEST2_CLR();
	}
#endif

#ifdef DEBUG
	if ((rPC < ROM_START) || (rPC >= (ROM_START+ROM_SIZE))) {
   		printf("\nrPC range! 0x%x 0x%x-0x%x\n", rPC, ROM_START, ROM_START+ROM_SIZE);
   		panic("");
	}
#endif

// remember: interrupts are level (not edge) sensitive on the 6800
// one consequence is that the interrupt is never taken if the level disappears before interrupt disable is unmasked
checkpending:

#if	defined(DEBUG) || defined(INSN_TRACE)
	itr = 0;
#endif
	
#ifdef FASTER_INTERRUPTS
	if (got_irq || hpib_causeIRQ) {
		got_irq = FALSE;
#endif

	if (CHECK_NMI()) {	// FIXME interrupt routine could set IRQs directly with proper locking
		#ifdef DBG_INTRS
			PFC(dintr, "*NMI*\n");
		#endif
		IRQs |= INT_NMI;
	} else {
		IRQs &= ~INT_NMI;
	}

#if 0
	i_level = 0;
	char *cp;
	if ((cp = sim_input())) {
		if (*cp == 'l' && cp[1] == '1') i_level = 1;
		if (*cp == 'l' && cp[1] == '2') i_latched = 1;
	}
	if (CHECK_IRQ() || sim_key || i_level || i_latched) {
#else
	if (CHECK_IRQ() || sim_key) {
#endif
		#ifdef DBG_INTRS
			if (iPendCt == 0) PFC(dintr, "*IRQ* msk? %s\n" , getI()? "YES":"NO");
		#endif
		IRQs |= INT_IRQ;
	} else {
		IRQs &= ~INT_IRQ;
	}

#ifdef HPIB_SIM
	if (hpib_causeIRQ) {
		#ifdef DBG_INTRS
			PFC(dintr, "*HIRQ*\n");
		#endif
		#ifdef HPIB_SIM_DEBUG
			PFC(hps, "----HPIB IRQ\n");
		#endif
		IRQs |= INT_IRQ;
	}
#endif

	if (IRQs) {

		if (IRQs & INT_NMI) {	// NMI is highest priority
			pushW(rPC);
			pushW(rX);
			pushB(rA);
			pushB(rB);
			pushB(getCC());
			setI();		// mask interrupts during service routine
			rPC = readWord(VEC_NMI);
			IRQs &= ~INT_NMI;
#ifdef INSN_TRACE
			if (itr) {
				printf("-------- NMI\n");
			}
#endif
#ifdef DEBUG
			//printf("NMI@0x%x ", rPC); fflush(stdout);
#endif
#ifdef DBG_INTRS
			iPendCt = 0;
			iPendPrint = PEND_PRINT;
#endif
			goto doexecute;
		}

		if ((IRQs & INT_IRQ) && (getI() == 0)) {	/* IRQ is lowest priority */
			pushW(rPC);
			pushW(rX);
			pushB(rA);
			pushB(rB);
			pushB(getCC());
			setI(); /* Mask interrupts during service routine */
			rPC = readWord(VEC_IRQ);
			IRQs &= ~INT_IRQ; /* clear this bit */

#ifdef HPIB_SIM
			hpib_causeIRQ = FALSE;
#endif

#ifdef INSN_TRACE
			if (itr) {
				printf("-------- IRQ\n");
			}
#endif
#ifdef DEBUG
			//printf("IRQ@0x%x ", rPC); fflush(stdout);
	#ifdef HPIB_RECORD
			writeDev(0, 0, iCount, rPC, 1, getI());		// record the interrupt event
	#endif
#endif
#ifdef DBG_INTRS
			if (iPendCt) {
				PFC(dintr, "[TAKE iPEND=%d --->]\n", iPendCt);
			} else {
				PFC(dintr, "TAKE--->\n");
			}
			iPendCt = 0;
			iPendPrint = PEND_PRINT;
			i_latched = 0;
#endif
			goto doexecute;
		}
	}

#ifdef DBG_INTRS
	if (IRQs & INT_IRQ) {
		iPendCt++;
		if (iPendCt == iPendPrint) {
			PFC(dintr, "iPEND-%d\n", iPendCt);
			iPendPrint <<= 2;
		}
	} else {
		if (iPendCt) {
			PFC(dintr, "[GONE iPEND=%d msk? %s]\n", iPendCt, getI()? "YES":"NO");
			iPendCt = 0;
			iPendPrint = PEND_PRINT;
		}
	}
#endif

doexecute: ;

#ifdef FASTER_INTERRUPTS
	}
#endif

#ifdef ROM_RAM_COVER
	if(cover_sampling) rom_coverage[rPC-ROM_START] = 0xee;
#endif

#ifdef DEBUG
	
	inIRQ = !getI();

	opcode = rom[rPC];
	op_coverage[opcode]++;
	
	if (irq_trace && inIRQ) {
		itr |= 1;
	}
	
	if (iSnap) {
		if (iSnap == 1) { trace_dump(); printf("---- snap ----------------\n"); }
		iSnap++;
		if (iSnap == 16) { iSnap = 0; trace_clear(); } else itr |= 1;
	}

	if (iDump) {
		itr |= 1;
	}

	#ifdef INSN_TRACE
		if (iTrace) {
			itr |= 1;
		}
	#endif
	
	// don't print trace when in this particular delay loop
	if ((itr == 0) || ((rPC >= 0x6064) && (rPC <= 0x606f))) inh = 1; else inh = 0;
	
	int i; if (ispeed) for (i=0; i<ispeed; i++) bus_delay();

#endif

#ifdef MEAS_IPS
	{
		u4_t now;
		static u4_t last, last_ciCount, last_iiCount, last_iCount;

		now = sys_now();
		if (time_diff(now, last) >= 1000) {
			u4_t cic, iic;
			char *cicu=" ", *iicu=" ";
			u4_t icnt = ciCount + iiCount;
			if ((cic = ciCount-last_ciCount) > 1024) {
				cic /= 1024;
				cicu = "K";
			}
			if ((iic = iiCount-last_iiCount) > 1024) {
				iic /= 1024;
				iicu = "K";
			}
			printf("%4d%s + %4d%s = %4dK i/sec\n", cic, cicu, iic, iicu, (icnt-last_iCount)/1024);
			last = now;
			last_ciCount = ciCount;
			last_iiCount = iiCount;
			last_iCount = icnt;
		}
	}
#endif
	
#ifdef ROM_RAM_COVER
	
	// wait until after self-test checksum routine that touches all locations of ROM and RAM
	if (iCount > 50000) {
		cover_sampling = 1;
	}
#endif

	// interpreter

	#define _4CALL(dummy_dest)		goto branch_taken;
	#define BRANCH_ALWAYS()			goto branch_taken;
	#define BRANCH_TAKEN()			goto branch_taken;
	#define _2RESTORE_PC(pc)		
	#define CAPTURE_PC()		
	
	#define INSN(opc, opn, x) \
			case opc: \
				asm("# ---- " #opc " " #opn " ----------------");	/* asm appears as a comment in the compiled code */ \
				{ x; }; \
			break;

#ifdef INSN_TRACE
	if (itr) trace(rPC, getI(), rA, rB, rX, rSP, C, VNZ);
#endif

	opcode = rom[rPC];
	rPC++;
	INTERP_ICOUNT();

	switch (opcode) {

		#ifdef ROM_RAM_COVER
		
			#define _08_REL(rom_const)		tS16 = (s2_t) (s1_t) rom[rPC]; rPC++; \
											if (cover_sampling) rom_coverage[rPC-1-ROM_START] = 0xee;
			
			#define _08_IMM(rom_const)		tU8 = rom[rPC]; rPC++; \
											if (cover_sampling) rom_coverage[rPC-1-ROM_START] = 0xee;
			
			#define _16_WIM(rom_const)		tU16 = (rom[rPC] << 8) | rom[rPC+1]; rPC += 2; \
											if (cover_sampling) rom_coverage[rPC-2-ROM_START] = 0xee; \
											if (cover_sampling) rom_coverage[rPC-1-ROM_START] = 0xee;
			
			#define _08_DIR(rom_const)		tAddr = (u2_t) rom[rPC]; rPC++; \
											if (cover_sampling) rom_coverage[rPC-1-ROM_START] = 0xee;
			
			#define _16_EXT(rom_const)		tAddr = (rom[rPC] << 8) | rom[rPC+1]; rPC += 2; \
											if (cover_sampling) rom_coverage[rPC-2-ROM_START] = 0xee; \
											if (cover_sampling) rom_coverage[rPC-1-ROM_START] = 0xee;
			
			#define _08_IDX(rom_const)		tAddr = rX + (u2_t) rom[rPC]; rPC++; \
											if (cover_sampling) rom_coverage[rPC-1-ROM_START] = 0xee;
		
		#else
		
			// definition of the various addressing modes
			#define _08_REL(rom_const)		tS16 = (s2_t) (s1_t) rom[rPC]; rPC++;
			#define _08_IMM(rom_const)		tU8 = rom[rPC]; rPC++;
			#define _16_WIM(rom_const)		tU16 = (rom[rPC] << 8) | rom[rPC+1]; rPC += 2;
			#define _08_DIR(rom_const)		tAddr = (u2_t) rom[rPC]; rPC++;
			#define _16_EXT(rom_const)		tAddr = (rom[rPC] << 8) | rom[rPC+1]; rPC += 2;
			#define _08_IDX(rom_const)		tAddr = rX + (u2_t) rom[rPC]; rPC++;
		
		#endif
		
		// code implementing the interpretation of the instruction set is inserted here
		#include "6800.insns.h"
	
		default:
			D_PRF(("opc 0x%x rPC 0x%x\n", tU8, rPC));
			panic("bad opcode");
			break;
	}

	if (!sys_reset) goto next_insn;
}
