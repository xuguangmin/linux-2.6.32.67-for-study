/*
 * Copyright (c) 2008 Intel Corporation
 * Author: Matthew Wilcox <willy@linux.intel.com>
 *
 * Distributed under the terms of the GNU GPL, version 2
 *
 * This file implements counting semaphores.
 * A counting semaphore may be acquired 'n' times before sleeping.
 * See mutex.c for single-acquisition sleeping locks which enforce
 * rules which allow code to be debugged more easily.
 */

/*
 * Some notes on the implementation:
 *
 * The spinlock controls access to the other members of the semaphore.
 * down_trylock() and up() can be called from interrupt context, so we
 * have to disable interrupts when taking the lock.  It turns out various
 * parts of the kernel expect to be able to use down() on a semaphore in
 * interrupt context when they know it will succeed, so we have to use
 * irqsave variants for down(), down_interruptible() and down_killable()
 * too.
 *
 * The ->count variable represents how many more tasks can acquire this
 * semaphore.  If it's zero, there may be tasks waiting on the wait_list.
 */

#include <linux/compiler.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/semaphore.h>
#include <linux/spinlock.h>
#include <linux/ftrace.h>

static noinline void __down(struct semaphore *sem);
static noinline int __down_interruptible(struct semaphore *sem);
static noinline int __down_killable(struct semaphore *sem);
static noinline int __down_timeout(struct semaphore *sem, long jiffies);
static noinline void __up(struct semaphore *sem);

/**
 * down - acquire the semaphore
 * @sem: the semaphore to be acquired
 *
 * Acquires the semaphore.  If no more tasks are allowed to acquire the
 * semaphore, calling this function will put the task to sleep until the
 * semaphore is released.
 *
 * Use of this function is deprecated, please use down_interruptible() or
 * down_killable() instead.
 */
/*与down_interruptible相比，down是不可中断的*/
void down(struct semaphore *sem)
{
	unsigned long flags;

	spin_lock_irqsave(&sem->lock, flags);
	if (likely(sem->count > 0))
		sem->count--;
	else
		__down(sem);
	spin_unlock_irqrestore(&sem->lock, flags);
}
EXPORT_SYMBOL(down);

/**
 * down_interruptible - acquire the semaphore unless interrupted
 * @sem: the semaphore to be acquired
 *
 * Attempts to acquire the semaphore.  If no more tasks are allowed to
 * acquire the semaphore, calling this function will put the task to sleep.
 * If the sleep is interrupted by a signal, this function will return -EINTR.
 * If the semaphore is successfully acquired, this function returns 0.
 */
 /* 对该函数的调用坚持检查其返回值,以确定函数是已经获得了信号量
  * 还是因为操作中断需要特殊处理;通常驱动对返回的非０做的工作是返回-ERESTARTSYS*/
  /*返回０表示调用者获得了信号量*/
int down_interruptible(struct semaphore *sem)
{
	unsigned long flags;
	int result = 0;

	/*保证操作的原子性,阻止多个进程对sem->count同时操作*/
	spin_lock_irqsave(&sem->lock, flags);
	/* 判断sem->count,如果大于０表明当前进程可以获得信号量，
		就将count值减1,然后退出;*/
	if (likely(sem->count > 0))
		sem->count--;
	else
	 	/*如果不大于０，表明当前进程无法获得信号量，
		此时调用__down_interruptible*/
		result = __down_interruptible(sem);
	spin_unlock_irqrestore(&sem->lock, flags);

	return result;
}
EXPORT_SYMBOL(down_interruptible);

/**
 * down_killable - acquire the semaphore unless killed
 * @sem: the semaphore to be acquired
 *
 * Attempts to acquire the semaphore.  If no more tasks are allowed to
 * acquire the semaphore, calling this function will put the task to sleep.
 * If the sleep is interrupted by a fatal signal, this function will return
 * -EINTR.  If the semaphore is successfully acquired, this function returns
 * 0.
 */
/*睡眠的进程可以因收到一些致命性信号被唤醒而导致获取信号量的操作失败，极少使用*/
int down_killable(struct semaphore *sem)
{
	unsigned long flags;
	int result = 0;

	spin_lock_irqsave(&sem->lock, flags);
	if (likely(sem->count > 0))
		sem->count--;
	else
		result = __down_killable(sem);
	spin_unlock_irqrestore(&sem->lock, flags);

	return result;
}
EXPORT_SYMBOL(down_killable);

/**
 * down_trylock - try to acquire the semaphore, without waiting
 * @sem: the semaphore to be acquired
 *
 * Try to acquire the semaphore atomically.  Returns 0 if the mutex has
 * been acquired successfully or 1 if it it cannot be acquired.
 *
 * NOTE: This return value is inverted from both spin_trylock and
 * mutex_trylock!  Be careful about this when converting code.
 *
 * Unlike mutex_trylock, this function can be used from interrupt context,
 * and the semaphore can be released by any task or interrupt.
 */

/*进程试图获得信号量，若无法获得直接返回１而不进入睡眠状态；
返回０意味着函数的调用者已经获得了信号量*/
int down_trylock(struct semaphore *sem)
{
	unsigned long flags;
	int count;

	spin_lock_irqsave(&sem->lock, flags);
	count = sem->count - 1;
	if (likely(count >= 0))
		sem->count = count;
	spin_unlock_irqrestore(&sem->lock, flags);

	return (count < 0);
}
EXPORT_SYMBOL(down_trylock);

/**
 * down_timeout - acquire the semaphore within a specified time
 * @sem: the semaphore to be acquired
 * @jiffies: how long to wait before failing
 *
 * Attempts to acquire the semaphore.  If no more tasks are allowed to
 * acquire the semaphore, calling this function will put the task to sleep.
 * If the semaphore is not released within the specified number of jiffies,
 * this function returns -ETIME.  It returns 0 if the semaphore was acquired.
 */
/* 函数在无法获得信号量的情况下将进入睡眠状态，但是处于这种睡眠状态有实际限制
 * 如果jiffies指明的实际到期时函数依然无法获得信号量，则将返回错误码-ETIME，
 * 在到期前进程的睡眠状态为TASK_UNINTERRUPTIBLE，成功获得信号量返回0.*/
int down_timeout(struct semaphore *sem, long jiffies)
{
	unsigned long flags;
	int result = 0;

	spin_lock_irqsave(&sem->lock, flags);
	if (likely(sem->count > 0))
		sem->count--;
	else
		result = __down_timeout(sem, jiffies);
	spin_unlock_irqrestore(&sem->lock, flags);

	return result;
}
EXPORT_SYMBOL(down_timeout);

/**
 * up - release the semaphore
 * @sem: the semaphore to release
 *
 * Release the semaphore.  Unlike mutexes, up() may be called from any
 * context and even by tasks which have never called down().
 */
 /*信号量up操作*/
void up(struct semaphore *sem)
{
	unsigned long flags;

	spin_lock_irqsave(&sem->lock, flags);
	/*判断sem的wait_list队列为空,则没有进程等待信号量*/
	if (likely(list_empty(&sem->wait_list)))
		sem->count++;
	else
		/*表明有进程等待信号量，将其唤醒*/
		__up(sem);
	spin_unlock_irqrestore(&sem->lock, flags);
}
EXPORT_SYMBOL(up);

/* Functions for the contended case */
/*将这个结构加入到sem信号量的等待列表sem->wait_list中*/
struct semaphore_waiter {
	struct list_head list;
	/*将当前等待的进程填入task中*/
	struct task_struct *task;
	int up;
};

/*
 * Because this function is inlined, the 'state' parameter will be
 * constant, and thus optimised away by the compiler.  Likewise the
 * 'timeout' parameter for the cases without timeouts.
 */
static inline int __sched __down_common(struct semaphore *sem, long state,
								long timeout)
{
	struct task_struct *task = current;
	struct semaphore_waiter waiter;

 	/*函数功能是通过对一个struct semaphore_waiter的使用，
	 把当前进程放到信号量sem的成员变量sem的成员变量wait_list所管理的队列中*/
	list_add_tail(&waiter.list, &sem->wait_list);
	waiter.task = task;
	waiter.up = 0;

	/*在一个for循环中把当前进程的状态设置为TASK_INTERRUPTIBLE*/
	for (;;) {
		if (signal_pending_state(state, task))
			goto interrupted;
		if (timeout <= 0)
			goto timed_out;
		__set_task_state(task, state);
		spin_unlock_irq(&sem->lock);
		/*使当前进程进入睡眠状态,函数将停留在schedule_timeout调用上*/
		timeout = schedule_timeout(timeout);
		spin_lock_irq(&sem->lock);
		/*如果waiter.up不为０，说明进程在信号量sem的wait_list队列中
		被该信号量的UP操作所唤醒，进程可以获得信号量, 返回０*/
		if (waiter.up)
			return 0;
	}

/*进程被超市引起的唤醒*/
 timed_out:
	list_del(&waiter.list);
	return -ETIME;

/*进程被中断引起的唤醒*/
 interrupted:
	list_del(&waiter.list);
	return -EINTR;
}

static noinline void __sched __down(struct semaphore *sem)
{
	__down_common(sem, TASK_UNINTERRUPTIBLE, MAX_SCHEDULE_TIMEOUT);
}

/*无法获得信号量时的操作*/
static noinline int __sched __down_interruptible(struct semaphore *sem)
{
	/**/
	return __down_common(sem, TASK_INTERRUPTIBLE, MAX_SCHEDULE_TIMEOUT);
}

static noinline int __sched __down_killable(struct semaphore *sem)
{
	return __down_common(sem, TASK_KILLABLE, MAX_SCHEDULE_TIMEOUT);
}

static noinline int __sched __down_timeout(struct semaphore *sem, long jiffies)
{
	return __down_common(sem, TASK_UNINTERRUPTIBLE, jiffies);
}

/*信号量的wait_list等待队列不为空，唤醒进程*/
static noinline void __sched __up(struct semaphore *sem)
{
	/*取得sem->wait_list链表上第一个waiter节点*/
	struct semaphore_waiter *waiter = list_first_entry(&sem->wait_list,
						struct semaphore_waiter, list);
	/*将取得的节点从链表上删除*/
	list_del(&waiter->list);
	waiter->up = 1;
	/*唤醒进程*/
	wake_up_process(waiter->task);
}
