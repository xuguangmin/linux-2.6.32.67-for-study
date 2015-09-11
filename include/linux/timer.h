#ifndef _LINUX_TIMER_H
#define _LINUX_TIMER_H

#include <linux/list.h>
#include <linux/ktime.h>
#include <linux/stddef.h>
#include <linux/debugobjects.h>
#include <linux/stringify.h>



/**
 * EXAMPLE:设备驱动程序通过使用内核定时器来轮询设备状态
   struct device_regs *devreg = NULL;//定义一个用于表示设备寄存器的结构体指针
   struct timer_list demo_timer;//定义一个内核定时器对象

   //
   //定义定时器函数,当定时器对象demo_timer中expires成员指定的时间到期后,该函数将被调用
   static void demo_timer_func(unsigned long data)
   {
   	//定时器函数中重新启动定时器以实现轮询的目的
	demo_timer.expires = jiffies + HZ;
	add_timer(&demo_timer);

	//定时器函数将data参数通过类型转换获得设备寄存器的结构体指针
	struct device_reg *preg = (struct device_regs*)data;
	//定时器函数此后将会读取设备状态
	...
   }

   //用于打开设备的函数实现
   static int demo_dev_open(...);
   {
   	...
	//分配设备寄存器结构体的指针变量,最好放在模块初始化函数中
	devreg = kmalloc(sizeof(struct device_regs), GFP_KERNEL);
	...
	init_timer(&demo_timer);//调用内核函数init_timer来初始化定时器对象 demo_timer
	demo_timer.expires = jiffies + HZ;//设定定时器到期时间点,从先在开始的1分钟
	demo_timer.data = (unsigned long)devreg;//将设备寄存器指针地址作为参数
	demo_timer.function = &demo_timer_func;
	add_timer(&demo_timer);
	...
   }


   //用于关闭设备的函数实现
   static int demo_dev_release(...)
   {
   	...
	del_timer_sync(&demo_timer);//删除定时器对象
	...
   }
 */




struct tvec_base;
/*用来表示定时器的数据结构*/
struct timer_list {
	struct list_head entry;
	unsigned long expires;	/*指定定时器的到期时间*/

	void (*function)(unsigned long);/*定时器函数,当expires中指定的时间到期时,
					 *该函数被触发*/
	unsigned long data;/*定时器对象中携带的数据,通常的用途是把该成员作为实际参数传递
			    *给定时器函数,因为定时器函数将在中断上下文中执行,而非当前
			    *进程的地址空间*/

	struct tvec_base *base;
#ifdef CONFIG_TIMER_STATS
	void *start_site;
	char start_comm[16];
	int start_pid;
#endif
#ifdef CONFIG_LOCKDEP
	struct lockdep_map lockdep_map;
#endif
};

extern struct tvec_base boot_tvec_bases;

#ifdef CONFIG_LOCKDEP
/*
 * NB: because we have to copy the lockdep_map, setting the lockdep_map key
 * (second argument) here is required, otherwise it could be initialised to
 * the copy of the lockdep_map later! We use the pointer to and the string
 * "<file>:<line>" as the key resp. the name of the lockdep_map.
 */
#define __TIMER_LOCKDEP_MAP_INITIALIZER(_kn)				\
	.lockdep_map = STATIC_LOCKDEP_MAP_INIT(_kn, &_kn),
#else
#define __TIMER_LOCKDEP_MAP_INITIALIZER(_kn)
#endif

#define TIMER_INITIALIZER(_function, _expires, _data) {		\
		.entry = { .prev = TIMER_ENTRY_STATIC },	\
		.function = (_function),			\
		.expires = (_expires),				\
		.data = (_data),				\
		.base = &boot_tvec_bases,			\
		__TIMER_LOCKDEP_MAP_INITIALIZER(		\
			__FILE__ ":" __stringify(__LINE__))	\
	}

#define DEFINE_TIMER(_name, _function, _expires, _data)		\
	struct timer_list _name =				\
		TIMER_INITIALIZER(_function, _expires, _data)

void init_timer_key(struct timer_list *timer,
		    const char *name,
		    struct lock_class_key *key);
void init_timer_deferrable_key(struct timer_list *timer,
			       const char *name,
			       struct lock_class_key *key);

#ifdef CONFIG_LOCKDEP
/*初始化定时器对象*/
#define init_timer(timer)						\
	do {								\
		static struct lock_class_key __key;			\
		init_timer_key((timer), #timer, &__key);		\
	} while (0)

#define init_timer_deferrable(timer)					\
	do {								\
		static struct lock_class_key __key;			\
		init_timer_deferrable_key((timer), #timer, &__key);	\
	} while (0)

#define init_timer_on_stack(timer)					\
	do {								\
		static struct lock_class_key __key;			\
		init_timer_on_stack_key((timer), #timer, &__key);	\
	} while (0)

#define setup_timer(timer, fn, data)					\
	do {								\
		static struct lock_class_key __key;			\
		setup_timer_key((timer), #timer, &__key, (fn), (data));\
	} while (0)

#define setup_timer_on_stack(timer, fn, data)				\
	do {								\
		static struct lock_class_key __key;			\
		setup_timer_on_stack_key((timer), #timer, &__key,	\
					 (fn), (data));			\
	} while (0)
#else
#define init_timer(timer)\
	init_timer_key((timer), NULL, NULL)
#define init_timer_deferrable(timer)\
	init_timer_deferrable_key((timer), NULL, NULL)
#define init_timer_on_stack(timer)\
	init_timer_on_stack_key((timer), NULL, NULL)
#define setup_timer(timer, fn, data)\
	setup_timer_key((timer), NULL, NULL, (fn), (data))
#define setup_timer_on_stack(timer, fn, data)\
	setup_timer_on_stack_key((timer), NULL, NULL, (fn), (data))
#endif

#ifdef CONFIG_DEBUG_OBJECTS_TIMERS
extern void init_timer_on_stack_key(struct timer_list *timer,
				    const char *name,
				    struct lock_class_key *key);
extern void destroy_timer_on_stack(struct timer_list *timer);
#else
static inline void destroy_timer_on_stack(struct timer_list *timer) { }
static inline void init_timer_on_stack_key(struct timer_list *timer,
					   const char *name,
					   struct lock_class_key *key)
{
	init_timer_key(timer, name, key);
}
#endif

static inline void setup_timer_key(struct timer_list * timer,
				const char *name,
				struct lock_class_key *key,
				void (*function)(unsigned long),
				unsigned long data)
{
	timer->function = function;
	timer->data = data;
	init_timer_key(timer, name, key);
}

static inline void setup_timer_on_stack_key(struct timer_list *timer,
					const char *name,
					struct lock_class_key *key,
					void (*function)(unsigned long),
					unsigned long data)
{
	timer->function = function;
	timer->data = data;
	init_timer_on_stack_key(timer, name, key);
}

/**
 * timer_pending - is a timer pending?
 * @timer: the timer in question
 *
 * timer_pending will tell whether a given timer is currently pending,
 * or not. Callers must ensure serialization wrt. other operations done
 * to this timer, eg. interrupt contexts, or other CPUs on SMP.
 *
 * return value: 1 if the timer is pending, 0 if not.
 */
static inline int timer_pending(const struct timer_list * timer)
{
	return timer->entry.next != NULL;
}

extern void add_timer_on(struct timer_list *timer, int cpu);
extern int del_timer(struct timer_list * timer);
extern int mod_timer(struct timer_list *timer, unsigned long expires);
extern int mod_timer_pending(struct timer_list *timer, unsigned long expires);
extern int mod_timer_pinned(struct timer_list *timer, unsigned long expires);

#define TIMER_NOT_PINNED	0
#define TIMER_PINNED		1
/*
 * The jiffies value which is added to now, when there is no timer
 * in the timer wheel:
 */
#define NEXT_TIMER_MAX_DELTA	((1UL << 30) - 1)

/*
 * Return when the next timer-wheel timeout occurs (in absolute jiffies),
 * locks the timer base and does the comparison against the given
 * jiffie.
 */
extern unsigned long get_next_timer_interrupt(unsigned long now);

/*
 * Timer-statistics info:
 */
#ifdef CONFIG_TIMER_STATS

extern int timer_stats_active;

#define TIMER_STATS_FLAG_DEFERRABLE	0x1

extern void init_timer_stats(void);

extern void timer_stats_update_stats(void *timer, pid_t pid, void *startf,
				     void *timerf, char *comm,
				     unsigned int timer_flag);

extern void __timer_stats_timer_set_start_info(struct timer_list *timer,
					       void *addr);

static inline void timer_stats_timer_set_start_info(struct timer_list *timer)
{
	if (likely(!timer_stats_active))
		return;
	__timer_stats_timer_set_start_info(timer, __builtin_return_address(0));
}

static inline void timer_stats_timer_clear_start_info(struct timer_list *timer)
{
	timer->start_site = NULL;
}
#else
static inline void init_timer_stats(void)
{
}

static inline void timer_stats_timer_set_start_info(struct timer_list *timer)
{
}

static inline void timer_stats_timer_clear_start_info(struct timer_list *timer)
{
}
#endif

extern void add_timer(struct timer_list *timer);

#ifdef CONFIG_SMP
  extern int try_to_del_timer_sync(struct timer_list *timer);
  extern int del_timer_sync(struct timer_list *timer);
#else
# define try_to_del_timer_sync(t)	del_timer(t)
# define del_timer_sync(t)		del_timer(t)
#endif

#define del_singleshot_timer_sync(t) del_timer_sync(t)

extern void init_timers(void);
extern void run_local_timers(void);
struct hrtimer;
extern enum hrtimer_restart it_real_fn(struct hrtimer *);

unsigned long __round_jiffies(unsigned long j, int cpu);
unsigned long __round_jiffies_relative(unsigned long j, int cpu);
unsigned long round_jiffies(unsigned long j);
unsigned long round_jiffies_relative(unsigned long j);

unsigned long __round_jiffies_up(unsigned long j, int cpu);
unsigned long __round_jiffies_up_relative(unsigned long j, int cpu);
unsigned long round_jiffies_up(unsigned long j);
unsigned long round_jiffies_up_relative(unsigned long j);

#endif
