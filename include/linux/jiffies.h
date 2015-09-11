#ifndef _LINUX_JIFFIES_H
#define _LINUX_JIFFIES_H

#include <linux/math64.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/time.h>
#include <linux/timex.h>
#include <asm/param.h>			/* for HZ */

/*
 * The following defines establish the engineering parameters of the PLL
 * model. The HZ variable establishes the timer interrupt frequency, 100 Hz
 * for the SunOS kernel, 256 Hz for the Ultrix kernel and 1024 Hz for the
 * OSF/1 kernel. The SHIFT_HZ define expresses the same value as the
 * nearest power of two in order to avoid hardware multiply operations.
 */
#if HZ >= 12 && HZ < 24
# define SHIFT_HZ	4
#elif HZ >= 24 && HZ < 48
# define SHIFT_HZ	5
#elif HZ >= 48 && HZ < 96
# define SHIFT_HZ	6
#elif HZ >= 96 && HZ < 192
# define SHIFT_HZ	7
#elif HZ >= 192 && HZ < 384
# define SHIFT_HZ	8
#elif HZ >= 384 && HZ < 768
# define SHIFT_HZ	9
#elif HZ >= 768 && HZ < 1536
# define SHIFT_HZ	10
#elif HZ >= 1536 && HZ < 3072
# define SHIFT_HZ	11
#elif HZ >= 3072 && HZ < 6144
# define SHIFT_HZ	12
#elif HZ >= 6144 && HZ < 12288
# define SHIFT_HZ	13
#else
# error Invalid value of HZ.
#endif

/* LATCH is used in the interval timer and ftape setup. */
#define LATCH  ((CLOCK_TICK_RATE + HZ/2) / HZ)	/* For divider */

/* Suppose we want to devide two numbers NOM and DEN: NOM/DEN, then we can
 * improve accuracy by shifting LSH bits, hence calculating:
 *     (NOM << LSH) / DEN
 * This however means trouble for large NOM, because (NOM << LSH) may no
 * longer fit in 32 bits. The following way of calculating this gives us
 * some slack, under the following conditions:
 *   - (NOM / DEN) fits in (32 - LSH) bits.
 *   - (NOM % DEN) fits in (32 - LSH) bits.
 */
#define SH_DIV(NOM,DEN,LSH) (   (((NOM) / (DEN)) << (LSH))              \
                             + ((((NOM) % (DEN)) << (LSH)) + (DEN) / 2) / (DEN))

/* HZ is the requested value. ACTHZ is actual HZ ("<< 8" is for accuracy) */
#define ACTHZ (SH_DIV (CLOCK_TICK_RATE, LATCH, 8))

/* TICK_NSEC is the time between ticks in nsec assuming real ACTHZ */
#define TICK_NSEC (SH_DIV (1000000UL * 1000, ACTHZ, 8))

/* TICK_USEC is the time between ticks in usec assuming fake USER_HZ */
#define TICK_USEC ((1000000UL + USER_HZ/2) / USER_HZ)

/* TICK_USEC_TO_NSEC is the time between ticks in nsec assuming real ACTHZ and	*/
/* a value TUSEC for TICK_USEC (can be set bij adjtimex)		*/
#define TICK_USEC_TO_NSEC(TUSEC) (SH_DIV (TUSEC * USER_HZ * 1000, ACTHZ, 8))

/* some arch's have a small-data section that can be accessed register-relative
 * but that can only take up to, say, 4-byte variables. jiffies being part of
 * an 8-byte variable may not be correctly accessed unless we force the issue
 */
#define __jiffy_data  __attribute__((section(".data")))

/*
 * The 64-bit value is not atomic - you MUST NOT read it
 * without sampling the sequence number in xtime_lock.
 * get_jiffies_64() will do this for you as appropriate.
 */
/*__jiffy_data表明这两个变量将出现在内核最终映像的.data区中*/
extern u64 __jiffy_data jiffies_64;
/*通常jiffies在linux系统启动引导阶段被初始化为0,当系统完成了对时钟中断的初始化之后,在每个
 *时钟中断处理例程中该值都会被加1,该值存储了系统自最近一次启动以来的时钟滴答*/
extern unsigned long volatile __jiffy_data jiffies;

#if (BITS_PER_LONG < 64)
u64 get_jiffies_64(void);
#else
/*如果在32系统上读取jiffies_64的值,必须使用get_jiffies_64函数,因为直接读取jiffies_64的
 *高32位或者低32位时,对应的低32位或者高32位可能已经发生更新,get_jiffies_64函数使用顺序锁
 *的方式来保证对jiffies_64变量读取操作的原子性*/
static inline u64 get_jiffies_64(void)
{
	return (u64)jiffies;
}
#endif

/*
 *	These inlines deal with timer wrapping correctly. You are 
 *	strongly encouraged to use them
 *	1. Because people otherwise forget
 *	2. Because if the timer wrap changes in future you won't have to
 *	   alter your driver code.
 *
 * time_after(a,b) returns true if the time a is after time b.
 *
 *EXAMPLE:假设驱动程序的某个函数demo_function需要调用比如do_time_task函数来完成
 *一个任务,但是对任务完成的时间有特定的要求(比如要在2毫秒内完成),如果在规定的时间内
 *没有完成,就需要调用task_timeout函数来处理,否则demo_function函数就算顺利完成
 * 下面的代码将是不安全的:
   int demo_function()
   {
   	unsigned long timeout = jiffies + 2*HZ/1000;  //设定超时的时间为2毫秒
	do_time_task();	//调用do_time_task来完成某一任务
	if(timeout < jiffies)	//根据当前最新的jiffies值来判断是否超时
		return task_timeout();
	return 0;
   }

   正确的用法:
   int demo_function()
   {
   	unsigned long timeout = jiffies + 2*HZ/1000;
	do_time_task();
	if(time_after(jiffies, timeout))
		return task_timeout();
	return 0;
   }

   延迟操作:
   write_command_reg(...); //CPU写外设的寄存器以发起一个操作指令
   delay(...);  //等待设备操作完成
   read_status_reg(...);  //CPU读取外设的寄存器以获得设备执行结果



 * Do this with "<0" and ">=0" to only test the sign of the result. A
 * good compiler would generate better code (and a really good compiler
 * wouldn't care). Gcc is currently neither.
 */
 /*判定时间点先后顺序的宏,如果时间点a在时间点b之后,该宏返回true*/
#define time_after(a,b)		\
	(typecheck(unsigned long, a) && \
	 typecheck(unsigned long, b) && \
	 ((long)(b) - (long)(a) < 0))
/*判定时间点先后顺序的宏,如果时间点a在时间点b之前,该宏返回true*/
#define time_before(a,b)	time_after(b,a)

/*该宏类似于time_after,但是在a和b两个时间点相等时,该宏也返回true*/
#define time_after_eq(a,b)	\
	(typecheck(unsigned long, a) && \
	 typecheck(unsigned long, b) && \
	 ((long)(a) - (long)(b) >= 0))
/*该宏类似于time_before,但是在a和b两个时间点相等时,该宏也返回true*/
#define time_before_eq(a,b)	time_after_eq(b,a)

/*
 * Calculate whether a is in the range of [b, c].
 */
/*该宏用来检查时间点a是否包含在时间间隔[b,c]内,因为检查包含边界,所以当a等于b或者c时,该
 *宏也返回true*/
#define time_in_range(a,b,c) \
	(time_after_eq(a,b) && \
	 time_before_eq(a,c))

/*
 * Calculate whether a is in the range of [b, c).
 */
#define time_in_range_open(a,b,c) \
	(time_after_eq(a,b) && \
	 time_before(a,c))

/* Same as above, but does so with platform independent 64bit types.
 * These must be used when utilizing jiffies_64 (i.e. return value of
 * get_jiffies_64() */
#define time_after64(a,b)	\
	(typecheck(__u64, a) &&	\
	 typecheck(__u64, b) && \
	 ((__s64)(b) - (__s64)(a) < 0))
#define time_before64(a,b)	time_after64(b,a)

#define time_after_eq64(a,b)	\
	(typecheck(__u64, a) && \
	 typecheck(__u64, b) && \
	 ((__s64)(a) - (__s64)(b) >= 0))
#define time_before_eq64(a,b)	time_after_eq64(b,a)

/*
 * These four macros compare jiffies and 'a' for convenience.
 */

/* time_is_before_jiffies(a) return true if a is before jiffies */
#define time_is_before_jiffies(a) time_after(jiffies, a)

/* time_is_after_jiffies(a) return true if a is after jiffies */
#define time_is_after_jiffies(a) time_before(jiffies, a)

/* time_is_before_eq_jiffies(a) return true if a is before or equal to jiffies*/
#define time_is_before_eq_jiffies(a) time_after_eq(jiffies, a)

/* time_is_after_eq_jiffies(a) return true if a is after or equal to jiffies*/
#define time_is_after_eq_jiffies(a) time_before_eq(jiffies, a)

/*
 * Have the 32 bit jiffies value wrap 5 minutes after boot
 * so jiffies wrap bugs show up earlier.
 */
#define INITIAL_JIFFIES ((unsigned long)(unsigned int) (-300*HZ))

/*
 * Change timeval to jiffies, trying to avoid the
 * most obvious overflows..
 *
 * And some not so obvious.
 *
 * Note that we don't want to return LONG_MAX, because
 * for various timeout reasons we often end up having
 * to wait "jiffies+1" in order to guarantee that we wait
 * at _least_ "jiffies" - so "jiffies+1" had better still
 * be positive.
 */
#define MAX_JIFFY_OFFSET ((LONG_MAX >> 1)-1)

extern unsigned long preset_lpj;

/*
 * We want to do realistic conversions of time so we need to use the same
 * values the update wall clock code uses as the jiffies size.  This value
 * is: TICK_NSEC (which is defined in timex.h).  This
 * is a constant and is in nanoseconds.  We will use scaled math
 * with a set of scales defined here as SEC_JIFFIE_SC,  USEC_JIFFIE_SC and
 * NSEC_JIFFIE_SC.  Note that these defines contain nothing but
 * constants and so are computed at compile time.  SHIFT_HZ (computed in
 * timex.h) adjusts the scaling for different HZ values.

 * Scaled math???  What is that?
 *
 * Scaled math is a way to do integer math on values that would,
 * otherwise, either overflow, underflow, or cause undesired div
 * instructions to appear in the execution path.  In short, we "scale"
 * up the operands so they take more bits (more precision, less
 * underflow), do the desired operation and then "scale" the result back
 * by the same amount.  If we do the scaling by shifting we avoid the
 * costly mpy and the dastardly div instructions.

 * Suppose, for example, we want to convert from seconds to jiffies
 * where jiffies is defined in nanoseconds as NSEC_PER_JIFFIE.  The
 * simple math is: jiff = (sec * NSEC_PER_SEC) / NSEC_PER_JIFFIE; We
 * observe that (NSEC_PER_SEC / NSEC_PER_JIFFIE) is a constant which we
 * might calculate at compile time, however, the result will only have
 * about 3-4 bits of precision (less for smaller values of HZ).
 *
 * So, we scale as follows:
 * jiff = (sec) * (NSEC_PER_SEC / NSEC_PER_JIFFIE);
 * jiff = ((sec) * ((NSEC_PER_SEC * SCALE)/ NSEC_PER_JIFFIE)) / SCALE;
 * Then we make SCALE a power of two so:
 * jiff = ((sec) * ((NSEC_PER_SEC << SCALE)/ NSEC_PER_JIFFIE)) >> SCALE;
 * Now we define:
 * #define SEC_CONV = ((NSEC_PER_SEC << SCALE)/ NSEC_PER_JIFFIE))
 * jiff = (sec * SEC_CONV) >> SCALE;
 *
 * Often the math we use will expand beyond 32-bits so we tell C how to
 * do this and pass the 64-bit result of the mpy through the ">> SCALE"
 * which should take the result back to 32-bits.  We want this expansion
 * to capture as much precision as possible.  At the same time we don't
 * want to overflow so we pick the SCALE to avoid this.  In this file,
 * that means using a different scale for each range of HZ values (as
 * defined in timex.h).
 *
 * For those who want to know, gcc will give a 64-bit result from a "*"
 * operator if the result is a long long AND at least one of the
 * operands is cast to long long (usually just prior to the "*" so as
 * not to confuse it into thinking it really has a 64-bit operand,
 * which, buy the way, it can do, but it takes more code and at least 2
 * mpys).

 * We also need to be aware that one second in nanoseconds is only a
 * couple of bits away from overflowing a 32-bit word, so we MUST use
 * 64-bits to get the full range time in nanoseconds.

 */

/*
 * Here are the scales we will use.  One for seconds, nanoseconds and
 * microseconds.
 *
 * Within the limits of cpp we do a rough cut at the SEC_JIFFIE_SC and
 * check if the sign bit is set.  If not, we bump the shift count by 1.
 * (Gets an extra bit of precision where we can use it.)
 * We know it is set for HZ = 1024 and HZ = 100 not for 1000.
 * Haven't tested others.

 * Limits of cpp (for #if expressions) only long (no long long), but
 * then we only need the most signicant bit.
 */

#define SEC_JIFFIE_SC (31 - SHIFT_HZ)
#if !((((NSEC_PER_SEC << 2) / TICK_NSEC) << (SEC_JIFFIE_SC - 2)) & 0x80000000)
#undef SEC_JIFFIE_SC
#define SEC_JIFFIE_SC (32 - SHIFT_HZ)
#endif
#define NSEC_JIFFIE_SC (SEC_JIFFIE_SC + 29)
#define USEC_JIFFIE_SC (SEC_JIFFIE_SC + 19)
#define SEC_CONVERSION ((unsigned long)((((u64)NSEC_PER_SEC << SEC_JIFFIE_SC) +\
                                TICK_NSEC -1) / (u64)TICK_NSEC))

#define NSEC_CONVERSION ((unsigned long)((((u64)1 << NSEC_JIFFIE_SC) +\
                                        TICK_NSEC -1) / (u64)TICK_NSEC))
#define USEC_CONVERSION  \
                    ((unsigned long)((((u64)NSEC_PER_USEC << USEC_JIFFIE_SC) +\
                                        TICK_NSEC -1) / (u64)TICK_NSEC))
/*
 * USEC_ROUND is used in the timeval to jiffie conversion.  See there
 * for more details.  It is the scaled resolution rounding value.  Note
 * that it is a 64-bit value.  Since, when it is applied, we are already
 * in jiffies (albit scaled), it is nothing but the bits we will shift
 * off.
 */
#define USEC_ROUND (u64)(((u64)1 << USEC_JIFFIE_SC) - 1)
/*
 * The maximum jiffie value is (MAX_INT >> 1).  Here we translate that
 * into seconds.  The 64-bit case will overflow if we are not careful,
 * so use the messy SH_DIV macro to do it.  Still all constants.
 */
#if BITS_PER_LONG < 64
# define MAX_SEC_IN_JIFFIES \
	(long)((u64)((u64)MAX_JIFFY_OFFSET * TICK_NSEC) / NSEC_PER_SEC)
#else	/* take care of overflow on 64 bits machines */
# define MAX_SEC_IN_JIFFIES \
	(SH_DIV((MAX_JIFFY_OFFSET >> SEC_JIFFIE_SC) * TICK_NSEC, NSEC_PER_SEC, 1) - 1)

#endif

/*
 * Convert various time units to each other:
 */
/*转换时间单位为为ms,易于人类理解*/
extern unsigned int jiffies_to_msecs(const unsigned long j);
/*换算成微妙us*/
extern unsigned int jiffies_to_usecs(const unsigned long j);
extern unsigned long msecs_to_jiffies(const unsigned int m);
extern unsigned long usecs_to_jiffies(const unsigned int u);
/*jiffies变量和数据结构之间的转换*/
extern unsigned long timespec_to_jiffies(const struct timespec *value);
extern void jiffies_to_timespec(const unsigned long jiffies,
				struct timespec *value);
/*jiffies变量和数据结构之间的转换*/
extern unsigned long timeval_to_jiffies(const struct timeval *value);
extern void jiffies_to_timeval(const unsigned long jiffies,
			       struct timeval *value);
extern clock_t jiffies_to_clock_t(unsigned long x);
extern unsigned long clock_t_to_jiffies(unsigned long x);
extern u64 jiffies_64_to_clock_t(u64 x);
extern u64 nsec_to_clock_t(u64 x);

#define TIMESTAMP_SIZE	30

#endif
