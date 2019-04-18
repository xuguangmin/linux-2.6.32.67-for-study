/*
 * Copyright (c) 2008 Intel Corporation
 * Author: Matthew Wilcox <willy@linux.intel.com>
 *
 * Distributed under the terms of the GNU GPL, version 2
 *
 * Please see kernel/semaphore.c for documentation of these functions
 */
#ifndef __LINUX_SEMAPHORE_H
#define __LINUX_SEMAPHORE_H

#include <linux/list.h>
#include <linux/spinlock.h>
/*************     内核信号量   ********************/
/* 内核提供了两种信号量,此处为内核信号量
  * 内核信号量:由内核控制路径使用
  *System V IPC信号量:由用户态进程使用
  */

/* Please don't access any members of this structure directly */
/*
 * 其中lock是个自旋锁变量,用于实现对count的原子操作
 * count用于表示通过该信号量允许进入临界区的执行路径的个数
 * wait_list用于管理所有在该信号量上睡眠的进程，
          无法获得该信号量的进程将进入睡眠状态
 */
struct semaphore {
	spinlock_t		lock;
	/* > 0:信号量是空闲的
	  * =0: 信号量是忙的，但没有进程等待这个被保护的资源
	  * <0:资源是不可用的，并至少有一个进程等待资源
	  */
	unsigned int		count;
	struct list_head	wait_list;//存放等待队列链表的地址
};

/*完成信号量的初始化*/
#define __SEMAPHORE_INITIALIZER(name, n)				\
{									\
	.lock		= __SPIN_LOCK_UNLOCKED((name).lock),		\
	.count		= n,						\
	.wait_list	= LIST_HEAD_INIT((name).wait_list),		\
}

/* count值为1,实现互斥机制,任意时刻只允许一个进程进入临界区
  * 静态分配信号量并初始化
  */
#define DECLARE_MUTEX(name)	\
	struct semaphore name = __SEMAPHORE_INITIALIZER(name, 1)

/*初始化信号量*/
static inline void sema_init(struct semaphore *sem, int val)
{
	static struct lock_class_key __key;
	/*初始化主要函数*/
	*sem = (struct semaphore) __SEMAPHORE_INITIALIZER(*sem, val);
	lockdep_init_map(&sem->lock.dep_map, "semaphore->lock", &__key, 0);
}
//初始化信号量
#define init_MUTEX(sem)		sema_init(sem, 1)
#define init_MUTEX_LOCKED(sem)	sema_init(sem, 0)

/*与down_interruptible相比，down是不可中断的*/
extern void down(struct semaphore *sem);
/*可中断的*/
extern int __must_check down_interruptible(struct semaphore *sem);
/*睡眠的进程可以因收到一些致命性信号被唤醒而导致获取信号量的操作失败，极少使用*/
extern int __must_check down_killable(struct semaphore *sem);
/*进程试图获得信号量，若无法获得直接返回１而不进入睡眠状态；
返回０意味着函数的调用者已经获得了信号量*/
extern int __must_check down_trylock(struct semaphore *sem);
/* 函数在无法获得信号量的情况下将进入睡眠状态，但是处于这种睡眠状态有实际限制
 * 如果jiffies指明的实际到期时函数依然无法获得信号量，则将返回错误码-ETIME，
 * 在到期前进程的睡眠状态为TASK_UNINTERRUPTIBLE，成功获得信号量返回0.*/
extern int __must_check down_timeout(struct semaphore *sem, long jiffies);
/*只有一个UP函数*/
extern void up(struct semaphore *sem);

#endif /* __LINUX_SEMAPHORE_H */
