#ifndef _BOOT_H_
#define _BOOT_H_

#include "config.h"

#include <stdlib.h>

typedef unsigned int        u4_t;
typedef unsigned char       u1_t;
typedef unsigned short      u2_t;

typedef signed long         s4_t;
typedef signed short        s2_t;
typedef signed char         s1_t;

typedef	unsigned char		bool;

#ifndef TRUE
	#define TRUE 1
#endif
#ifndef FALSE
	#define FALSE 0
#endif

#define	K	1024
#define	M	(K*K)
#define	B	(M*K)

#include "misc.h"

bool sys_reset;

#endif
