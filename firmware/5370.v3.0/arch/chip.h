#ifndef _CHIP_H_
#define _CHIP_H_

#if 1
	#define set_register(reg, data) \
		reg = data
#else
	#define set_register(reg, data) ({ \
		printf("set *%x = %x\n", (u4_t) &reg, data); \
		reg = data; \
	})
#endif

	#define set_reg_ptr(reg, data) \
		*(reg) = data

#if 1
	#define get_register(reg) \
		reg
#else
	#define get_register(reg) ({ \
		printf("get %x = *%x\n", (u4_t) reg, (u4_t) &reg); \
		reg; \
	})
#endif

	#define get_reg_ptr(reg) \
		*(reg)

#ifdef INTERRUPTS
	u4_t hw_check_irq();
	u4_t hw_check_nmi();
	
	// this is more expensive than polling the interrupt line (as is done below)
	// but will catch level-sensitive interrupts that polling would otherwise miss
	#define CHECK_IRQ(iCount) hw_check_irq()
	#define CHECK_NMI(iCount) hw_check_nmi()
#endif

#include ARCH_INCLUDE

void bus_init();
void bus_gpio_init();
u4_t bus_fast_read(u4_t addr);
void bus_fast_write(u4_t addr, u4_t data);
void bus_delay();

#endif
