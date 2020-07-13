#ifndef __REDIS_UTIL_2_H
#define __REDIS_UTIL_2_H

#include <stdint.h>
#include "demo_sds_2.h"

#define MAX_LONG_DOUBLE_CHARS 5 * 1024

typedef enum {
    LD_STR_AUTO,
    LD_STR_HUMM,
    LD_STR_HEX
} ld2string_mode;

int stringmatchlen(const char *p, int plen, const char *s, int slen, int nocase);
int stringmatch(const char *p, const char *s, int nocase);
int stringmatchlen_fuzz_test(void);
long long memtoll(const char *p, int *err);
uint32_t digits10(uint64_t v);
uint32_t sdigits10(int64_t v);
int ll2string(char *s, size_t len, long long value);
int string2ll(const char *s, size_t slen, long long *value);
int string2l(const char *s, size_t slen, long *value);
int string2ld(const char *s, size_t slen, long double *dp);
int d2string(char *buf, size_t len, double value);
int ld2string(char *buf, size_t len, long double value, ld2string_mode mode);
sds getAbsolutePath(char *filename);
unsigned long getTimeZone(void);
int pathIsBaseName(char *path);

#endif