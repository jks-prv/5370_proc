#ifndef _CONFIG_H_
#define _CONFIG_H_

// these defines are set by the makefile:
// {HP5370A, HP5370B, HP5359A}
// SETUP_*
// CHIP_INCLUDE
// DEBUG
// INSN_TRACE
// FIRMWARE_VER_MAJ, FIRMWARE_VER_MIN


// configuration options
//#define SIM_INPUT_NET			// network connection talks to sim input instead of HPIB
#define HPIB_SIM				// simulates the HPIB hardware board so the unmodified firmware can continue to work
#define HPIB_FAST_BINARY_PRU	// use the PRU to collect HPIB fast binary data
//#define INTERRUPTS			// real interrupts instead of polling (not fully working yet)
//#define FASTER_INTERRUPTS
//#define HPIB_ECHO_INPUT
//#define MEAS_IP_HPIB_MEAS		// measure instructions-per-HPIB-measurement
//#define MEAS_IPS				// measure instructions-interpreted-per-second

#ifdef DEBUG
 //#define BUS_TEST
 //#define DBG_INTRS
 #define HPIB_SIM_DEBUG
 //#define RECORD_IO_HIST
 //#define HPIB_RECORD			// record HPIB bus cycles for use in developing HPIB_SIM
 //#define FREQ_DEBUG
#endif

#if defined(HPIB_RECORD) || defined(HPIB_SIM_DEBUG) || defined(FREQ_DEBUG)
 #define REG_RECORD
#endif

#ifdef HP5370A
	#define ROM_CODE_H		"rom_5370a.h"
	#define INST_STR		"5370A"
	#define INST_NAME		"HP 5370A Universal Time Interval Counter"
#endif

#ifdef HP5370B
	#define ROM_CODE_H		"rom_5370b.h"
	#define INST_STR		"5370B"
	#define INST_NAME		"HP 5370B Universal Time Interval Counter"
#endif

#ifdef HP5359A
	#define ROM_CODE_H		"rom_5359a.h"
	#define INST_STR		"5359A"
	#define INST_NAME		"HP 5359A Time Synthesizer"
#endif

#endif
