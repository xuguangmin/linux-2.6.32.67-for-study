/*
 * workqueue.h --- work queue handling for Linux.
 */


/**
 * 使用了工作队列执行延迟操作的代码范例:
 
 //定义全局性的struct workqueue_struct(工作队列管理结构)指针demo_dev_wq
 static struct workqueue_struct * demo_dev_wq;
 
 //设备特定的数据结构,实际使用中大部分struct work_struct结构都内嵌在这个数据结构中
 struct demo_device {
 	...
	struct work_struct work;
	...
 }
 static struct demo_device *demo_dev;

 //定义延迟操作函数
 void demo_work_func(struct work_struct *work)
 {
 	...
 }

 //驱动程序模块初始化代码调用creat_singlethread_workqueue创建工作队列
 static int __init demo_dev_init(void)
 {
 	...
	demo_dev = kzalloc(sizeof(*demo_dev), GFP_KERNEL);
	demo_dev_wq = create_singlethread_workqueue("demo_dev_workqueue");
	INIT_WORK(&demo_dev->work, demo_work_func);
	...
 }

 //模块退出函数
 static void demo_dev_exit(void)
 {
 	...
	flush_workqueue(demo_dev_wq);
	destroy_workqueue(demo_dev_wq);
	...
 }

 //中断处理函数
 irqreturn_t demo_isr(iint irq, void *dev_id)
 {
 	...
	queue_work(demo_dev_wq, &demo_dev->work);
	...
 }

 */

#ifndef _LINUX_WORKQUEUE_H
#define _LINUX_WORKQUEUE_H

#include <linux/timer.h>
#include <linux/linkage.h>
#include <linux/bitops.h>
#include <linux/lockdep.h>
#include <asm/atomic.h>

/* 工作队列管理结构*/
struct workqueue_struct;

struct work_struct;
typedef void (*work_func_t)(struct work_struct *work);

/*
 * The first word is the work queue pointer and the flags rolled into
 * one
 */
#define work_data_bits(work) ((unsigned long *)(&(work)->data))

/*工作队列*/
struct work_struct {
	atomic_long_t data;	/* 驱动程序可以利用data来将设备驱动程序使用
				 * 的某些指针传递给延迟函数*/
#define WORK_STRUCT_PENDING 0		/* T if work item pending execution */
#define WORK_STRUCT_FLAG_MASK (3UL)
#define WORK_STRUCT_WQ_DATA_MASK (~WORK_STRUCT_FLAG_MASK)
	struct list_head entry;	/* 双向链表对象,用来将提交的等待处理的工作节点
				 * 形成链表*/
	work_func_t func;	/*工作节点的延迟函数,用来完成实际的延迟操作*/
#ifdef CONFIG_LOCKDEP
	struct lockdep_map lockdep_map;
#endif
};

#define WORK_DATA_INIT()	ATOMIC_LONG_INIT(0)

/**
 * 使用queue_delayed_work时用到此结构体,实现延迟提交功能
 */
struct delayed_work {
	struct work_struct work;
	struct timer_list timer;	/*实现时间上的延迟操作*/
};

static inline struct delayed_work *to_delayed_work(struct work_struct *work)
{
	return container_of(work, struct delayed_work, work);
}

struct execute_work {
	struct work_struct work;
};

#ifdef CONFIG_LOCKDEP
/*
 * NB: because we have to copy the lockdep_map, setting _key
 * here is required, otherwise it could get initialised to the
 * copy of the lockdep_map!
 */
#define __WORK_INIT_LOCKDEP_MAP(n, k) \
	.lockdep_map = STATIC_LOCKDEP_MAP_INIT(n, k),
#else
#define __WORK_INIT_LOCKDEP_MAP(n, k)
#endif

#define __WORK_INITIALIZER(n, f) {				\
	.data = WORK_DATA_INIT(),				\
	.entry	= { &(n).entry, &(n).entry },			\
	.func = (f),						\
	__WORK_INIT_LOCKDEP_MAP(#n, &(n))			\
	}

#define __DELAYED_WORK_INITIALIZER(n, f) {			\
	.work = __WORK_INITIALIZER((n).work, (f)),		\
	.timer = TIMER_INITIALIZER(NULL, 0, 0),			\
	}

/*可以让驱动程序静态定义一个struct work_struct对象同时初始化*/
#define DECLARE_WORK(n, f)					\
	struct work_struct n = __WORK_INITIALIZER(n, f)

#define DECLARE_DELAYED_WORK(n, f)				\
	struct delayed_work n = __DELAYED_WORK_INITIALIZER(n, f)

/*
 * initialize a work item's function pointer
 */
/*
 * 初始化一个工作队列节点,只是重新设置struct work_struct中的func指针
 */
#define PREPARE_WORK(_work, _func)				\
	do {							\
		(_work)->func = (_func);			\
	} while (0)

#define PREPARE_DELAYED_WORK(_work, _func)			\
	PREPARE_WORK(&(_work)->work, (_func))

/*
 * initialize all of a work item in one go
 *
 * NOTE! No point in using "atomic_long_set()": using a direct
 * assignment of the work data initializer allows the compiler
 * to generate better code.
 */
#ifdef CONFIG_LOCKDEP
/* 初始化一个工作队列节点,INIT_WORK初始化struct work_struct中的每个成员*/
#define INIT_WORK(_work, _func)						\
	do {								\
		static struct lock_class_key __key;			\
									\
		(_work)->data = (atomic_long_t) WORK_DATA_INIT();	\
		lockdep_init_map(&(_work)->lockdep_map, #_work, &__key, 0);\
		INIT_LIST_HEAD(&(_work)->entry);			\
		PREPARE_WORK((_work), (_func));				\
	} while (0)
#else
#define INIT_WORK(_work, _func)						\
	do {								\
		(_work)->data = (atomic_long_t) WORK_DATA_INIT();	\
		INIT_LIST_HEAD(&(_work)->entry);			\
		PREPARE_WORK((_work), (_func));				\
	} while (0)
#endif

#define INIT_DELAYED_WORK(_work, _func)				\
	do {							\
		INIT_WORK(&(_work)->work, (_func));		\
		init_timer(&(_work)->timer);			\
	} while (0)

#define INIT_DELAYED_WORK_ON_STACK(_work, _func)		\
	do {							\
		INIT_WORK(&(_work)->work, (_func));		\
		init_timer_on_stack(&(_work)->timer);		\
	} while (0)

#define INIT_DELAYED_WORK_DEFERRABLE(_work, _func)			\
	do {							\
		INIT_WORK(&(_work)->work, (_func));		\
		init_timer_deferrable(&(_work)->timer);		\
	} while (0)

#define INIT_DELAYED_WORK_ON_STACK(_work, _func)		\
	do {							\
		INIT_WORK(&(_work)->work, (_func));		\
		init_timer_on_stack(&(_work)->timer);		\
	} while (0)

/**
 * work_pending - Find out whether a work item is currently pending
 * @work: The work item in question
 */
#define work_pending(work) \
	test_bit(WORK_STRUCT_PENDING, work_data_bits(work))

/**
 * delayed_work_pending - Find out whether a delayable work item is currently
 * pending
 * @work: The work item in question
 */
#define delayed_work_pending(w) \
	work_pending(&(w)->work)

/**
 * work_clear_pending - for internal use only, mark a work item as not pending
 * @work: The work item in question
 */
#define work_clear_pending(work) \
	clear_bit(WORK_STRUCT_PENDING, work_data_bits(work))


/*核心函数*/
extern struct workqueue_struct *
__create_workqueue_key(const char *name, int singlethread,
		       int freezeable, int rt, struct lock_class_key *key,
		       const char *lock_name);

#ifdef CONFIG_LOCKDEP
#define __create_workqueue(name, singlethread, freezeable, rt)	\
({								\
	static struct lock_class_key __key;			\
	const char *__lock_name;				\
								\
	if (__builtin_constant_p(name))				\
		__lock_name = (name);				\
	else							\
		__lock_name = #name;				\
								\
	__create_workqueue_key((name), (singlethread),		\
			       (freezeable), (rt), &__key,	\
			       __lock_name);			\
})
#else
#define __create_workqueue(name, singlethread, freezeable, rt)	\
	__create_workqueue_key((name), (singlethread), (freezeable), (rt), \
			       NULL, NULL)
#endif

/**/
#define create_workqueue(name) __create_workqueue((name), 0, 0, 0)
#define create_rt_workqueue(name) __create_workqueue((name), 0, 0, 1)
#define create_freezeable_workqueue(name) __create_workqueue((name), 1, 1, 0)
/* 生成单线程的工作队列 singlethread=1,与create_workqueue函数的区别是
 * create_singlethread_workqueue只在系统中的第一个CPU(singlethread_cpu)上创建
 * 工作队列和工人线程,而create_workquque函数会在系统中的每个CPU上都创建工作
 * 队列和工人线程*/
#define create_singlethread_workqueue(name) __create_workqueue((name), 1, 0, 0)

extern void destroy_workqueue(struct workqueue_struct *wq);

/* 在用queue_work向工作队列提交工作节点时，如果工作队列是singlethread类型的，
 * 因为此时只有一个worklist，所以工作节点只能提交到这唯一的一个worklist上，
 * 反之，如果工作队列不是singlethread类型的，那么工作节点将会提交到当前运行
 * queue_work的CPU所在的worklist中*/
extern int queue_work(struct workqueue_struct *wq, struct work_struct *work);
/**/
extern int queue_work_on(int cpu, struct workqueue_struct *wq,
			struct work_struct *work);
extern int queue_delayed_work(struct workqueue_struct *wq,
			struct delayed_work *work, unsigned long delay);
extern int queue_delayed_work_on(int cpu, struct workqueue_struct *wq,
			struct delayed_work *work, unsigned long delay);

extern void flush_workqueue(struct workqueue_struct *wq);
extern void flush_scheduled_work(void);
extern void flush_delayed_work(struct delayed_work *work);

extern int schedule_work(struct work_struct *work);
extern int schedule_work_on(int cpu, struct work_struct *work);
extern int schedule_delayed_work(struct delayed_work *work, unsigned long delay);
extern int schedule_delayed_work_on(int cpu, struct delayed_work *work,
					unsigned long delay);
extern int schedule_on_each_cpu(work_func_t func);
extern int current_is_keventd(void);
extern int keventd_up(void);

extern void init_workqueues(void);
int execute_in_process_context(work_func_t fn, struct execute_work *);

extern int flush_work(struct work_struct *work);

extern int cancel_work_sync(struct work_struct *work);

/*
 * Kill off a pending schedule_delayed_work().  Note that the work callback
 * function may still be running on return from cancel_delayed_work(), unless
 * it returns 1 and the work doesn't re-arm itself. Run flush_workqueue() or
 * cancel_work_sync() to wait on it.
 */
static inline int cancel_delayed_work(struct delayed_work *work)
{
	int ret;

	ret = del_timer_sync(&work->timer);
	if (ret)
		work_clear_pending(&work->work);
	return ret;
}

/*
 * Like above, but uses del_timer() instead of del_timer_sync(). This means,
 * if it returns 0 the timer function may be running and the queueing is in
 * progress.
 */
static inline int __cancel_delayed_work(struct delayed_work *work)
{
	int ret;

	ret = del_timer(&work->timer);
	if (ret)
		work_clear_pending(&work->work);
	return ret;
}

extern int cancel_delayed_work_sync(struct delayed_work *work);

/* Obsolete. use cancel_delayed_work_sync() */
static inline
void cancel_rearming_delayed_workqueue(struct workqueue_struct *wq,
					struct delayed_work *work)
{
	cancel_delayed_work_sync(work);
}

/* Obsolete. use cancel_delayed_work_sync() */
static inline
void cancel_rearming_delayed_work(struct delayed_work *work)
{
	cancel_delayed_work_sync(work);
}

#ifndef CONFIG_SMP
static inline long work_on_cpu(unsigned int cpu, long (*fn)(void *), void *arg)
{
	return fn(arg);
}
#else
long work_on_cpu(unsigned int cpu, long (*fn)(void *), void *arg);
#endif /* CONFIG_SMP */
#endif
