#ifndef _PRU_REALTIME_H_
#define _PRU_REALTIME_H_

#define	PRU_DONE		0
#define	PRU_PING		1
#define	PRU_READ		2
#define	PRU_WRITE		3
#define	PRU_CLEAR		4

#define	PRU_COUNT		1


// no -I path when using pasm
#include "../arch/sitara/sitara.h"
#include "../5370/5370_regs.h"

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
	CONV_ADDR_DCL(a_n0st);
	CONV_ADDR_DCL(a_o3);
	CONV_DATA_DCL(d_n0_clr_ovfl);
	CONV_DATA_DCL(d_n3_clr_ovfl);
	CONV_ADDR_DCL(g_addr);
	CONV_DATA_DCL(g_data);
} com_t;

extern com_t *pru;

void pru_start();

#endif

#endif
