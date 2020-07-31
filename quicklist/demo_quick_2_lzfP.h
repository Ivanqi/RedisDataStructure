#ifndef LZFP_2_h
#define LZFP_2_h

#define STANDALONE 1

#ifndef STANDALONE
#include "demo_quick_2_lzf.h"
#endif

/**
 * 哈希表的大小为（1<<HLOG）*sizeof（char*）
 * 解压与哈希表大小无关
 * 15和14之间的差别很小
 * 对于小区间（和14通常更快）。
 * 对于低内存/更快的配置，使用HLOG==13；
 * 要获得最佳压缩效果，请使用15或16（或更多，最多22个）。
 */
#ifndef HLOG
#define HLOG 16
#endif

/**
 * 牺牲很少的压缩质量而有利于压缩速度
 * 这提供了与默认代码几乎相同的压缩，并且（大致）快了15%
 * 这是首选的操作模式
 */
#ifndef VERY_FAST
#define VERY_FAST 1
#endif

/**
 * 为了提高压缩速度，牺牲更多的压缩质量
 * （大数据块的压缩性能下降约1-2%，小型冗余块的压缩性能下降9-10%，两种情况下的压缩速度都提高了>>20%）
 * 简而言之：当需要速度时，对二进制数据启用此功能，对于文本数据可能禁用此功能。
 */
#ifndef ULTRA_FAST
#define ULTRA_FAST 0
#endif

// 无条件对齐不会花费太多，因此如果不确定，也可以这样做
#ifndef STRICT_ALIGN
# if !(defined(__i386) || defined (__amd64))
#  define STRICT_ALIGN 1
# else
#  define STRICT_ALIGN 0
# endif
#endif

/**
 * 您可以选择预先设置哈希表
 *（在某些情况下可能更快现代CPU和大型（>>64k）块，并且当配置相同时，也使压缩具有确定性/可重复性）。
 */
#ifndef INIT_HTAB
#define INIT_HTAB 0
#endif

/**
 * 避免给errno变量赋值？
 * 对于某些嵌入目的（例如linux内核），这是必要的
 * 注意：这破坏了lzf.h中的文档。避免errno不会影响速度。
 */
#ifndef AVOID_ERRNO
#define AVOID_ERRNO 0
#endif

/**
 * 是将LZF_状态变量作为参数传递，还是在堆栈上分配它
 * 对于小堆栈环境，将其定义为1
 * 注意：这破坏了lzf.h中的原型
 */
#ifndef LZF_STATE_ARG
#define LZF_STATE_ARG 0
#endif

/**
 * 是否在lzf_decompress中添加额外的输入有效性检查，如果输入流已损坏，则返回EINVAL
 * 这仅能防止输入缓冲区溢出，并且不会检测到大多数损坏的流
 * 此检查通常在现代硬件上不明显（速度下降<1％），但可能会大大降低较旧的CPU的速度
 */
#ifndef CHECK_INPUT
#define CHECK_INPUT 1
#endif

#ifdef __cplusplus
#include <cstring>
#include <climits>
using namespace std;
#else
#include <string.h>
#include <limits.h>
#endif

#ifndef LZF_USE_OFFSETS
#if defined (WIN32)
#define LZF_USE_OFFSETS defined(_M_X64)
#else
#if __cplusplus > 199711L
#include <cstdint>
#else
#include <stdint.h>
#endif
#define LZF_USE_OFFSETS (UINTPTR_MAX > 0xffffffffU)
#endif
#endif

typedef unsigned char u8;

#if LZF_USE_OFFSETS
#define LZF_HSLOT_BIAS ((const u8 *)in_data)
typedef unsigned int LZF_HSLOT;
#else
#define LZF_HSLOT_BIAS 0
typedef const u8 *LZF_HSLOT;
#endif

typedef LZF_HSLOT LZF_STATE[1 << (HLOG)];

#if !STRICT_ALIGN
/* for unaligned accesses we need a 16 bit datatype. */
#if USHRT_MAX == 65535
typedef unsigned short u16;
#elif UINT_MAX == 65535
typedef unsigned int u16;
#else
#undef STRICT_ALIGN
#define STRICT_ALIGN 1
#endif
#endif

#if ULTRA_FAST
#undef VERY_FAST
#endif

#endif