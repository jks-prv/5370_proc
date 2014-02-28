#ifndef _PRINTF_H_
#define _PRINTF_H_

#include "config.h"

#ifdef NET_PRINTF
 #define printf		lprintf
#endif

#endif
