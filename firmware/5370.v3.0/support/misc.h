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

#ifdef CLIENT_SIDE
 #define sys_now() 0
#else
 u4_t sys_now(void);
#endif

u4_t time_diff(u4_t t1, u4_t t2);
void delay(u4_t msec);

#endif
