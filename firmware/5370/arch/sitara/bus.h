#ifndef _BUS_H_
#define _BUS_H_

// bus signal mapping between microcontroller and 5370 bus for various setups
// recall that the 5370 bus signals are active low and therefore named BUS_Lxxx

#ifdef SETUP_TIME_NUTS_PCB_V3

// busses are scrambled across the four gpios to make pcb layout easier

#ifndef _PASM_
	volatile u4_t *gpio0, *gpio1, *gpio2, *gpio3;
	
	#define	B0	0x01
	#define	B1	0x02
	#define	B2	0x04
	#define	B3	0x08
	#define	B4	0x10
	#define	B5	0x20
	#define	B6	0x40
	#define	B7	0x80
#endif


// on GPIO0:
#define BUS_LA5		0x00000004		// 1<<2
#define BUS_LA6		0x00000008		// 1<<3
#define	BUS_LRW		0x00000010
#define	BUS_LIRQ	0x00000020
#define BUS_LA2		0x00004000		// 1<<14
#define BUS_LA3		0x00008000		// 1<<15
#define BUS_DIR		0x00400000		// 1 = read, direction pin on 74LVC4245 xcvr
#define BUS_LD4		0x00800000		// 1<<23
#define BUS_LD3		0x04000000		// 1<<26
#define BUS_LD0		0x08000000		// 1<<27
#define BUS_LRST	0x40000000

#define	G0_ADDR		(BUS_LA6 | BUS_LA5 | BUS_LA3 | BUS_LA2)
#define	G0_DATA		(BUS_LD4 | BUS_LD3 | BUS_LD0)

#define GPIO0_AVAIL	0x80000000		// means available for our future use
#define GPIO0_LCD	0x00000f00
#define GPIO0_CONF	0x00003000		// cape expansion configuration EEPROM
#define GPIO0_MULT	0x00100080		// multi-use pins
#define GPIO0_UNK	0x332f0043		// other unknown use by BBB (not on expansion header)


// on GPIO1:
#define BUS_LD5		0x00001000		// 1<<12
#define BUS_LD6		0x00002000		// 1<<13
#define BUS_LD1		0x00004000		// 1<<14
#define BUS_LD2		0x00008000		// 1<<15
#define BUS_LA4		0x00020000		// 1<<17
#define BUS_LNMI	0x00080000
#define BUS_LVMA	0x20000000

#define	G1_ADDR		(BUS_LA4)
#define	G1_DATA		(BUS_LD6 | BUS_LD5 | BUS_LD2 | BUS_LD1)

#define GPIO1_AVAIL	0x10050000
#define GPIO1_EMMC	0xc00000ff
#define GPIO1_UNK	0x0ff00f00
#define	GPIO1_LEDS	0x01e00000


// on GPIO2:
#define BUS_OE		0x00000002		// 1 = Z, oe pin on 74LVC4245 xcvr
#define BUS_CLK		0x00000004
#define BUS_LD7		0x00000010		// 1<<4

#define	G2_DATA		(BUS_LD7)

#define GPIO2_AVAIL	0x00000028
#define GPIO2_LCD	0x03e3ffc0
#define GPIO2_UNK	0xfc1c0001


// on GPIO3:
#define BUS_LA0		0x00010000		// 1<<16
#define BUS_LA1		0x00080000		// 1<<19

#define	G3_ADDR		(BUS_LA1 | BUS_LA0)

#define GPIO3_LCD	0x0022c000
#define GPIO3_MULT	0x00140000
#define GPIO3_UNK	0xffc03fff

#endif

#endif
