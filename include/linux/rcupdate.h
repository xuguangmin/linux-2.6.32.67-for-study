/*
 * Read-Copy Update mechanism for mutual exclusion
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Copyright IBM Corporation, 2001
 *
 * Author: Dipankar Sarma <dipankar@in.ibm.com>
 *
 * Based on the original work by Paul McKenney <paulmck@us.ibm.com>
 * and inputs from Rusty Russell, Andrea Arcangeli and Andi Kleen.
 * Papers:
 * http://www.rdrop.com/users/paulmck/paper/rclockpdcsproof.pdf
 * http://lse.sourceforge.net/locking/rclock_OLS.2001.05.01c.sc.pdf (OLS2001)
 *
 * For detailed explanation of Read-Copy Update mechanism see -
 *		http://lse.sourceforge.net/locking/rcupdate.html
 *
 */

#ifndef __LINUX_RCUPDATE_H
#define __LINUX_RCUPDATE_H

#include <linux/cache.h>
#include <linux/spinlock.h>
#include <linux/threads.h>
#include <linux/cpumask.h>
#include <linux/seqlock.h>
#include <linux/lockdep.h>
#include <linux/completion.h>

/*RCU机制*/
/*
 * RCU典型用法范例:

   //假设struct shared_data时一个在读取者和写入者之间共享的受保护数据
   struct shared_data{
   	int a;
	int b;
	struct rcu_head rcu;
   };
   //读取者侧的代码,读取者调用rcu_read_lock和rcu_read_unlock构建它的读取临界区,
   	所有的指向被保护资源指针的引用都应该在临界区中出现,而且临界区中的代码不能睡眠
   static void demo_reader(struct shared_data *ptr)
   {
   	struct shared_data *p = NULL;
	rcu_read_lock();
	//调用rcu_dereference获得ptr的指针
	p = rcu_dereference(ptr);
	if(p)
		do_something_withp(p);
	rcu_read_unlock();
   }


   //写入者侧的代码
   //写入者提供的回调函数,用于释放老指针
   static void demo_del_oldptr(struct rcu_head *rh)
   {
   	struct shared_data *p = container_of(rh, struct shared_data, rcu)
	kfree(p);
   }
   static void demo_writer(struct shared_data *ptr)
   {
   	struct shared_data *new_ptr = kmalloc(...);
	...
	new_ptr->a = 10;
	new_ptr->b = 10;
	//用新指针更新老指针
	rcu_assign_pointer(ptr, new_ptr);
	//调用call_rcu让内核在确保所有对老指针ptr的引用都结束后回调demo_del_oldptr释放老指针
	call_rcu(ptr->rcu, demo_del_oldptr);
   }
 */



/**
 * struct rcu_head - callback structure for use with RCU
 * @next: next update requests in a list
 * @func: actual update function to call after the grace period.
 */
struct rcu_head {
	struct rcu_head *next;
	void (*func)(struct rcu_head *head);
};

/* Exported common interfaces */
#ifdef CONFIG_TREE_PREEMPT_RCU
/*类似call_rcu的功能,但函数可能会阻塞,不能在中断上下文使用, 应使用call_rcu注册进内核*/
extern void synchronize_rcu(void);
#else /* #ifdef CONFIG_TREE_PREEMPT_RCU */
#define synchronize_rcu synchronize_sched
#endif /* #else #ifdef CONFIG_TREE_PREEMPT_RCU */
extern void synchronize_rcu_bh(void);
extern void synchronize_sched(void);
extern void rcu_barrier(void);
extern void rcu_barrier_bh(void);
extern void rcu_barrier_sched(void);
extern void synchronize_sched_expedited(void);
extern int sched_expedited_torture_stats(char *page);

/* Internal to kernel */
extern void rcu_init(void);
extern void rcu_scheduler_starting(void);
extern int rcu_needs_cpu(int cpu);
extern int rcu_scheduler_active;

#if defined(CONFIG_TREE_RCU) || defined(CONFIG_TREE_PREEMPT_RCU)
#include <linux/rcutree.h>
#else
#error "Unknown RCU implementation specified to kernel configuration"
#endif

#define RCU_HEAD_INIT	{ .next = NULL, .func = NULL }
#define RCU_HEAD(head) struct rcu_head head = RCU_HEAD_INIT
#define INIT_RCU_HEAD(ptr) do { \
       (ptr)->next = NULL; (ptr)->func = NULL; \
} while (0)

#ifdef CONFIG_DEBUG_LOCK_ALLOC
extern struct lockdep_map rcu_lock_map;
# define rcu_read_acquire()	\
			lock_acquire(&rcu_lock_map, 0, 0, 2, 1, NULL, _THIS_IP_)
# define rcu_read_release()	lock_release(&rcu_lock_map, 1, _THIS_IP_)
#else
# define rcu_read_acquire()	do { } while (0)
# define rcu_read_release()	do { } while (0)
#endif

/**
 * rcu_read_lock - mark the beginning of an RCU read-side critical section.
 *
 * When synchronize_rcu() is invoked on one CPU while other CPUs
 * are within RCU read-side critical sections, then the
 * synchronize_rcu() is guaranteed to block until after all the other
 * CPUs exit their critical sections.  Similarly, if call_rcu() is invoked
 * on one CPU while other CPUs are within RCU read-side critical
 * sections, invocation of the corresponding RCU callback is deferred
 * until after the all the other CPUs exit their critical sections.
 *
 * Note, however, that RCU callbacks are permitted to run concurrently
 * with RCU read-side critical sections.  One way that this can happen
 * is via the following sequence of events: (1) CPU 0 enters an RCU
 * read-side critical section, (2) CPU 1 invokes call_rcu() to register
 * an RCU callback, (3) CPU 0 exits the RCU read-side critical section,
 * (4) CPU 2 enters a RCU read-side critical section, (5) the RCU
 * callback is invoked.  This is legal, because the RCU read-side critical
 * section that was running concurrently with the call_rcu() (and which
 * therefore might be referencing something that the corresponding RCU
 * callback would free up) has completed before the corresponding
 * RCU callback is invoked.
 *
 * RCU read-side critical sections may be nested.  Any deferred actions
 * will be deferred until the outermost RCU read-side critical section
 * completes.
 *
 * It is illegal to block while in an RCU read-side critical section.
 */
 /*RCU 临街区中不会发生进程切换*/
static inline void rcu_read_lock(void)
{
	/*禁止抢占*/
	__rcu_read_lock();
	__acquire(RCU);
	rcu_read_acquire();
}

/*
 * So where is rcu_write_lock()?  It does not exist, as there is no
 * way for writers to lock out RCU readers.  This is a feature, not
 * a bug -- this property is what provides RCU's performance benefits.
 * Of course, writers must coordinate with each other.  The normal
 * spinlock primitives work well for this, but any other technique may be
 * used as well.  RCU does not care how the writers keep out of each
 * others' way, as long as they do so.
 */

/**
 * rcu_read_unlock - marks the end of an RCU read-side critical section.
 *
 * See rcu_read_lock() for more information.
 */
 /*读者在读取由RCU保护的共享数据时使用该函数标记它进入读端临界区。*/
static inline void rcu_read_unlock(void)
{
	rcu_read_release();
	__release(RCU);
	__rcu_read_unlock();
}

/**
 * rcu_read_lock_bh - mark the beginning of a softirq-only RCU critical section
 *
 * This is equivalent of rcu_read_lock(), but to be used when updates
 * are being done using call_rcu_bh(). Since call_rcu_bh() callbacks
 * consider completion of a softirq handler to be a quiescent state,
 * a process in RCU read-side critical section must be protected by
 * disabling softirqs. Read-side critical sections in interrupt context
 * can use just rcu_read_lock().
 */
 /* 该函数与rcu_read_lock配对使用，用以标记读者退出读端临界区。
  * 夹在这两个函数之间的代码区称为"读端临界区*/
static inline void rcu_read_lock_bh(void)
{
	__rcu_read_lock_bh();
	__acquire(RCU_BH);
	rcu_read_acquire();
}

/*
 * rcu_read_unlock_bh - marks the end of a softirq-only RCU critical section
 *
 * See rcu_read_lock_bh() for more information.
 */
static inline void rcu_read_unlock_bh(void)
{
	rcu_read_release();
	__release(RCU_BH);
	__rcu_read_unlock_bh();
}

/**
 * rcu_read_lock_sched - mark the beginning of a RCU-classic critical section
 *
 * Should be used with either
 * - synchronize_sched()
 * or
 * - call_rcu_sched() and rcu_barrier_sched()
 * on the write-side to insure proper synchronization.
 */
static inline void rcu_read_lock_sched(void)
{
	preempt_disable();
	__acquire(RCU_SCHED);
	rcu_read_acquire();
}

/* Used by lockdep and tracing: cannot be traced, cannot call lockdep. */
static inline notrace void rcu_read_lock_sched_notrace(void)
{
	preempt_disable_notrace();
	__acquire(RCU_SCHED);
}

/*
 * rcu_read_unlock_sched - marks the end of a RCU-classic critical section
 *
 * See rcu_read_lock_sched for more information.
 */
static inline void rcu_read_unlock_sched(void)
{
	rcu_read_release();
	__release(RCU_SCHED);
	preempt_enable();
}

/* Used by lockdep and tracing: cannot be traced, cannot call lockdep. */
static inline notrace void rcu_read_unlock_sched_notrace(void)
{
	__release(RCU_SCHED);
	preempt_enable_notrace();
}


/**
 * rcu_dereference - fetch an RCU-protected pointer in an
 * RCU read-side critical section.  This pointer may later
 * be safely dereferenced.
 *
 * Inserts memory barriers on architectures that require them
 * (currently only the Alpha), and, more importantly, documents
 * exactly which pointers are protected by RCU.
 */
/*获得p的指针*/
#define rcu_dereference(p)     ({ \
				typeof(p) _________p1 = ACCESS_ONCE(p); \
				smp_read_barrier_depends(); \
				(_________p1); \
				})

/**
 * rcu_assign_pointer - assign (publicize) a pointer to a newly
 * initialized structure that will be dereferenced by RCU read-side
 * critical sections.  Returns the value assigned.
 *
 * Inserts memory barriers on architectures that require them
 * (pretty much all of them other than x86), and also prevents
 * the compiler from reordering the code that initializes the
 * structure after the pointer assignment.  More importantly, this
 * call documents which pointers will be dereferenced by RCU read-side
 * code.
 */

/*更新老指针*/
#define rcu_assign_pointer(p, v) \
	({ \
		if (!__builtin_constant_p(v) || \
		    ((v) != NULL)) \
			smp_wmb(); \
		(p) = (v); \
	})

/* Infrastructure to implement the synchronize_() primitives. */

struct rcu_synchronize {
	struct rcu_head head;
	struct completion completion;
};

extern void wakeme_after_rcu(struct rcu_head  *head);

/**
 * call_rcu - Queue an RCU callback for invocation after a grace period.
 * @head: structure to be used for queueing the RCU updates.
 * @func: actual update function to be invoked after the grace period
 *
 * The update function will be invoked some time after a full grace
 * period elapses, in other words after all currently executing RCU
 * read-side critical sections have completed.  RCU read-side critical
 * sections are delimited by rcu_read_lock() and rcu_read_unlock(),
 * and may be nested.
 */
 /* RCU的写入者负责在替换掉老指针之后调用call_rcu向内核注册一回调函数,
  * 回调函数负责实现释放老指针指向的内存空间,call_rcu中的参数func就是指向该回调函数的指针.
  * head是内核在调用func时传递到func中的参数,实际使用中会把rcu_head内嵌到共享数据
  * 所在的结构体中,这样在回调函数中可以通过传进来的struct rcu_head指针,使用container_of
  * 获得指向旧的共享数据区的指针,然后调用kfree释放旧的数据区*/
extern void call_rcu(struct rcu_head *head,
			      void (*func)(struct rcu_head *head));

/**
 * call_rcu_bh - Queue an RCU for invocation after a quicker grace period.
 * @head: structure to be used for queueing the RCU updates.
 * @func: actual update function to be invoked after the grace period
 *
 * The update function will be invoked some time after a full grace
 * period elapses, in other words after all currently executing RCU
 * read-side critical sections have completed. call_rcu_bh() assumes
 * that the read-side critical sections end on completion of a softirq
 * handler. This means that read-side critical sections in process
 * context must not be interrupted by softirqs. This interface is to be
 * used when most of the read-side critical sections are in softirq context.
 * RCU read-side critical sections are delimited by :
 *  - rcu_read_lock() and  rcu_read_unlock(), if in interrupt context.
 *  OR
 *  - rcu_read_lock_bh() and rcu_read_unlock_bh(), if in process context.
 *  These may be nested.
 */
extern void call_rcu_bh(struct rcu_head *head,
			void (*func)(struct rcu_head *head));

#endif /* __LINUX_RCUPDATE_H */
