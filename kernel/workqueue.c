/*
 * linux/kernel/workqueue.c
 *
 * Generic mechanism for defining kernel helper threads for running
 * arbitrary tasks in process context.
 *
 * Started by Ingo Molnar, Copyright (C) 2002
 *
 * Derived from the taskqueue/keventd code by:
 *
 *   David Woodhouse <dwmw2@infradead.org>
 *   Andrew Morton
 *   Kai Petzke <wpp@marie.physik.tu-berlin.de>
 *   Theodore Ts'o <tytso@mit.edu>
 *
 * Made to use alloc_percpu by Christoph Lameter.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/signal.h>
#include <linux/completion.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <linux/cpu.h>
#include <linux/notifier.h>
#include <linux/kthread.h>
#include <linux/hardirq.h>
#include <linux/mempolicy.h>
#include <linux/freezer.h>
#include <linux/kallsyms.h>
#include <linux/debug_locks.h>
#include <linux/lockdep.h>
#define CREATE_TRACE_POINTS
#include <trace/events/workqueue.h>

/*
 * The per-CPU workqueue (if single thread, we always use the first
 * possible cpu).
 */
 /*CPU工作队列管理结构,实际代码中struct cpu_workqueue_struct对象是个per_CPU型的变量，通过alloc_percpu函数动态创建，系统中的每个cpu都有一份，本书称struct cpu_workqueue_struct为CPU工作队列管理结构*/
struct cpu_workqueue_struct {

	spinlock_t lock;	/*对象的自旋锁,用于对可能的并发访问该对象时提供互斥保护机制*/

	struct list_head worklist;  /*双向链表对象,用来将驱动程序提交的工作节点形成链表,驱动程序中的延迟操作以工作节点的形式存在*/
	wait_queue_head_t more_work; /*等待队列头节点,工作队列的工人线程没有工作节点需要处理时将进入睡眠状态,此时他需要进入该等待队列*/
	struct work_struct *current_work;/*用于记录当前工人线程正在处理的工作节点*/

	struct workqueue_struct *wq;	/*指向系统工作队列管理结构*/
	struct task_struct *thread;	/*指向工人线程所在的进程空间结构*/
} ____cacheline_aligned;

/*
 * The externally visible workqueue abstraction is an array of
 * per-CPU workqueues:
 */
 /*工作队列管理结构,内核会为创建的每个工作队列生成一个工作队列管理结构对象*/
struct workqueue_struct {
	struct cpu_workqueue_struct *cpu_wq; /*指向cpu工作队列管理结构的per-CPU
			* 根据该指针,系统中每个cpu都可以通过per_cpu_ptr来获得属				* 于自己的cpu工作队列管理结构的对象*/
	struct list_head list;/*双向链表对象,用于将工作队列管理结构加入到一
				*个全局变量中,只有对非singlethread工作队列有效*/
	const char *name;  /* 工作队列的名称*/
	int singlethread;  /* 标识创建的工作队列中工人线程的数量*/
	int freezeable;		/* Freeze threads during suspend 
				 * 表示进程可否进入冻结状态*/
	int rt;		/*用来调整worker_thread线程所在进程的调度策略*/
#ifdef CONFIG_LOCKDEP
	struct lockdep_map lockdep_map;
#endif
};

/* Serializes the accesses to the list of workqueues. */
static DEFINE_SPINLOCK(workqueue_lock);
static LIST_HEAD(workqueues);

static int singlethread_cpu __read_mostly;
static const struct cpumask *cpu_singlethread_map __read_mostly;
/*
 * _cpu_down() first removes CPU from cpu_online_map, then CPU_DEAD
 * flushes cwq->worklist. This means that flush_workqueue/wait_on_work
 * which comes in between can't use for_each_online_cpu(). We could
 * use cpu_possible_map, the cpumask below is more a documentation
 * than optimization.
 */
static cpumask_var_t cpu_populated_map __read_mostly;

/* If it's single threaded, it isn't in the list of workqueues. */
static inline int is_wq_single_threaded(struct workqueue_struct *wq)
{
	return wq->singlethread;
}

static const struct cpumask *wq_cpu_map(struct workqueue_struct *wq)
{
	return is_wq_single_threaded(wq)
		? cpu_singlethread_map : cpu_populated_map;
}

/* 如果是singlethread类型的工作队列,那么工作节点就提交到第一个CPU的cwq上,
 * 否则哪个CPU调用queue_work,工作节点就提交到哪个CPU的cwq上*/
static
struct cpu_workqueue_struct *wq_per_cpu(struct workqueue_struct *wq, int cpu)
{
	if (unlikely(is_wq_single_threaded(wq)))
		cpu = singlethread_cpu;
	return per_cpu_ptr(wq->cpu_wq, cpu);
}

/*
 * Set the workqueue on which a work item is to be run
 * - Must *only* be called if the pending flag is set
 */
static inline void set_wq_data(struct work_struct *work,
				struct cpu_workqueue_struct *cwq)
{
	unsigned long new;

	BUG_ON(!work_pending(work));

	new = (unsigned long) cwq | (1UL << WORK_STRUCT_PENDING);
	new |= WORK_STRUCT_FLAG_MASK & *work_data_bits(work);
	atomic_long_set(&work->data, new);
}

static inline
struct cpu_workqueue_struct *get_wq_data(struct work_struct *work)
{
	return (void *) (atomic_long_read(&work->data) & WORK_STRUCT_WQ_DATA_MASK);
}

/*__queue_work调用该函数,完成节点的提交*/
static void insert_work(struct cpu_workqueue_struct *cwq,
			struct work_struct *work, struct list_head *head)
{
	trace_workqueue_insertion(cwq->thread, work);

	set_wq_data(work, cwq);
	/*
	 * Ensure that we get the right work->data if we see the
	 * result of list_add() below, see try_to_grab_pending().
	 */
	smp_wmb();
	list_add_tail(&work->entry, head);
	/*唤醒在等待队列cwq->more_work上睡眠的worker_thread,如果worker_thread正在运行,就什么也不做*/
	wake_up(&cwq->more_work);
}

/* 提交节点
 * @cwq:CPU工作队列管理结构
 * @work:待提交的节点指针
 */
static void __queue_work(struct cpu_workqueue_struct *cwq,
			 struct work_struct *work)
{
	unsigned long flags;

	spin_lock_irqsave(&cwq->lock, flags);
	/*完成节点的提交*/
	insert_work(cwq, work, &cwq->worklist);
	spin_unlock_irqrestore(&cwq->lock, flags);
}

/**
 * queue_work - queue work on a workqueue
 * @wq: workqueue to use
 * @work: work to queue
 *
 * Returns 0 if @work was already on a queue, non-zero otherwise.
 *
 * We queue the work to the CPU on which it was submitted, but if the CPU dies
 * it can be processed by another CPU.
 */
 /* 当驱动程序调用queue_work向工作队列提交节点work时,queue_work会把work->data的
  * WORK_STRUCT_PENDING位置1,这是为了防止驱动程序将一个尚未被处理的工作节点
  * 再次提交cwq->worklist

 * 在用queue_work向工作队列提交工作节点时，如果工作队列是singlethread类型的，
 * 因为此时只有一个worklist，所以工作节点只能提交到这唯一的一个worklist上，
 * 反之，如果工作队列不是singlethread类型的，那么工作节点将会提交到当前运行
 * queue_work的CPU所在的worklist中*/
int queue_work(struct workqueue_struct *wq, struct work_struct *work)
{
	int ret;

	ret = queue_work_on(get_cpu(), wq, work);
	put_cpu();

	return ret;
}
EXPORT_SYMBOL_GPL(queue_work);

/**
 * queue_work_on - queue work on specific cpu
 * @cpu: CPU number to execute work on
 * @wq: workqueue to use
 * @work: work to queue
 *
 * Returns 0 if @work was already on a queue, non-zero otherwise.
 *
 * We queue the work to a specific CPU, the caller must ensure it
 * can't go away.
 */
int
queue_work_on(int cpu, struct workqueue_struct *wq, struct work_struct *work)
{
	int ret = 0;

	/* 首先检测work->data的WORK_STRUCT_PENDING位有没有被置1,置1意味着此前该
	 * work已经被提交还没有处理,内核禁止驱动程序在一个工作节点还没处理完就
	 * 再次提交该节点,此处的检测也告诉驱动程序,在构造工作节点对象work时,
	 * 应该确保work->data低2位为0,如果work->data的WORK_STRUCT_PENDING为是0
	 * ,那么就把该位置1表明工作节点处于等待处理的状态,然后调用__queue_work
	 * 来提交节点*/
	if (!test_and_set_bit(WORK_STRUCT_PENDING, work_data_bits(work))) {
		BUG_ON(!list_empty(&work->entry));
		__queue_work(wq_per_cpu(wq, cpu), work);
		ret = 1;
	}
	return ret;
}
EXPORT_SYMBOL_GPL(queue_work_on);

static void delayed_work_timer_fn(unsigned long __data)
{
	struct delayed_work *dwork = (struct delayed_work *)__data;
	struct cpu_workqueue_struct *cwq = get_wq_data(&dwork->work);
	struct workqueue_struct *wq = cwq->wq;

	__queue_work(wq_per_cpu(wq, smp_processor_id()), &dwork->work);
}

/**
 * queue_delayed_work - queue work on a workqueue after delay
 * @wq: workqueue to use
 * @dwork: delayable work to queue
 * @delay: number of jiffies to wait before queueing
 *
 * Returns 0 if @work was already on a queue, non-zero otherwise.
 */

/** 
 * delay代表一个延迟的时间,工作节点需要等到delay指定的时间过后才会被真正提交
 * 到队列wq上
 */
int queue_delayed_work(struct workqueue_struct *wq,
			struct delayed_work *dwork, unsigned long delay)
{
	/*和queue_work一样*/
	if (delay == 0)
		return queue_work(wq, &dwork->work);

	/*延迟提交*/
	return queue_delayed_work_on(-1, wq, dwork, delay);
}
EXPORT_SYMBOL_GPL(queue_delayed_work);

/**
 * queue_delayed_work_on - queue work on specific CPU after delay
 * @cpu: CPU number to execute work on
 * @wq: workqueue to use
 * @dwork: work to queue
 * @delay: number of jiffies to wait before queueing
 *
 * Returns 0 if @work was already on a queue, non-zero otherwise.
 */
/**
 * 利用定时器timer来实现延迟提交的工作,timer->expires = jiffies + delay,这样当
 * delay实际到期后,timer->function = delayed_work_timer_fn将被调用,
 * delayed_work_timer_fn会把queue_delayed_work_on要提交的节点提交到工作队列中,
 * 所有驱动程序要使用queue_delayed_work,要生成一个struct delayed_work对象
 */
int queue_delayed_work_on(int cpu, struct workqueue_struct *wq,
			struct delayed_work *dwork, unsigned long delay)
{
	int ret = 0;
	struct timer_list *timer = &dwork->timer;
	struct work_struct *work = &dwork->work;

	if (!test_and_set_bit(WORK_STRUCT_PENDING, work_data_bits(work))) {
		BUG_ON(timer_pending(timer));
		BUG_ON(!list_empty(&work->entry));

		timer_stats_timer_set_start_info(&dwork->timer);

		/* This stores cwq for the moment, for the timer_fn */
		set_wq_data(work, wq_per_cpu(wq, raw_smp_processor_id()));
		timer->expires = jiffies + delay;
		timer->data = (unsigned long)dwork;
		timer->function = delayed_work_timer_fn;

		if (unlikely(cpu >= 0))
			add_timer_on(timer, cpu);
		else
			add_timer(timer);
		ret = 1;
	}
	return ret;
}
EXPORT_SYMBOL_GPL(queue_delayed_work_on);

/*处理cwq->worklist上的工作节点*/
static void run_workqueue(struct cpu_workqueue_struct *cwq)
{
	spin_lock_irq(&cwq->lock);
	/**
	 * 函数在while循环中遍历cwq->worklist链表,对于其中的每个工作节点work,
	 * 先将其从cwk->worklist链表删除,然后调用工作节点上的延迟函数f(work),
	 * 传递给函数的参数是延迟函数所在工作节点的指针work,一个工作节点被处理
	 * 完之后,将不会再出现在工作队列的cwq->worklist中,除非被再次提交
	 */
	while (!list_empty(&cwq->worklist)) {
		struct work_struct *work = list_entry(cwq->worklist.next,
						struct work_struct, entry);
		work_func_t f = work->func;
#ifdef CONFIG_LOCKDEP
		/*
		 * It is permissible to free the struct work_struct
		 * from inside the function that is called from it,
		 * this we need to take into account for lockdep too.
		 * To avoid bogus "held lock freed" warnings as well
		 * as problems when looking into work->lockdep_map,
		 * make a copy and use that here.
		 */
		struct lockdep_map lockdep_map = work->lockdep_map;
#endif
		trace_workqueue_execution(cwq->thread, work);
		cwq->current_work = work;
		list_del_init(cwq->worklist.next);
		spin_unlock_irq(&cwq->lock);

		BUG_ON(get_wq_data(work) != cwq);
		/* 函数用来清除work->data的WORK_STRUCT_PENDING位(位0),这里内核把
		 * work->data的低2位用于记录work的状态信息*/
		work_clear_pending(work);
		lock_map_acquire(&cwq->wq->lockdep_map);
		lock_map_acquire(&lockdep_map);
		/*调用延迟函数*/
		f(work);
		lock_map_release(&lockdep_map);
		lock_map_release(&cwq->wq->lockdep_map);

		if (unlikely(in_atomic() || lockdep_depth(current) > 0)) {
			printk(KERN_ERR "BUG: workqueue leaked lock or atomic: "
					"%s/0x%08x/%d\n",
					current->comm, preempt_count(),
				       	task_pid_nr(current));
			printk(KERN_ERR "    last function: ");
			print_symbol("%s\n", (unsigned long)f);
			debug_show_held_locks(current);
			dump_stack();
		}

		spin_lock_irq(&cwq->lock);
		cwq->current_work = NULL;
	}
	spin_unlock_irq(&cwq->lock);
}

/*工人线程*/
static int worker_thread(void *__cwq)
{
	struct cpu_workqueue_struct *cwq = __cwq;
	DEFINE_WAIT(wait);

	if (cwq->wq->freezeable)
		set_freezable();

	/*主体*/
	for (;;) {
		prepare_to_wait(&cwq->more_work, &wait, TASK_INTERRUPTIBLE);
		if (!freezing(current) &&
		    !kthread_should_stop() &&
		    list_empty(&cwq->worklist))
			schedule();
		finish_wait(&cwq->more_work, &wait);

		try_to_freeze();

		/*检测有没有别的函数对他调用了kthread_stop,有的话代表该线程的kthread对象的should_stop成员将被置1,此时worker_thread将通过break跳出循环,线程函数所在的进程将会终结*/
		if (kthread_should_stop())
			break;

		/*处理cwq->worklist上的工作节点*/
		run_workqueue(cwq);
	}

	return 0;
}

struct wq_barrier {
	struct work_struct	work;
	struct completion	done;
};

static void wq_barrier_func(struct work_struct *work)
{
	struct wq_barrier *barr = container_of(work, struct wq_barrier, work);
	complete(&barr->done);
}

/*函数提交中止节点*/
static void insert_wq_barrier(struct cpu_workqueue_struct *cwq,
			struct wq_barrier *barr, struct list_head *head)
{
	INIT_WORK(&barr->work, wq_barrier_func);
	__set_bit(WORK_STRUCT_PENDING, work_data_bits(&barr->work));

	init_completion(&barr->done);

	insert_work(cwq, &barr->work, head);
}

/* 终止worker_thread所在进程的一个前提时要确保所有提交到cwq->worklist中的工作
 * 节点都已经处理完毕,由此函数完成;函数确保cwq->worklist上所有工作节点都已经
 * 处理完毕的设计思想是利用完成接口completion:如果cwq->worklist不为空或者
 * cwq->current_work不为空,说明cwq_worklist上还有工作节点或者worker_thread正在
 * 处理一个工作节点,则向cwq->worklist上提交一个新的工作节点,这里不妨称为终止
 * 节点,当终止节点上的延迟函数被执行时,它将调用complete函数通知
 * flush_cpu_workqueue,而后者在提交完终止节点之后将睡眠等待wait_for_completion
 * 函数上,直到之前提交的终止节点上的延迟函数执行结束;
 * 
 * 函数的操作范围只限于单个CPU,对于非singlethread工作队列,因为每个CPU上都有
 * 一个工作队列和worker_thread,要确保系统中所有CPU上的工作队列中的工作节点都被
 * 处理完,应该使用flush_workqueue函数*/
static int flush_cpu_workqueue(struct cpu_workqueue_struct *cwq)
{
	int active = 0;
	struct wq_barrier barr;

	/*意味着驱动程序不应该在提交的工作节点延迟函数中调用flush_cpu_workqueue*/
	WARN_ON(cwq->thread == current);

	spin_lock_irq(&cwq->lock);
	if (!list_empty(&cwq->worklist) || cwq->current_work != NULL) {
		/*函数提交中止节点*/
		insert_wq_barrier(cwq, &barr, &cwq->worklist);
		active = 1;
	}
	spin_unlock_irq(&cwq->lock);

	if (active)
		/*提交完中止节点,再此等待延迟函数执行*/
		wait_for_completion(&barr.done);

	return active;
}

/**
 * flush_workqueue - ensure that any scheduled work has run to completion.
 * @wq: workqueue to flush
 *
 * Forces execution of the workqueue and blocks until its completion.
 * This is typically used in driver shutdown handlers.
 *
 * We sleep until all works which were queued on entry have been handled,
 * but we are not livelocked by new incoming ones.
 *
 * This function used to run the workqueues itself.  Now we just wait for the
 * helper threads to do it.
 */
 /* 函数返回后可以确保函数调用前提交的所有工作节点都已经处理完毕,与之类似的还有
  * 一个函数flush_work,如果驱动程序想等待在某个提交的工作节点上直到该节点处理
  * 完毕函数才返回，就可以使用flush_work函数*/
void flush_workqueue(struct workqueue_struct *wq)
{
	const struct cpumask *cpu_map = wq_cpu_map(wq);
	int cpu;

	might_sleep();
	lock_map_acquire(&wq->lockdep_map);
	lock_map_release(&wq->lockdep_map);
	for_each_cpu(cpu, cpu_map)
		flush_cpu_workqueue(per_cpu_ptr(wq->cpu_wq, cpu));
}
EXPORT_SYMBOL_GPL(flush_workqueue);

/**
 * flush_work - block until a work_struct's callback has terminated
 * @work: the work which is to be flushed
 *
 * Returns false if @work has already terminated.
 *
 * It is expected that, prior to calling flush_work(), the caller has
 * arranged for the work to not be requeued, otherwise it doesn't make
 * sense to use this function.
 */

/* 如果驱动程序想等待在某个提交的工作节点上直到该节点处理完毕函数才返回，
 * 就可以使用flush_work函数,参数work就是调用者要等待在其上的工作节点,如果函数
 * 调用时work已经处理完毕，函数返回０*/
int flush_work(struct work_struct *work)
{
	struct cpu_workqueue_struct *cwq;
	struct list_head *prev;
	struct wq_barrier barr;

	might_sleep();
	cwq = get_wq_data(work);
	if (!cwq)
		return 0;

	lock_map_acquire(&cwq->wq->lockdep_map);
	lock_map_release(&cwq->wq->lockdep_map);

	prev = NULL;
	spin_lock_irq(&cwq->lock);
	if (!list_empty(&work->entry)) {
		/*
		 * See the comment near try_to_grab_pending()->smp_rmb().
		 * If it was re-queued under us we are not going to wait.
		 */
		smp_rmb();
		if (unlikely(cwq != get_wq_data(work)))
			goto out;
		prev = &work->entry;
	} else {
		if (cwq->current_work != work)
			goto out;
		prev = &cwq->worklist;
	}
	insert_wq_barrier(cwq, &barr, prev->next);
out:
	spin_unlock_irq(&cwq->lock);
	if (!prev)
		return 0;

	wait_for_completion(&barr.done);
	return 1;
}
EXPORT_SYMBOL_GPL(flush_work);

/*
 * Upon a successful return (>= 0), the caller "owns" WORK_STRUCT_PENDING bit,
 * so this work can't be re-armed in any way.
 */
static int try_to_grab_pending(struct work_struct *work)
{
	struct cpu_workqueue_struct *cwq;
	int ret = -1;

	if (!test_and_set_bit(WORK_STRUCT_PENDING, work_data_bits(work)))
		return 0;

	/*
	 * The queueing is in progress, or it is already queued. Try to
	 * steal it from ->worklist without clearing WORK_STRUCT_PENDING.
	 */

	cwq = get_wq_data(work);
	if (!cwq)
		return ret;

	spin_lock_irq(&cwq->lock);
	if (!list_empty(&work->entry)) {
		/*
		 * This work is queued, but perhaps we locked the wrong cwq.
		 * In that case we must see the new value after rmb(), see
		 * insert_work()->wmb().
		 */
		smp_rmb();
		if (cwq == get_wq_data(work)) {
			list_del_init(&work->entry);
			ret = 1;
		}
	}
	spin_unlock_irq(&cwq->lock);

	return ret;
}

static void wait_on_cpu_work(struct cpu_workqueue_struct *cwq,
				struct work_struct *work)
{
	struct wq_barrier barr;
	int running = 0;

	spin_lock_irq(&cwq->lock);
	if (unlikely(cwq->current_work == work)) {
		insert_wq_barrier(cwq, &barr, cwq->worklist.next);
		running = 1;
	}
	spin_unlock_irq(&cwq->lock);

	if (unlikely(running))
		wait_for_completion(&barr.done);
}

static void wait_on_work(struct work_struct *work)
{
	struct cpu_workqueue_struct *cwq;
	struct workqueue_struct *wq;
	const struct cpumask *cpu_map;
	int cpu;

	might_sleep();

	lock_map_acquire(&work->lockdep_map);
	lock_map_release(&work->lockdep_map);

	cwq = get_wq_data(work);
	if (!cwq)
		return;

	wq = cwq->wq;
	cpu_map = wq_cpu_map(wq);

	for_each_cpu(cpu, cpu_map)
		wait_on_cpu_work(per_cpu_ptr(wq->cpu_wq, cpu), work);
}

static int __cancel_work_timer(struct work_struct *work,
				struct timer_list* timer)
{
	int ret;

	do {
		ret = (timer && likely(del_timer(timer)));
		if (!ret)
			ret = try_to_grab_pending(work);
		wait_on_work(work);
	} while (unlikely(ret < 0));

	work_clear_pending(work);
	return ret;
}

/**
 * cancel_work_sync - block until a work_struct's callback has terminated
 * @work: the work which is to be flushed
 *
 * Returns true if @work was pending.
 *
 * cancel_work_sync() will cancel the work if it is queued. If the work's
 * callback appears to be running, cancel_work_sync() will block until it
 * has completed.
 *
 * It is possible to use this function if the work re-queues itself. It can
 * cancel the work even if it migrates to another workqueue, however in that
 * case it only guarantees that work->func() has completed on the last queued
 * workqueue.
 *
 * cancel_work_sync(&delayed_work->work) should be used only if ->timer is not
 * pending, otherwise it goes into a busy-wait loop until the timer expires.
 *
 * The caller must ensure that workqueue_struct on which this work was last
 * queued can't be destroyed before this function returns.
 */
int cancel_work_sync(struct work_struct *work)
{
	return __cancel_work_timer(work, NULL);
}
EXPORT_SYMBOL_GPL(cancel_work_sync);

/**
 * cancel_delayed_work_sync - reliably kill off a delayed work.
 * @dwork: the delayed work struct
 *
 * Returns true if @dwork was pending.
 *
 * It is possible to use this function if @dwork rearms itself via queue_work()
 * or queue_delayed_work(). See also the comment for cancel_work_sync().
 */
int cancel_delayed_work_sync(struct delayed_work *dwork)
{
	return __cancel_work_timer(&dwork->work, &dwork->timer);
}
EXPORT_SYMBOL(cancel_delayed_work_sync);

static struct workqueue_struct *keventd_wq __read_mostly;

/**
 * schedule_work - put work task in global workqueue
 * @work: job to be done
 *
 * Returns zero if @work was already on the kernel-global workqueue and
 * non-zero otherwise.
 *
 * This puts a job in the kernel-global workqueue if it was not already
 * queued and leaves it in the same position on the kernel-global
 * workqueue otherwise.
 */
 /* 提交工作节点,queue_work的包装函数,,queue_work的包装函数
  * 驱动程序如果使用内核创建的工作队列只需调用schedule_workqueue就可以了*/
int schedule_work(struct work_struct *work)
{
	return queue_work(keventd_wq, work);
}
EXPORT_SYMBOL(schedule_work);

/*
 * schedule_work_on - put work task on a specific cpu
 * @cpu: cpu to put the work task on
 * @work: job to be done
 *
 * This puts a job on a specific cpu
 */
int schedule_work_on(int cpu, struct work_struct *work)
{
	return queue_work_on(cpu, keventd_wq, work);
}
EXPORT_SYMBOL(schedule_work_on);

/**
 * schedule_delayed_work - put work task in global workqueue after delay
 * @dwork: job to be done
 * @delay: number of jiffies to wait or 0 for immediate execution
 *
 * After waiting for a given time this puts a job in the kernel-global
 * workqueue.
 */
 /*对于内核创建的工作队列的延迟提交函数;对应queue_delayed_work*/
int schedule_delayed_work(struct delayed_work *dwork,
					unsigned long delay)
{
	return queue_delayed_work(keventd_wq, dwork, delay);
}
EXPORT_SYMBOL(schedule_delayed_work);

/**
 * flush_delayed_work - block until a dwork_struct's callback has terminated
 * @dwork: the delayed work which is to be flushed
 *
 * Any timeout is cancelled, and any pending work is run immediately.
 */
void flush_delayed_work(struct delayed_work *dwork)
{
	if (del_timer_sync(&dwork->timer)) {
		struct cpu_workqueue_struct *cwq;
		cwq = wq_per_cpu(keventd_wq, get_cpu());
		__queue_work(cwq, &dwork->work);
		put_cpu();
	}
	flush_work(&dwork->work);
}
EXPORT_SYMBOL(flush_delayed_work);

/**
 * schedule_delayed_work_on - queue work in global workqueue on CPU after delay
 * @cpu: cpu to use
 * @dwork: job to be done
 * @delay: number of jiffies to wait
 *
 * After waiting for a given time this puts a job in the kernel-global
 * workqueue on the specified CPU.
 */
int schedule_delayed_work_on(int cpu,
			struct delayed_work *dwork, unsigned long delay)
{
	return queue_delayed_work_on(cpu, keventd_wq, dwork, delay);
}
EXPORT_SYMBOL(schedule_delayed_work_on);

/**
 * schedule_on_each_cpu - call a function on each online CPU from keventd
 * @func: the function to call
 *
 * Returns zero on success.
 * Returns -ve errno on failure.
 *
 * schedule_on_each_cpu() is very slow.
 */
int schedule_on_each_cpu(work_func_t func)
{
	int cpu;
	int orig = -1;
	struct work_struct *works;

	works = alloc_percpu(struct work_struct);
	if (!works)
		return -ENOMEM;

	get_online_cpus();

	/*
	 * When running in keventd don't schedule a work item on
	 * itself.  Can just call directly because the work queue is
	 * already bound.  This also is faster.
	 */
	if (current_is_keventd())
		orig = raw_smp_processor_id();

	for_each_online_cpu(cpu) {
		struct work_struct *work = per_cpu_ptr(works, cpu);

		INIT_WORK(work, func);
		if (cpu != orig)
			schedule_work_on(cpu, work);
	}
	if (orig >= 0)
		func(per_cpu_ptr(works, orig));

	for_each_online_cpu(cpu)
		flush_work(per_cpu_ptr(works, cpu));

	put_online_cpus();
	free_percpu(works);
	return 0;
}

void flush_scheduled_work(void)
{
	flush_workqueue(keventd_wq);
}
EXPORT_SYMBOL(flush_scheduled_work);

/**
 * execute_in_process_context - reliably execute the routine with user context
 * @fn:		the function to execute
 * @ew:		guaranteed storage for the execute work structure (must
 *		be available when the work executes)
 *
 * Executes the function immediately if process context is available,
 * otherwise schedules the function for delayed execution.
 *
 * Returns:	0 - function was executed
 *		1 - function was scheduled for execution
 */
int execute_in_process_context(work_func_t fn, struct execute_work *ew)
{
	if (!in_interrupt()) {
		fn(&ew->work);
		return 0;
	}

	INIT_WORK(&ew->work, fn);
	schedule_work(&ew->work);

	return 1;
}
EXPORT_SYMBOL_GPL(execute_in_process_context);

int keventd_up(void)
{
	return keventd_wq != NULL;
}

int current_is_keventd(void)
{
	struct cpu_workqueue_struct *cwq;
	int cpu = raw_smp_processor_id(); /* preempt-safe: keventd is per-cpu */
	int ret = 0;

	BUG_ON(!keventd_wq);

	cwq = per_cpu_ptr(keventd_wq->cpu_wq, cpu);
	if (current == cwq->thread)
		ret = 1;

	return ret;

}
EXPORT_SYMBOL_GPL(current_is_keventd);

static struct cpu_workqueue_struct *
init_cpu_workqueue(struct workqueue_struct *wq, int cpu)
{
	/*获得系统中第一个cpu对应的CPU工作队列管理结构指针cwq*/
	struct cpu_workqueue_struct *cwq = per_cpu_ptr(wq->cpu_wq, cpu);

	/*初始化cwq中的等待队列和双向链表等成员变量*/
	cwq->wq = wq;
	spin_lock_init(&cwq->lock);
	INIT_LIST_HEAD(&cwq->worklist);
	init_waitqueue_head(&cwq->more_work);

	return cwq;
}

/*生成一个工人线程,在linux内核中内核线程其实是一个进程*/
static int create_workqueue_thread(struct cpu_workqueue_struct *cwq, int cpu)
{
	struct sched_param param = { .sched_priority = MAX_RT_PRIO-1 };
	struct workqueue_struct *wq = cwq->wq;
	const char *fmt = is_wq_single_threaded(wq) ? "%s" : "%s/%d";
	struct task_struct *p;

	/*将进程task_struct中保存有进程执行现场寄存器的pc值指向worker_thread函数,这样当该进程被调度运行时将执行work_thread函数,传给函数的参数时系统中第一个cpu上的cwq指针*/
	p = kthread_create(worker_thread, cwq, fmt, wq->name, cpu);
	/*
	 * Nobody can add the work_struct to this cwq,
	 *	if (caller is __create_workqueue)
	 *		nobody should see this wq
	 *	else // caller is CPU_UP_PREPARE
	 *		cpu is not on cpu_online_map
	 * so we can abort safely.
	 */
	if (IS_ERR(p))
		return PTR_ERR(p);
	if (cwq->wq->rt)
		sched_setscheduler_nocheck(p, SCHED_FIFO, &param);
	/*新进程的task_struct结构体指针p将保存在cpu工作队列管理结构的thread成员中*/
	cwq->thread = p;

	trace_workqueue_creation(cwq->thread, cpu);

	return 0;
}

static void start_workqueue_thread(struct cpu_workqueue_struct *cwq, int cpu)
{
	struct task_struct *p = cwq->thread;

	if (p != NULL) {
		if (cpu >= 0)
			kthread_bind(p, cpu);
		/*将新进程投入到系统的运行队列中, 如此之后新进程就具备了被调度运行的条件*/
		wake_up_process(p);
	}
}

struct workqueue_struct *__create_workqueue_key(const char *name,
						int singlethread,
						int freezeable,
						int rt,
						struct lock_class_key *key,
						const char *lock_name)
{
	struct workqueue_struct *wq;
	struct cpu_workqueue_struct *cwq;
	int err = 0, cpu;

	/*生成工作队列管理结构的对象wq并初始化*/
	wq = kzalloc(sizeof(*wq), GFP_KERNEL);
	if (!wq)
		return NULL;

	/*alloc_percpu函数生成了per_CPU类型的CPU工作队列管理结构对象*/
	wq->cpu_wq = alloc_percpu(struct cpu_workqueue_struct);
	if (!wq->cpu_wq) {
		kfree(wq);
		return NULL;
	}

	/*初始化工作队列管理结构wq*/
	wq->name = name;
	lockdep_init_map(&wq->lockdep_map, lock_name, key, 0);
	wq->singlethread = singlethread;
	wq->freezeable = freezeable;
	wq->rt = rt;
	INIT_LIST_HEAD(&wq->list);

	/*根据singlethread的值对单线程队列和多线程队列分别进行处理*/
	if (singlethread) {
		/*singlethread = 1*/

		/*获得系统中第一个cpu对应的cpu工作队列管理结构指针*/
		cwq = init_cpu_workqueue(wq, singlethread_cpu);
		/*生成工人线程(linux内核中的线程其实是一个进程,拥有独立的task_struct) 单线程的  singlethread = 1*/
		err = create_workqueue_thread(cwq, singlethread_cpu);
		start_workqueue_thread(cwq, -1);
	} else {
		/*singlethread != 1*/

		cpu_maps_update_begin();
		/*
		 * We must place this wq on list even if the code below fails.
		 * cpu_down(cpu) can remove cpu from cpu_populated_map before
		 * destroy_workqueue() takes the lock, in that case we leak
		 * cwq[cpu]->thread.
		 */
		spin_lock(&workqueue_lock);
		/* 工作队列管理结构对象wq还将把自己加入到
		 * workqueues管理的链表中,workqueues是一个全局型的双向链表对象,用来链接系统中所有非singlethread的工作队列 */
		list_add(&wq->list, &workqueues);
		spin_unlock(&workqueue_lock);
		/*
		 * We must initialize cwqs for each possible cpu even if we
		 * are going to call destroy_workqueue() finally. Otherwise
		 * cpu_up() can hit the uninitialized cwq once we drop the
		 * lock.
		 */
		/*获得系统中第一个cpu对应的cpu工作队列管理结构指针*/
		for_each_possible_cpu(cpu) {
			cwq = init_cpu_workqueue(wq, cpu);
			if (err || !cpu_online(cpu))
				continue;

			/* 生成工人线程,singlethread != 1; 函数对系统中的每个
			 * cpu调用singlethread中的三个步骤,这样么个cpu都将拥有
			 * 自己的cpu工作队列管理结构和工作在其上的工人线程,这
			 * 种情况下,工作队列管理结构对象wq还将把自己加入到
			 * workqueues管理的链表中 */
			err = create_workqueue_thread(cwq, cpu);
			start_workqueue_thread(cwq, cpu);
		}
		cpu_maps_update_done();
	}

	if (err) {
		destroy_workqueue(wq);
		wq = NULL;
	}
	return wq;
}
EXPORT_SYMBOL_GPL(__create_workqueue_key);

/* 安全的终结worker_thread,因为destroy_workqueue被调用的时候,
 * worker_thread很有可能正在处理worklist中余下的工作节点*/
static void cleanup_workqueue_thread(struct cpu_workqueue_struct *cwq)
{
	/*
	 * Our caller is either destroy_workqueue() or CPU_POST_DEAD,
	 * cpu_add_remove_lock protects cwq->thread.
	 */
	if (cwq->thread == NULL)
		return;

	lock_map_acquire(&cwq->wq->lockdep_map);
	lock_map_release(&cwq->wq->lockdep_map);

	/*终止worker_thread所在进程的一个前提时要确保所有提交到cwq->worklist中
	 * 的工作节点都已经处理完毕,由此函数完成*/
	flush_cpu_workqueue(cwq);
	/*
	 * If the caller is CPU_POST_DEAD and cwq->worklist was not empty,
	 * a concurrent flush_workqueue() can insert a barrier after us.
	 * However, in that case run_workqueue() won't return and check
	 * kthread_should_stop() until it flushes all work_struct's.
	 * When ->worklist becomes empty it is safe to exit because no
	 * more work_structs can be queued on this cwq: flush_workqueue
	 * checks list_empty(), and a "normal" queue_work() can't use
	 * a dead CPU.
	 */
	trace_workqueue_destruction(cwq->thread);
	/*让worker_thread所在的进程终止，因为一旦进程的执行函数worker_thread
	 * 结束，进程将调用do_exit而终结，所以kthread_stop让worker_thread结束
	 * 的原理就是设置should_stop = 1*/
	kthread_stop(cwq->thread);
	cwq->thread = NULL;
}

/**
 * destroy_workqueue - safely terminate a workqueue
 * @wq: target workqueue
 *
 * Safely destroy a workqueue. All work currently pending will be done first.
 */
 /*函数执行与create_singlethread_workqueue/create_workqueue相反的任务,当驱动程序不再需要后者创建的工作队列时(比如驱动程序模块从系统中移走或者关闭设备),需要调用该函数来做工作队列的清理善后工作*/
void destroy_workqueue(struct workqueue_struct *wq)
{
	const struct cpumask *cpu_map = wq_cpu_map(wq);
	int cpu;

	cpu_maps_update_begin();
	spin_lock(&workqueue_lock);
	list_del(&wq->list);
	spin_unlock(&workqueue_lock);

	for_each_cpu(cpu, cpu_map)
		/*安全的终结worker_thread,因为destroy_workqueue被调用的时候,
		 * worker_thread很有可能正在处理worklist中余下的工作节点*/
		cleanup_workqueue_thread(per_cpu_ptr(wq->cpu_wq, cpu));
 	cpu_maps_update_done();

	free_percpu(wq->cpu_wq);
	kfree(wq);
}
EXPORT_SYMBOL_GPL(destroy_workqueue);

static int __devinit workqueue_cpu_callback(struct notifier_block *nfb,
						unsigned long action,
						void *hcpu)
{
	unsigned int cpu = (unsigned long)hcpu;
	struct cpu_workqueue_struct *cwq;
	struct workqueue_struct *wq;
	int ret = NOTIFY_OK;

	action &= ~CPU_TASKS_FROZEN;

	switch (action) {
	case CPU_UP_PREPARE:
		cpumask_set_cpu(cpu, cpu_populated_map);
	}
undo:
	list_for_each_entry(wq, &workqueues, list) {
		cwq = per_cpu_ptr(wq->cpu_wq, cpu);

		switch (action) {
		case CPU_UP_PREPARE:
			if (!create_workqueue_thread(cwq, cpu))
				break;
			printk(KERN_ERR "workqueue [%s] for %i failed\n",
				wq->name, cpu);
			action = CPU_UP_CANCELED;
			ret = NOTIFY_BAD;
			goto undo;

		case CPU_ONLINE:
			start_workqueue_thread(cwq, cpu);
			break;

		case CPU_UP_CANCELED:
			start_workqueue_thread(cwq, -1);
		case CPU_POST_DEAD:
			cleanup_workqueue_thread(cwq);
			break;
		}
	}

	switch (action) {
	case CPU_UP_CANCELED:
	case CPU_POST_DEAD:
		cpumask_clear_cpu(cpu, cpu_populated_map);
	}

	return ret;
}

#ifdef CONFIG_SMP

struct work_for_cpu {
	struct completion completion;
	long (*fn)(void *);
	void *arg;
	long ret;
};

static int do_work_for_cpu(void *_wfc)
{
	struct work_for_cpu *wfc = _wfc;
	wfc->ret = wfc->fn(wfc->arg);
	complete(&wfc->completion);
	return 0;
}

/**
 * work_on_cpu - run a function in user context on a particular cpu
 * @cpu: the cpu to run on
 * @fn: the function to run
 * @arg: the function arg
 *
 * This will return the value @fn returns.
 * It is up to the caller to ensure that the cpu doesn't go offline.
 * The caller must not hold any locks which would prevent @fn from completing.
 */
long work_on_cpu(unsigned int cpu, long (*fn)(void *), void *arg)
{
	struct task_struct *sub_thread;
	struct work_for_cpu wfc = {
		.completion = COMPLETION_INITIALIZER_ONSTACK(wfc.completion),
		.fn = fn,
		.arg = arg,
	};

	sub_thread = kthread_create(do_work_for_cpu, &wfc, "work_for_cpu");
	if (IS_ERR(sub_thread))
		return PTR_ERR(sub_thread);
	kthread_bind(sub_thread, cpu);
	wake_up_process(sub_thread);
	wait_for_completion(&wfc.completion);
	return wfc.ret;
}
EXPORT_SYMBOL_GPL(work_on_cpu);
#endif /* CONFIG_SMP */

/**/
void __init init_workqueues(void)
{
	alloc_cpumask_var(&cpu_populated_map, GFP_KERNEL);

	cpumask_copy(cpu_populated_map, cpu_online_mask);
	singlethread_cpu = cpumask_first(cpu_possible_mask);
	cpu_singlethread_map = cpumask_of(singlethread_cpu);
	hotcpu_notifier(workqueue_cpu_callback, 0);
	/*初始化阶段创建一个名为events的工作队列*/
	keventd_wq = create_workqueue("events");
	BUG_ON(!keventd_wq);
}
