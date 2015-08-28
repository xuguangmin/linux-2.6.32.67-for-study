#ifndef __LINUX_COMPLETION_H
#define __LINUX_COMPLETION_H

/*
 * (C) Copyright 2001 Linus Torvalds
 *
 * Atomic wait-for-completion handler data structures.
 * See kernel/sched.c for details.
 */

#include <linux/wait.h>

/*
 * 完成接口，是一个同步机制，用来在多个执行路径间作同步使用，
 * 即协调多个执行路径的执行顺序
 */

/**
 * struct completion - structure used to maintain state for a "completion"
 *
 * This is the opaque structure used to maintain the state for a "completion".
 * Completions currently use a FIFO to queue threads that have to wait for
 * the "completion" event.
 *
 * See also:  complete(), wait_for_completion() (and friends _timeout,
 * _interruptible, _interruptible_timeout, and _killable), init_completion(),
 * and macros DECLARE_COMPLETION(), DECLARE_COMPLETION_ONSTACK(), and
 * INIT_COMPLETION().
 */
 /*完成借口*/
struct completion {
	unsigned int done;	/*表示当前completion的状态*/
	wait_queue_head_t wait;	/* wait是一等待队列,用来管理当前等待在
				 * 该completion上的所有进程*/
};

/*等待队列*/
#define COMPLETION_INITIALIZER(work) \
	{ 0, __WAIT_QUEUE_HEAD_INITIALIZER((work).wait) }

#define COMPLETION_INITIALIZER_ONSTACK(work) \
	({ init_completion(&work); work; })

/**
 * DECLARE_COMPLETION: - declare and initialize a completion structure
 * @work:  identifier for the completion structure
 *
 * This macro declares and initializes a completion structure. Generally used
 * for static declarations. You should use the _ONSTACK variant for automatic
 * variables.
 */
 /*静态定义一个struct completion变量并初始化*/
#define DECLARE_COMPLETION(work) \
	struct completion work = COMPLETION_INITIALIZER(work)

/*
 * Lockdep needs to run a non-constant initializer for on-stack
 * completions - so we use the _ONSTACK() variant for those that
 * are on the kernel stack:
 */
/**
 * DECLARE_COMPLETION_ONSTACK: - declare and initialize a completion structure
 * @work:  identifier for the completion structure
 *
 * This macro declares and initializes a completion structure on the kernel
 * stack.
 */
#ifdef CONFIG_LOCKDEP
# define DECLARE_COMPLETION_ONSTACK(work) \
	struct completion work = COMPLETION_INITIALIZER_ONSTACK(work)
#else
# define DECLARE_COMPLETION_ONSTACK(work) DECLARE_COMPLETION(work)
#endif

/**
 * init_completion: - Initialize a dynamically allocated completion
 * @x:  completion structure that is to be initialized
 *
 * This inline function will initialize a dynamically created completion
 * structure.
 */
 /*动态初始化一个completion变量*/
static inline void init_completion(struct completion *x)
{
	x->done = 0;
	init_waitqueue_head(&x->wait);
}

/* 完成接口completion对执行路径间的同步可以通过等待者与完成者模型来表述，
 * 对于等待者的行为,内核定义了wait_for_completion函数*/
extern void wait_for_completion(struct completion *);
extern int wait_for_completion_interruptible(struct completion *x);
extern int wait_for_completion_killable(struct completion *x);
extern unsigned long wait_for_completion_timeout(struct completion *x,
						   unsigned long timeout);
extern unsigned long wait_for_completion_interruptible_timeout(
			struct completion *x, unsigned long timeout);
extern bool try_wait_for_completion(struct completion *x);
extern bool completion_done(struct completion *x);

extern void complete(struct completion *);
extern void complete_all(struct completion *);

/**
 * INIT_COMPLETION: - reinitialize a completion structure
 * @x:  completion structure to be reinitialized
 *
 * This macro should be used to reinitialize a completion structure so it can
 * be reused. This is especially important after complete_all() is used.
 */
 /*重新初始化一个使用过的struct completion变量*/
#define INIT_COMPLETION(x)	((x).done = 0)


#endif
