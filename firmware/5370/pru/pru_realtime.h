#ifndef _PRU_REALTIME_H_
#define _PRU_REALTIME_H_

#define	PRU_DONE			0
#define	PRU_PING			1
#define	PRU_READ			2
#define	PRU_WRITE			3
#define	PRU_CLEAR			4
#define PRU_BUS_CLK_STOP	5
#define PRU_BUS_CLK_START	6
#define	PRU_TI_MEAS			7
#define	PRU_HALT			8

#define	PRU_COUNT		1


// no -I path when using pasm
#include "../arch/sitara/bus.h"
#include "../arch/sitara/sitara.h"
#include "../5370/5370_regs.h"

#define PRU_COM_SIZE	(64*4)		// 64 entries max in a PRU assembler .struct
#define PRU_COM_MEM		0
#define PRU_COM_MEM2	(PRU_COM_MEM + PRU_COM_SIZE)

#ifndef _PASM_

#include "misc.h"

// shared memory used to communicate with PRU
// layout must match pru_realtime.p
typedef volatile struct {
	u4_t cmd;
	u4_t count;
	u4_t watchdog;
	u4_t p[4];

	u4_t n0_ovfl, n3_ovfl, ovfl_none;

	CONV_ADDR_DCL(a_gen);
	
	CONV_ADDR_DCL(a_n0st);
	CONV_ADDR_DCL(a_n0_h);
	CONV_ADDR_DCL(a_n1n2_h);
	CONV_ADDR_DCL(a_o3);
	
	CONV_DATA_DCL(d_n0_clr_ovfl);
	CONV_DATA_DCL(d_n3_clr_ovfl);
} com_t;

extern com_t *pru;

typedef volatile struct {
	u4_t m2_offset;
	
	CONV_DATA_DCL(d_gen);
	
	CONV_ADDR_DATA_DCL(ad_rst);
	CONV_ADDR_DATA_DCL(ad_ena);
	CONV_ADDR_DATA_DCL(ad_arm);
	CONV_ADDR_DATA_DCL(ad_idle);

	CONV_DATA_READ_DCL(d_n0st);
	CONV_DATA_READ_DCL(d_n1n2_h);
	CONV_DATA_READ_DCL(d_n1n2_l);
	CONV_DATA_READ_DCL(d_n0_h);
	CONV_DATA_READ_DCL(d_n0_l);
} com2_t;

extern com2_t *pru2;

void pru_start();

#endif

#endif
