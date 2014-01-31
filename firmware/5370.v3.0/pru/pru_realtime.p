//
// Code that runs on the PRU to count overflows of the N0/N3 hardware registers and
// perform 5370 bus cycles.
//

.origin 0
.entrypoint start

#include "pru_realtime.hp"

	// shared memory used to communicate with CPU
	// layout must match pru_realtime.h
	.struct	c
		.u32	cmd
		.u32	count
		.u32	watchdog
		
		.u32	p0
		.u32	p1
		.u32	p2
		.u32	p3

		.u32	n0_ovfl					// overflow counts
		.u32	n3_ovfl
		.u32	ovfl_none

		.u32	a_n0st_0				// pre-scrambled RREG_N0ST address
		.u32	a_n0st_0c
		.u32	a_n0st_1
		.u32	a_n0st_1c
		.u32	a_n0st_3
		.u32	a_n0st_3c

		.u32	a_o3_0					// pre-scrambled RREG_O3 address
		.u32	a_o3_0c
		.u32	a_o3_1
		.u32	a_o3_1c
		.u32	a_o3_3
		.u32	a_o3_3c

		.u32	d_n0_clr_ovfl_0			// pre-scrambled WREG_O3_N0_CLR_OVFL data
		.u32	d_n0_clr_ovfl_0c
		.u32	d_n0_clr_ovfl_1
		.u32	d_n0_clr_ovfl_1c
		.u32	d_n0_clr_ovfl_2
		.u32	d_n0_clr_ovfl_2c

		.u32	d_n3_clr_ovfl_0			// pre-scrambled WREG_O3_N3_CLR_OVFL data
		.u32	d_n3_clr_ovfl_0c
		.u32	d_n3_clr_ovfl_1
		.u32	d_n3_clr_ovfl_1c
		.u32	d_n3_clr_ovfl_2
		.u32	d_n3_clr_ovfl_2c

		.u32	g_addr_0				// pre-scrambled address for general bus i/o
		.u32	g_addr_0c
		.u32	g_addr_1
		.u32	g_addr_1c
		.u32	g_addr_3
		.u32	g_addr_3c

		.u32	g_data_0				// pre/post-scrambled read/write data for general bus i/o
		.u32	g_data_0c
		.u32	g_data_1
		.u32	g_data_1c
		.u32	g_data_2
		.u32	g_data_2c

	.ends

	// register aliases
	.struct	regs
		.u32	com
		.u32	gpio0
		.u32	gpio1
		.u32	gpio2
		.u32	gpio3
		.u32	gpio_in
		.u32	gpio_oe
		.u32	gpio_set
		.u32	gpio_clr
		.u32	timer
		.u32	timer_tclr
		.u32	timer_tcrr
		.u32	p0
		.u32	p1
		.u32	p2
		.u32	p3
		.u32	p4
		.u32	p5
		.u32	n0_ovfl
		.u32	n3_ovfl
		.u32	ovfl_none
		.u32	ra1
		.u32	ra2
	.ends
	
	.assign	regs, r7, r29, r			// r0-r6 are temps, avoid r30-r31

start:
    // enable OCP master ports so we can access gpio & timer
    lbco	r1, c4, 4, 4				// SYSCFG register (C4 in constant table)
    clr		r1, r1, 4					// clear standby_init bit
    sbco	r1, c4, 4, 4

	// address pointers (too big for immediate field)
	mov		r.com, 0
	mov		r.gpio0, GPIO0_BASE
	mov		r.gpio1, GPIO1_BASE
	mov		r.gpio2, GPIO2_BASE
	mov		r.gpio3, GPIO3_BASE
	mov		r.gpio_in, _GPIO_IN
	mov		r.gpio_oe, _GPIO_OE
	mov		r.gpio_set, _GPIO_SET
	mov		r.gpio_clr, _GPIO_CLR
	mov		r.timer, TIMER4_BASE
	mov		r.timer_tclr, _TIMER_TCLR
	mov		r.timer_tcrr, _TIMER_TCRR

	mov		r1, 0
	eput	r1, r.com, c.watchdog
	jmp		cmd_clear

cmd_done:
	mov		r1, PRU_DONE
    eput	r1, r.com, c.cmd
    
cmd_get:
	eget	r1, r.com, c.cmd
	qbeq	cmd_ping, r1, PRU_PING
	qbeq	cmd_read, r1, PRU_READ
	qbeq	cmd_write, r1, PRU_WRITE
	qbeq	cmd_clear, r1, PRU_CLEAR

	// note that cmd_count() is run concurrently with command processing
	eget	r1, r.com, c.count
	qbeq	cmd_count, r1, PRU_COUNT
	jmp		cmd_get

// establish we're running by answering a ping
cmd_ping:
	eget	r1, r.com, c.p0
	eget	r2, r.com, c.p1
	add		r1, r1, r2
    eput	r1, r.com, c.p2
	jmp 	cmd_done

// 5370 bus read
cmd_read:
	eget	r0, r.com, c.g_addr_0
	eget	r1, r.com, c.g_addr_0c
	eget	r2, r.com, c.g_addr_1
	eget	r3, r.com, c.g_addr_1c
	eget	r4, r.com, c.g_addr_3
	eget	r5, r.com, c.g_addr_3c
	jsr2	set_addr
	jsr1	bus_read
	eput	r.p0, r.com, c.g_data_0
	eput	r.p1, r.com, c.g_data_1
	eput	r.p2, r.com, c.g_data_2
	jmp		cmd_done
	
// 5370 bus write
cmd_write:
	eget	r0, r.com, c.g_addr_0
	eget	r1, r.com, c.g_addr_0c
	eget	r2, r.com, c.g_addr_1
	eget	r3, r.com, c.g_addr_1c
	eget	r4, r.com, c.g_addr_3
	eget	r5, r.com, c.g_addr_3c
	jsr2	set_addr
	eget	r.p0, r.com, c.g_data_0
	eget	r.p1, r.com, c.g_data_0c
	eget	r.p2, r.com, c.g_data_1
	eget	r.p3, r.com, c.g_data_1c
	eget	r.p4, r.com, c.g_data_2
	eget	r.p5, r.com, c.g_data_2c
	jsr1	bus_write
	jmp		cmd_done

// clear overflow couters
cmd_clear:
	mov		r.n0_ovfl, 0
	eput	r.n0_ovfl, r.com, c.n0_ovfl
	mov		r.n3_ovfl, 0
	eput	r.n3_ovfl, r.com, c.n3_ovfl
	mov		r.ovfl_none, 0
	eput	r.ovfl_none, r.com, c.ovfl_none
	mov		r1, PRU_DONE
	eput	r1, r.com, c.count
	jmp 	cmd_done

// count N0/N3 counter overflows; stop when EOM (end-of-measurement) detected
cmd_count:
	eget	r1, r.com, c.watchdog
	add		r1, r1, 1
	eput	r1, r.com, c.watchdog

	jsr1	wait_bus_clk					// let bus clock run
	jsr1	addr_n0st
	jsr1	bus_read

check_n0:
	mov		r1, N0ST_N0_OVFL_GPIO			// on gpio1, active high
	and		r1, r.p1, r1
	qbeq	check_n3, r1, 0
	add		r.n0_ovfl, r.n0_ovfl, 1
	eput	r.n0_ovfl, r.com, c.n0_ovfl
	
	eget	r.p0, r.com, c.d_n0_clr_ovfl_0
	eget	r.p1, r.com, c.d_n0_clr_ovfl_0c
	eget	r.p2, r.com, c.d_n0_clr_ovfl_1
	eget	r.p3, r.com, c.d_n0_clr_ovfl_1c
	eget	r.p4, r.com, c.d_n0_clr_ovfl_2
	eget	r.p5, r.com, c.d_n0_clr_ovfl_2c
	
	jsr1	addr_out3
	jsr1	bus_write
	jmp		cmd_get

check_n3:
	mov		r1, N0ST_N3_OVFL_GPIO			// on gpio2, active high
	and		r1, r.p2, r1
	qbeq	check_eom, r1, 0
	add		r.n3_ovfl, r.n3_ovfl, 1
	eput	r.n3_ovfl, r.com, c.n3_ovfl
	
	eget	r.p0, r.com, c.d_n3_clr_ovfl_0
	eget	r.p1, r.com, c.d_n3_clr_ovfl_0c
	eget	r.p2, r.com, c.d_n3_clr_ovfl_1
	eget	r.p3, r.com, c.d_n3_clr_ovfl_1c
	eget	r.p4, r.com, c.d_n3_clr_ovfl_2
	eget	r.p5, r.com, c.d_n3_clr_ovfl_2c
	
	jsr1	addr_out3
	jsr1	bus_write
	jmp		cmd_get

check_eom:
	add		r.ovfl_none, r.ovfl_none, 1
	eput	r.ovfl_none, r.com, c.ovfl_none
	mov		r1, N0ST_EOM_GPIO				// on gpio1, active low
	and		r1, r.p1, r1
	qbne	cmd_get, r1, 0
	mov		r1, PRU_DONE
    eput	r1, r.com, c.count
	jmp		cmd_get


// ---------------------------

// returns: r.p0=gpio0 r.p1=gpio1 r.p2=gpio2
bus_read:
	jsr2	bus_clock_stop_deassert
	jsr2	wait_clk
	
	// FAST_READ_CYCLE()
	mov		r1, BUS_DIR
	st32	r1, r.gpio0, r.gpio_set
	mov		r1, BUS_LRW
	st32	r1, r.gpio0, r.gpio_clr
	mov		r1, BUS_LVMA
	st32	r1, r.gpio1, r.gpio_clr
	jsr2	wait_clk
	jsr2	bus_clock_assert
	
	jsr2	wait_access
	ld32	r.p0, r.gpio0, r.gpio_in
	not		r.p0, r.p0						// invert LDn
	ld32	r.p1, r.gpio1, r.gpio_in
	not		r.p1, r.p1
	ld32	r.p2, r.gpio2, r.gpio_in
	not		r.p2, r.p2
	
	jsr2	bus_clock_stop_deassert
	mov		r1, BUS_LVMA
	st32	r1, r.gpio1, r.gpio_set

	jsr2	wait_clk
	jsr2	bus_clock_start
	rtn1

// call with:
//		r.p0=gpio0_set r.p1=gpio0_clr
//		r.p2=gpio1_set r.p3=gpio1_clr
//		r.p4=gpio2_set r.p5=gpio2_clr
bus_write:
	jsr2	bus_clock_stop_deassert
	jsr2	wait_clk

	// FAST_WRITE_ENTER()
	mov		r1, BUS_DIR
	st32	r1, r.gpio0, r.gpio_clr

	mov		r0, (~G0_DATA)
	mov		r1, 0
	jsr2	set_oe0
	mov		r0, (~G1_DATA)
	mov		r1, 0
	jsr2	set_oe1
	mov		r0, (~G2_DATA)
	mov		r1, 0
	jsr2	set_oe2

	mov		r1, BUS_LRW
	st32	r1, r.gpio0, r.gpio_set
	mov		r1, BUS_LVMA
	st32	r1, r.gpio1, r.gpio_clr

	// FAST_WRITE_CYCLE()
	st32	r.p0, r.gpio0, r.gpio_clr		// data is active low, so swap set/clr sense
	st32	r.p1, r.gpio0, r.gpio_set
	st32	r.p2, r.gpio1, r.gpio_clr
	st32	r.p3, r.gpio1, r.gpio_set
	st32	r.p4, r.gpio2, r.gpio_clr
	st32	r.p5, r.gpio2, r.gpio_set

//	jsr2	wait_clk
	jsr2	wait_access
	jsr2	bus_clock_assert
	jsr2	wait_access
	jsr2	bus_clock_stop_deassert
	jsr2	wait_clk

	// FAST_WRITE_EXIT()
	mov		r1, BUS_LRW
	st32	r1, r.gpio0, r.gpio_clr
	mov		r1, BUS_LVMA
	st32	r1, r.gpio1, r.gpio_set

	mov		r0, 0xffffffff
	mov		r1, G0_DATA
	jsr2	set_oe0
	mov		r0, 0xffffffff
	mov		r1, G1_DATA
	jsr2	set_oe1
	mov		r0, 0xffffffff
	mov		r1, G2_DATA
	jsr2	set_oe2

	mov		r1, BUS_DIR
	st32	r1, r.gpio0, r.gpio_set

	jsr2	wait_clk
	jsr2	bus_clock_start
	rtn1

set_addr:
	st32	r0, r.gpio0, r.gpio_clr			// set/clr order reversed for LAn
	st32	r1, r.gpio0, r.gpio_set
	st32	r2, r.gpio1, r.gpio_clr
	st32	r3, r.gpio1, r.gpio_set
	st32	r4, r.gpio3, r.gpio_clr
	st32	r5, r.gpio3, r.gpio_set
	rtn2

addr_n0st:
	eget	r0, r.com, c.a_n0st_0
	eget	r1, r.com, c.a_n0st_0c
	eget	r2, r.com, c.a_n0st_1
	eget	r3, r.com, c.a_n0st_1c
	eget	r4, r.com, c.a_n0st_3
	eget	r5, r.com, c.a_n0st_3c
	jsr2	set_addr
	rtn1

addr_out3:
	eget	r0, r.com, c.a_o3_0
	eget	r1, r.com, c.a_o3_0c
	eget	r2, r.com, c.a_o3_1
	eget	r3, r.com, c.a_o3_1c
	eget	r4, r.com, c.a_o3_3
	eget	r5, r.com, c.a_o3_3c
	jsr2	set_addr
	rtn1

set_oe0:
	ld32	r3, r.gpio0, r.gpio_oe
	and		r3, r3, r0
	or		r3, r3, r1
	st32	r3, r.gpio0, r.gpio_oe
	rtn2

set_oe1:
	ld32	r3, r.gpio1, r.gpio_oe
	and		r3, r3, r0
	or		r3, r3, r1
	st32	r3, r.gpio1, r.gpio_oe
	rtn2

set_oe2:
	ld32	r3, r.gpio2, r.gpio_oe
	and		r3, r3, r0
	or		r3, r3, r1
	st32	r3, r.gpio2, r.gpio_oe
	rtn2
	
wait_clk:
	// 100 ns
	mov		r1, 10
wait_clk1:
	sub		r1, r1, 1
	qbne	wait_clk1, r1, 0
	rtn2
	
wait_access:
	// 800 ns
	mov		r1, 80
wait_access1:
	sub		r1, r1, 1
	qbne	wait_access1, r1, 0
	rtn2

wait_bus_clk:
	// 50 us
	mov		r1, 5000
wait_bus_clk1:
	sub		r1, r1, 1
	qbne	wait_bus_clk1, r1, 0
	rtn2
	
bus_clock_stop_deassert:
	mov		r1, T_MODE
	st32	r1,	r.timer, r.timer_tclr
	rtn2
	
bus_clock_assert:
	mov		r1, (T_MODE | T_HIGH)
	st32	r1,	r.timer, r.timer_tclr
	rtn2
	
bus_clock_start:
	mov		r1, -9
	st32	r1,	r.timer, r.timer_tcrr
	mov		r1, (T_MODE | T_START)
	st32	r1,	r.timer, r.timer_tclr
	rtn2
