#ifndef _MISC_H_
#define _MISC_H_

#include "boot.h"

#ifdef DEBUG
	#define D_PRF(x) printf x;
	#define D_STMT(x) x;
	#define assert(e) \
		if (!(e)) { \
			printf("assertion failed: \"%s\" %s line %d\n", #e, __FILE__, __LINE__); \
			panic("assert"); \
		}
	#define assert_exit(e) \
		if (!(e)) { \
			printf("assertion failed: \"%s\" %s line %d\n", #e, __FILE__, __LINE__); \
			goto error_exit; \
		}
#else
	#define D_PRF(x)
	#define D_STMT(x)
	#define assert(e)
	#define assert_exit(e)
#endif

void lprintf(char *fmt, ...);
void panic(char *str);
void sys_panic(char *str);
void xit(int err);

#define scall(x, y) if ((y) < 0) sys_panic(x);
#define scallz(x, y) if ((y) == 0) sys_panic(x);

int split(char *cp, int *argc, char *argv[], int nargs);

#ifdef CLIENT_SIDE
 #define timer_ms() 0
#else
 u4_t timer_ms(void);
 u4_t timer_us(void);
#endif

u4_t time_diff(u4_t t1, u4_t t2);
void delay(u4_t msec);

#ifdef REG_RECORD
	typedef enum { REG_READ, REG_WRITE, REG_STR, REG_RESET } reg_type_t;
	
	typedef struct {
		u4_t iCount;
		u2_t rPC;
		u1_t n_irq;
		u4_t irq_masked;
	} t_hr_stamp;

	typedef void (callback_t)(u2_t addr, u1_t data, u4_t dup, u4_t time, char *str);

	void reg_record(int chan, reg_type_t type, u2_t addr, u1_t data, u4_t time, char *str);
	void reg_stamp(int write, u4_t iCount, u2_t rPC, u1_t n_irq, u4_t irq_masked);
	t_hr_stamp *reg_get_stamp();
	void reg_dump(u1_t chan, callback_t rdecode, callback_t wdecode, callback_t sdecode);
	void reg_cmp(callback_t adecode);
#else
	#define reg_record(chan, type, addr, data, time, str)
	#define reg_stamp(write, iCount, rPC, n_irq, irq_masked)
	#define reg_get_stamp()
	#define reg_dump(chan, rdecode, wdecode, sdecode)
	#define reg_cmp(adecode)
#endif

#endif
