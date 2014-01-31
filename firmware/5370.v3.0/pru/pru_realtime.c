/*
 * Copyright (C) 2012 Texas Instruments Incorporated - http://www.ti.com/ 
 * 
 * 
 *  Redistribution and use in source and binary forms, with or without 
 *  modification, are permitted provided that the following conditions 
 *  are met:
 *
 *    Redistributions of source code must retain the above copyright 
 *    notice, this list of conditions and the following disclaimer.
 *
 *    Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the 
 *    documentation and/or other materials provided with the   
 *    distribution.
 *
 *    Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT 
 *  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
 *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
*/

/*
 * ============================================================================
 * Copyright (c) Texas Instruments Inc 2010-12
 *
 * Use of this software is controlled by the terms and conditions found in the
 * license agreement under which this software has been supplied or provided.
 * ============================================================================
 */

/*****************************************************************************
*
* The PRU reads and stores values into the PRU Data RAM memory. PRU Data RAM 
* memory has an address in both the local data memory map and global memory 
* map. The example accesses the local Data RAM using both the local address 
* through a register pointed base address and the global address pointed by 
* entries in the constant table. 
*
******************************************************************************/

#include "boot.h"
#include "pru_realtime.h"
#include <stdio.h>

#include <prussdrv.h>
#include <pruss_intc_mapping.h>

#define PRU_NUM 	0

com_t *pru;
static void *pruDataMem;

void pru_start()
{
    unsigned int ret;
    tpruss_intc_initdata pruss_intc_initdata = PRUSS_INTC_INITDATA;

    prussdrv_init();		
    if (prussdrv_open(PRU_EVTOUT_0)) panic("prussdrv_open");
    prussdrv_pruintc_init(&pruss_intc_initdata);

	prussdrv_map_prumem(PRUSS0_PRU0_DATARAM, &pruDataMem);
    pru = (com_t *) pruDataMem;

    prussdrv_exec_program(PRU_NUM, "pru/pru_realtime.bin");
    
    pru->p[2] = 0;
    u4_t key1 = sys_now_us();
    u4_t key2 = key1 >> 8;
    pru->p[0] = key1;
    pru->p[1] = key2;
    pru->cmd = PRU_PING;
    while (pru->cmd != PRU_DONE);
    if (pru->p[2] != (key1+key2)) panic("PRU didn't start");
    printf("PRU started\n");

#if 0
    prussdrv_pru_wait_event(PRU_EVTOUT_0);		// wait for halt
    prussdrv_pru_clear_event(PRU_EVTOUT_0, PRU0_ARM_INTERRUPT);
    prussdrv_pru_disable(PRU_NUM);
    prussdrv_exit();
#endif
}
