#ifndef __ENDIANCONV_2_H
#define __ENDIANCONV_2_H

#include <stdint.h>

#ifndef BYTE_ORDER
#if (BSD >= 199103)
# include <machine/endian.h>
#else
#if defined(linux) || defined(__linux__)
# include <endian.h>
#else
#define	LITTLE_ENDIAN	1234	/* least-significant byte first (vax, pc) */
#define	BIG_ENDIAN	4321	   /* most-significant byte first (IBM, net) */
#define	PDP_ENDIAN	3412	   /* LSB first in word, MSW first in long (pdp)*/

#if defined(__i386__) || defined(__x86_64__) || defined(__amd64__) || \
   defined(vax) || defined(ns32000) || defined(sun386) || \
   defined(MIPSEL) || defined(_MIPSEL) || defined(BIT_ZERO_ON_RIGHT) || \
   defined(__alpha__) || defined(__alpha)
#define BYTE_ORDER    LITTLE_ENDIAN
#endif

#if defined(sel) || defined(pyr) || defined(mc68000) || defined(sparc) || \
   defined(is68k) || defined(tahoe) || defined(ibm032) || defined(ibm370) || \
   defined(MIPSEB) || defined(_MIPSEB) || defined(_IBMR2) || defined(DGUX) ||\
   defined(apollo) || defined(__convex__) || defined(_CRAY) || \
   defined(__hppa) || defined(__hp9000) || \
   defined(__hp9000s300) || defined(__hp9000s700) || \
   defined (BIT_ZERO_ON_LEFT) || defined(m68k) || defined(__sparc)
#define BYTE_ORDER	BIG_ENDIAN
#endif
#endif /* linux */
#endif /* BSD */
#endif /* BYTE_ORDER */

void memrev16(void *p);
void memrev32(void *p);
void memrev64(void *p);

uint16_t intrev16(uint64_t v);
uint32_t intrev32(uint32_t v);
uint64_t intrev64(uint64_t v);


#if (BYTE_ORDER == LITTLE_ENDIAN)
#define memrev16ifbe(p) ((void)(0))
#define memrev32ifbe(p) ((void)(0))
#define memrev64ifbe(p) ((void)(0))
#define intrev16ifbe(v) (v)
#define intrev32ifbe(v) (v)
#define intrev64ifbe(v) (v)
#else
#define memrev16ifbe(p) memrev16(p)
#define memrev32ifbe(p) memrev32(p)
#define memrev64ifbe(p) memrev64(p)
#define intrev16ifbe(v) intrev16(v)
#define intrev32ifbe(v) intrev32(v)
#define intrev64ifbe(v) intrev64(v)
#endif

#if (BYTE_ORDER == BIG_ENDIAD)
#define htonu64(v) (v)
#define ntohu64(v) (v)
#else
#define htonu64(v) intrev64(v)
#define ntohu64(v) intrev64(v)

#endif