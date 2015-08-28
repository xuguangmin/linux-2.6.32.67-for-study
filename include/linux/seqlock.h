#ifndef __LINUX_SEQLOCK_H
#define __LINUX_SEQLOCK_H
/*
 * Reader/writer consistent mechanism without starving writers. This type of
 * lock for data where the reader wants a consistent set of information
 * and is willing to retry if the information changes.  Readers never
 * block but they may have to retry if a writer is in
 * progress. Writers do not wait for readers. 
 *
 * This is not as cache friendly as brlock. Also, this will not work
 * for data that contains pointers, because any writer could
 * invalidate a pointer that a reader was following.
 *
 * Expected reader usage:
 * 	do {
 *	    seq = read_seqbegin(&foo);
 * 	...
 *      } while (read_seqretry(&foo, seq));
 *
 *
 * On non-SMP the spin locks disappear but the writer still needs
 * to increment the sequence variables because an interrupt routine could
 * change the state of the data.
 *
 * Based on x86_64 vsyscall gettimeofday 
 * by Keith Owens and Andrea Arcangeli
 */

 /*顺序锁*/

 /* 使用例子
  * 定义一个全局的seqlock变量demo_seqlock
    DEFINE_SEQLOCK(demo_seqlock);

    对于写入者的代码....
  * //实际写之前调用write_seqlock获取自旋锁,同时更新sequence的值
    write_seqlock(&demo_seqlock);
  * //获得自旋锁之后,调用do_write进行实际的写入操作
    do_write();
    //写入结束,调用write_sequnlock释放锁
    write_sequnlock(&demo_seqlock);


    对于读取者的代码....
    unsigned start;
    do{
    	//读操作前先得到sequence的值start,用以在读操作结束时判断是否发生数据更新
	//注意读操作无需获得锁
	start = read_seqbegin(&demo_seqlock);
	//调用do_read进行实际的读操作
	do_read();
    }while(read_seqretry(&demo_seqlock,start));//如果有数据更新,再重新读取
  *

  如果考虑到中断安全的问题,可以使用对应版本:
  write_seqlock_irq(lock)
  write_seqlock_irqsave(lock, flags);
  write_seqlock_bh(lock)

  write_sequnlock_irq(lock)
  write_sequnlock_irqrestore(lock, flags)
  write_sequnlock_bh(lock)

  read_seqbegin_irqsave(lock, flags)
  read_seqretry_irqrestore(lock, iv, flags)
  */

#include <linux/spinlock.h>
#include <linux/preempt.h>

/*顺序锁定义*/
typedef struct {
	unsigned sequence;	/*用来协调读取者与写入者的操作*/
	spinlock_t lock;	/*在多个写入者之间做互斥使用*/
} seqlock_t;

/*
 * These macros triggered gcc-3.x compile-time problems.  We think these are
 * OK now.  Be cautious.
 */
#define __SEQLOCK_UNLOCKED(lockname) \
		 { 0, __SPIN_LOCK_UNLOCKED(lockname) }

#define SEQLOCK_UNLOCKED \
		 __SEQLOCK_UNLOCKED(old_style_seqlock_init)

/*动态初始化顺序锁*/
#define seqlock_init(x)					\
	do {						\
		(x)->sequence = 0;			\
		spin_lock_init(&(x)->lock);		\
	} while (0)

/*静态定义一个互斥锁并初始化*/
#define DEFINE_SEQLOCK(x) \
		seqlock_t x = __SEQLOCK_UNLOCKED(x)

/* Lock out other writers and update the count.
 * Acts like a normal spin_lock/unlock.
 * Don't need preempt_disable() because that is in the spin_lock already.
 */
 /*写入者在seqlock上的上锁操作*/
static inline void write_seqlock(seqlock_t *sl)
{
	/*写入之前先获得seqlock上的自旋lock,在写入者之间必须保证互斥操作*/
	spin_lock(&sl->lock);
	++sl->sequence;
	smp_wmb();
}

 /*写入者在seqlock上的解锁操作*/
static inline void write_sequnlock(seqlock_t *sl)
{
	smp_wmb();
	/* 更新sequence告诉读取者有数据更新发生,所以
	 * 必须保证sequence的值在写入的前后发生变化
	 * 在此基础上sequence提供的另外一个信息是写入过程没有结束,通过最低位完成的
	 * 如果sequence&0为0表明写入过程已经结束,否则写入过程正值进行,在读取者的sequence
	   或看到这两种用途
	 */
	sl->sequence++;
	spin_unlock(&sl->lock);
}

/*在无法获得锁时不进入自选状态,返回0,成功返回1*/
static inline int write_tryseqlock(seqlock_t *sl)
{
	int ret = spin_trylock(&sl->lock);

	if (ret) {
		++sl->sequence;
		smp_wmb();
	}
	return ret;
}

/* Start of read calculation -- fetch last complete writer token */
static __always_inline unsigned read_seqbegin(const seqlock_t *sl)
{
	unsigned ret;

repeat:
	ret = ACCESS_ONCE(sl->sequence);
	if (unlikely(ret & 1)) {
		cpu_relax();
		goto repeat;
	}
	smp_rmb();

	return ret;
}

/*
 * Test if reader processed invalid data.
 *
 * If sequence value changed then writer changed data while in section.
 */
 /* 判断是否有数据更新
  * @start:是读取者在读取操作之前调用read_seqbegin获得的初始值,
  	如果本次读取无效,那么read_seqretry返回1,否则返回0
  */
static __always_inline int read_seqretry(const seqlock_t *sl, unsigned start)
{
	smp_rmb();

	return (sl->sequence != start);
}


/*
 * Version using sequence counter only.
 * This can be used when code has its own mutex protecting the
 * updating starting before the write_seqcountbeqin() and ending
 * after the write_seqcount_end().
 */

typedef struct seqcount {
	unsigned sequence;
} seqcount_t;

#define SEQCNT_ZERO { 0 }
#define seqcount_init(x)	do { *(x) = (seqcount_t) SEQCNT_ZERO; } while (0)

/* Start of read using pointer to a sequence counter only.  */
static inline unsigned read_seqcount_begin(const seqcount_t *s)
{
	unsigned ret;

repeat:
	ret = s->sequence;
	smp_rmb();
	if (unlikely(ret & 1)) {
		cpu_relax();
		goto repeat;
	}
	return ret;
}

/*
 * Test if reader processed invalid data because sequence number has changed.
 */
static inline int read_seqcount_retry(const seqcount_t *s, unsigned start)
{
	smp_rmb();

	return s->sequence != start;
}


/*
 * Sequence counter only version assumes that callers are using their
 * own mutexing.
 */
static inline void write_seqcount_begin(seqcount_t *s)
{
	s->sequence++;
	smp_wmb();
}

static inline void write_seqcount_end(seqcount_t *s)
{
	smp_wmb();
	s->sequence++;
}

/*
 * Possible sw/hw IRQ protected versions of the interfaces.
 */
#define write_seqlock_irqsave(lock, flags)				\
	do { local_irq_save(flags); write_seqlock(lock); } while (0)
#define write_seqlock_irq(lock)						\
	do { local_irq_disable();   write_seqlock(lock); } while (0)
#define write_seqlock_bh(lock)						\
        do { local_bh_disable();    write_seqlock(lock); } while (0)

#define write_sequnlock_irqrestore(lock, flags)				\
	do { write_sequnlock(lock); local_irq_restore(flags); } while(0)
#define write_sequnlock_irq(lock)					\
	do { write_sequnlock(lock); local_irq_enable(); } while(0)
#define write_sequnlock_bh(lock)					\
	do { write_sequnlock(lock); local_bh_enable(); } while(0)

#define read_seqbegin_irqsave(lock, flags)				\
	({ local_irq_save(flags);   read_seqbegin(lock); })

#define read_seqretry_irqrestore(lock, iv, flags)			\
	({								\
		int ret = read_seqretry(lock, iv);			\
		local_irq_restore(flags);				\
		ret;							\
	})

#endif /* __LINUX_SEQLOCK_H */
