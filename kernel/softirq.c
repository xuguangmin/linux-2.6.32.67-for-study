/*
 *	linux/kernel/softirq.c
 *
 *	Copyright (C) 1992 Linus Torvalds
 *
 *	Distribute under GPLv2.
 *
 *	Rewritten. Old one was good in 2.2, but in 2.3 it was immoral. --ANK (990903)
 *
 *	Remote softirq infrastructure is by Jens Axboe.
 */

#include <linux/module.h>
#include <linux/kernel_stat.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/notifier.h>
#include <linux/percpu.h>
#include <linux/cpu.h>
#include <linux/freezer.h>
#include <linux/kthread.h>
#include <linux/rcupdate.h>
#include <linux/ftrace.h>
#include <linux/smp.h>
#include <linux/tick.h>

#define CREATE_TRACE_POINTS
#include <trace/events/irq.h>

#include <asm/irq.h>
/*
   - No shared variables, all the data are CPU local.
   - If a softirq needs serialization, let it serialize itself
     by its own spinlocks.
   - Even if softirq is serialized, only local cpu is marked for
     execution. Hence, we get something sort of weak cpu binding.
     Though it is still not clear, will it result in better locality
     or will not.

   Examples:
   - NET RX softirq. It is multithreaded and does not require
     any global serialization.
   - NET TX softirq. It kicks software netdevice queues, hence
     it is logically serialized per device, but this serialization
     is invisible to common code.
   - Tasklets: serialized wrt itself.
 */

#ifndef __ARCH_IRQ_STAT
irq_cpustat_t irq_stat[NR_CPUS] ____cacheline_aligned;
EXPORT_SYMBOL(irq_stat);
#endif

/*数组中每一项,对应一个软件中断处理函数指针,在中断处理的SOFTIRQ部分,如果发现本地CPU的_softirq_pending上TASKLET_SOFTIRQ或者HI_SOFTIRQ位被置1,就将调用tasklet_action或者tasklet_hi_action*/
static struct softirq_action softirq_vec[NR_SOFTIRQS] __cacheline_aligned_in_smp;

static DEFINE_PER_CPU(struct task_struct *, ksoftirqd);

char *softirq_to_name[NR_SOFTIRQS] = {
	"HI", "TIMER", "NET_TX", "NET_RX", "BLOCK", "BLOCK_IOPOLL",
	"TASKLET", "SCHED", "HRTIMER",	"RCU"
};

/*
 * we cannot loop indefinitely here to avoid userspace starvation,
 * but we also don't want to introduce a worst case 1/HZ latency
 * to the pending events, so lets the scheduler to balance
 * the softirq load for us.
 */
void wakeup_softirqd(void)
{
	/* Interrupts are disabled: no need to stop preemption */
	struct task_struct *tsk = __get_cpu_var(ksoftirqd);

	if (tsk && tsk->state != TASK_RUNNING)
		wake_up_process(tsk);//����ksoftirqd
}

/*
 * preempt_count and SOFTIRQ_OFFSET usage:
 * - preempt_count is changed by SOFTIRQ_OFFSET on entering or leaving
 *   softirq processing.
 * - preempt_count is changed by SOFTIRQ_DISABLE_OFFSET (= 2 * SOFTIRQ_OFFSET)
 *   on local_bh_disable or local_bh_enable.
 * This lets us distinguish between whether we are currently processing
 * softirq and whether we just have bh disabled.
 */

/*
 * This one is for softirq.c-internal use,
 * where hardirqs are disabled legitimately:
 */
#ifdef CONFIG_TRACE_IRQFLAGS
static void __local_bh_disable(unsigned long ip, unsigned int cnt)
{
	unsigned long flags;

	WARN_ON_ONCE(in_irq());

	raw_local_irq_save(flags);
	/*
	 * The preempt tracer hooks into add_preempt_count and will break
	 * lockdep because it calls back into lockdep after SOFTIRQ_OFFSET
	 * is set and before current->softirq_enabled is cleared.
	 * We must manually increment preempt_count here and manually
	 * call the trace_preempt_off later.
	 */
	preempt_count() += cnt;
	/*
	 * Were softirqs turned off above:
	 */
	if (softirq_count() == cnt)
		trace_softirqs_off(ip);
	raw_local_irq_restore(flags);

	if (preempt_count() == cnt)
		trace_preempt_off(CALLER_ADDR0, get_parent_ip(CALLER_ADDR1));
}
#else /* !CONFIG_TRACE_IRQFLAGS */
/*用来在preempt_count()上表示SOFTIRQ的上下文,考虑到SOFTIRQ执行过程可能会被外部中断的情况,这可防止SOFTIRQ部分的重入,因为只有在非interrupt的上下文中才可以进入到SOFTIRQ*/
static inline void __local_bh_disable(unsigned long ip, unsigned int cnt)
{
	add_preempt_count(cnt);
	barrier();
}
#endif /* CONFIG_TRACE_IRQFLAGS */

void local_bh_disable(void)
{
	__local_bh_disable((unsigned long)__builtin_return_address(0),
				SOFTIRQ_DISABLE_OFFSET);
}

EXPORT_SYMBOL(local_bh_disable);

static void __local_bh_enable(unsigned int cnt)
{
	WARN_ON_ONCE(in_irq());
	WARN_ON_ONCE(!irqs_disabled());

	if (softirq_count() == cnt)
		trace_softirqs_on((unsigned long)__builtin_return_address(0));
	sub_preempt_count(cnt);
}

/*
 * Special-case - softirqs can safely be enabled in
 * cond_resched_softirq(), or by __do_softirq(),
 * without processing still-pending softirqs:
 */
void _local_bh_enable(void)
{
	__local_bh_enable(SOFTIRQ_DISABLE_OFFSET);
}

EXPORT_SYMBOL(_local_bh_enable);

static inline void _local_bh_enable_ip(unsigned long ip)
{
	WARN_ON_ONCE(in_irq() || irqs_disabled());
#ifdef CONFIG_TRACE_IRQFLAGS
	local_irq_disable();
#endif
	/*
	 * Are softirqs going to be turned on now:
	 */
	if (softirq_count() == SOFTIRQ_DISABLE_OFFSET)
		trace_softirqs_on(ip);
	/*
	 * Keep preemption disabled until we are done with
	 * softirq processing:
 	 */
	sub_preempt_count(SOFTIRQ_DISABLE_OFFSET - 1);

	if (unlikely(!in_interrupt() && local_softirq_pending()))
		do_softirq();

	dec_preempt_count();
#ifdef CONFIG_TRACE_IRQFLAGS
	local_irq_enable();
#endif
	preempt_check_resched();
}

void local_bh_enable(void)
{
	_local_bh_enable_ip((unsigned long)__builtin_return_address(0));
}
EXPORT_SYMBOL(local_bh_enable);

void local_bh_enable_ip(unsigned long ip)
{
	_local_bh_enable_ip(ip);
}
EXPORT_SYMBOL(local_bh_enable_ip);

/*
 * We restart softirq processing for at most MAX_SOFTIRQ_RESTART times,
 * but break the loop if need_resched() is set or after 2 ms.
 * The MAX_SOFTIRQ_TIME provides a nice upper bound in most cases, but in
 * certain cases, such as stop_machine(), jiffies may cease to
 * increment and so we need the MAX_SOFTIRQ_RESTART limit as
 * well to make sure we eventually return from this method.
 *
 * These limits have been established via experimentation.
 * The two things to balance is latency against fairness -
 * we want to handle softirqs as soon as possible, but they
 * should not be able to lock up the box.
 */
#define MAX_SOFTIRQ_TIME  msecs_to_jiffies(2)
#define MAX_SOFTIRQ_RESTART 10

/*函数的核心思想是:从CPU本地的_softirq_pending的最低位开始,依次往高位扫描,如果发现某位为1,说明对应该位有个等待中的softirq需要处理,那么就调用soft_vec数组中对应项的action函数,这个过程会一直持续下去,知道__softirq_pending为0*/
asmlinkage void __do_softirq(void)
{
	struct softirq_action *h;
	__u32 pending;
	unsigned long end = jiffies + MAX_SOFTIRQ_TIME;
	int cpu;
	int max_restart = MAX_SOFTIRQ_RESTART;

	pending = local_softirq_pending();
	account_system_vtime(current);

	__local_bh_disable((unsigned long)__builtin_return_address(0),
				SOFTIRQ_OFFSET);
	lockdep_softirq_enter();

	cpu = smp_processor_id();
restart:
	/* Reset the pending bitmask before enabling irqs */
	set_softirq_pending(0);

	local_irq_enable();

	h = softirq_vec;

	do {
		if (pending & 1) {
			int prev_count = preempt_count();
			kstat_incr_softirqs_this_cpu(h - softirq_vec);

			trace_softirq_entry(h, softirq_vec);
			h->action(h);
			trace_softirq_exit(h, softirq_vec);
			if (unlikely(prev_count != preempt_count())) {
				printk(KERN_ERR "huh, entered softirq %td %s %p"
				       "with preempt_count %08x,"
				       " exited with %08x?\n", h - softirq_vec,
				       softirq_to_name[h - softirq_vec],
				       h->action, prev_count, preempt_count());
				preempt_count() = prev_count;
			}

			rcu_bh_qs(cpu);
		}
		h++;
		pending >>= 1;
	} while (pending);

	local_irq_disable();

	pending = local_softirq_pending();
	if (pending) {
		if (time_before(jiffies, end) && !need_resched() &&
		    --max_restart)
			goto restart;

		//�Ѿ�ִ�е����ж��ֱ��������˳�__do_softirq��Ϊ��ƽ���������
		wakeup_softirqd();// �����ں��߳�
	}

	lockdep_softirq_exit();

	account_system_vtime(current);
	__local_bh_enable(SOFTIRQ_OFFSET);
}

#ifndef __ARCH_HAS_DO_SOFTIRQ

asmlinkage void do_softirq(void)
{
	__u32 pending;
	unsigned long flags;

	if (in_interrupt())
		return;

	local_irq_save(flags);

	pending = local_softirq_pending();

	if (pending)
		__do_softirq();

	local_irq_restore(flags);
}

#endif

/*
 * Enter an interrupt context.
 */
/*HARDIRQ部分的开始,告诉系统进入冲断处理的上半部分，与irq_enter对应的是irq_exit,irq_enter会更新系统中的一些统计量，同时会把当前栈中的preemtp_count变量加上HARDIRQ_OFFSET来标识一个HARDIRQ中上下文*/
void irq_enter(void)
{
	int cpu = smp_processor_id();

	rcu_irq_enter();
	if (idle_cpu(cpu) && !in_interrupt()) {
		/*
		 * Prevent raise_softirq from needlessly waking up ksoftirqd
		 * here, as softirq will be serviced on return from interrupt.
		 */
		local_bh_disable();
		tick_check_idle(cpu);
		_local_bh_enable();
	}

	__irq_enter();
}

/*__ARCH_IRQ_EXIT_IRQS_DISABLED是个体系架构相关的宏,用来决定在HARDIRQ部分
 * 结束时有没有关闭处理器响应外部中断的能力,如果定义了__ARCH_IRQ_EXIT_IRQS_DISABLED,就意味着在处理SOFTIRQ部分时,可以保证外部中断已经关闭,此时可以直接调用_do_softire,不过之前要做一些中断屏蔽的事情,保证_do_softirq开始执行时中断是关闭的*/
#ifdef __ARCH_IRQ_EXIT_IRQS_DISABLED
#define invoke_softirq()	__do_softirq()
#else
/*invoke_softirq是真正处理SOFTIRQ部分的函数*/
#define invoke_softirq()	do_softirq()
#endif

/*
 * Exit an interrupt context. Process softirqs if needed and possible:
 */
 /*中断下半部SOFTIRQ*/
void irq_exit(void)
{
	account_system_vtime(current);
	trace_hardirq_exit();
	/*函数把当前栈中的preempt_count变量减去IRQ_EXIT_OFFSET来标识HARDIRQ中断上下文的结束,这步动作对应do_IRQ中的irq_enter*/
	sub_preempt_count(IRQ_EXIT_OFFSET);
	/* invoke_softirq是真正处理SOFTIRQ部分的函数,这个函数的前提是if中的
	 * 两个条件:in_interrupt和local_softirq_pending;invoke_softirq被调用的前提是:当前不在interrupt上下文中而且__softirq_pending中有等待的softirq.当前不再interrupt保证了如果代码正在SOFTIRQ部分执行时,如果发生了一个外部中断,那么在中断处理函数结束HARDIRQ部分时,将不会处理softirq,而是直接返回,这样此前被中断的SOFTIRQ部分将继续执行*/
	if (!in_interrupt() && local_softirq_pending())
		invoke_softirq();

#ifdef CONFIG_NO_HZ
	/* Make sure that timer wheel updates are propagated */
	rcu_irq_exit();
	if (idle_cpu(smp_processor_id()) && !in_interrupt() && !need_resched())
		tick_nohz_stop_sched_tick(0);
#endif
	preempt_enable_no_resched();
}

/*
 * This function must run with irqs disabled!
 * �������жϣ�nrΪ���ж��±�
 */
inline void raise_softirq_irqoff(unsigned int nr)
{
	__raise_softirq_irqoff(nr);//�����жϱ��Ϊ����״̬

	/*
	 * If we're in an interrupt or softirq, we're done
	 * (this also catches softirq-disabled code). We will
	 * actually run the softirq once we return from
	 * the irq or softirq.
	 *
	 * Otherwise we wake up ksoftirqd to make sure we
	 * schedule the softirq soon.
	 */
	if (!in_interrupt())// ���preempt_count�ֶε�Ӳ�жϼ����������ն˼�������ֻҪ�����������һ��Ϊ�������Ͳ���һ����0ֵ
		wakeup_softirqd();
}

void raise_softirq(unsigned int nr)
{
	unsigned long flags;

	local_irq_save(flags);
	raise_softirq_irqoff(nr);
	local_irq_restore(flags);
}

/*用来给TASKLET_SOFTIRQ和HI_SOFTIRQ安装对应的执行函数*/
void open_softirq(int nr, void (*action)(struct softirq_action *))
{
	softirq_vec[nr].action = action;
}

/*
 * Tasklets
 * tasklet_vec的声明
 */
struct tasklet_head
{
	struct tasklet_struct *head;	/*head总是指向tasklet对象链表的第一个节点*/
	struct tasklet_struct **tail;	/*这是个指向tasklet对象指针的指针,tail总是保存tasklet链表最后一个节点所在tasklet对象中next成员的地址*/
};

/*
 *    定义tasklet_vec, 
 *    定义tasklet_hi_vec
 */
 /* struct tasklet_head per_cpu_tasklet_vec;
   * percpu_defs.h 
   */
static DEFINE_PER_CPU(struct tasklet_head, tasklet_vec);//����tasklet
static DEFINE_PER_CPU(struct tasklet_head, tasklet_hi_vec);// �����ȼ�tasklet

void __tasklet_schedule(struct tasklet_struct *t)
{
	unsigned long flags;

	local_irq_save(flags);
	t->next = NULL;
	*__get_cpu_var(tasklet_vec).tail = t;
	__get_cpu_var(tasklet_vec).tail = &(t->next);
	raise_softirq_irqoff(TASKLET_SOFTIRQ);
	local_irq_restore(flags);
}

EXPORT_SYMBOL(__tasklet_schedule);

void __tasklet_hi_schedule(struct tasklet_struct *t)
{
	unsigned long flags;

	local_irq_save(flags);
	t->next = NULL;
	*__get_cpu_var(tasklet_hi_vec).tail = t;
	__get_cpu_var(tasklet_hi_vec).tail = &(t->next);
	raise_softirq_irqoff(HI_SOFTIRQ);
	local_irq_restore(flags);
}

EXPORT_SYMBOL(__tasklet_hi_schedule);

void __tasklet_hi_schedule_first(struct tasklet_struct *t)
{
	BUG_ON(!irqs_disabled());

	t->next = __get_cpu_var(tasklet_hi_vec).head;
	__get_cpu_var(tasklet_hi_vec).head = t;
	__raise_softirq_irqoff(HI_SOFTIRQ);
}

EXPORT_SYMBOL(__tasklet_hi_schedule_first);

/* ����tasklet ���жϴ�������ͨ��open_softirq������װ */
static void tasklet_action(struct softirq_action *a)
{
	struct tasklet_struct *list;

	local_irq_disable();// ���ñ����ж�
	list = __get_cpu_var(tasklet_vec).head;//��tasklet_vec[n]ָ��������ַ����list
	__get_cpu_var(tasklet_vec).head = NULL;//����ѵ���tasklet������������
	__get_cpu_var(tasklet_vec).tail = &__get_cpu_var(tasklet_vec).head;
	local_irq_enable();// �ָ������ж�

	while (list) {
		struct tasklet_struct *t = list;

		list = list->next;

		if (tasklet_trylock(t)) {// page 182,���ͬ���͵�tasklet�Ƿ���������CPU��ִ��
			if (!atomic_read(&t->count)) {//�����������
				if (!test_and_clear_bit(TASKLET_STATE_SCHED, &t->state))//��TASKLET_STATE_SCHED
					BUG();
				t->func(t->data);//ִ��ע���tasklet������
				tasklet_unlock(t);
				continue;
			}
			tasklet_unlock(t);
		}
		/* �������������㣬���������������²���tasklet_vec[n]
		  * ָ��������У��������ٴμ���TASKLET_SOFTIRQ */ 
		local_irq_disable();
		t->next = NULL;
		*__get_cpu_var(tasklet_vec).tail = t;
		__get_cpu_var(tasklet_vec).tail = &(t->next);
		__raise_softirq_irqoff(TASKLET_SOFTIRQ);
		local_irq_enable();
	}
}

/*tasklet_hi_action,�ο�tasklet_action */
static void tasklet_hi_action(struct softirq_action *a)
{
	struct tasklet_struct *list;

	local_irq_disable();
	list = __get_cpu_var(tasklet_hi_vec).head;
	__get_cpu_var(tasklet_hi_vec).head = NULL;
	__get_cpu_var(tasklet_hi_vec).tail = &__get_cpu_var(tasklet_hi_vec).head;
	local_irq_enable();

	while (list) {
		struct tasklet_struct *t = list;

		list = list->next;

		if (tasklet_trylock(t)) {
			if (!atomic_read(&t->count)) {
				if (!test_and_clear_bit(TASKLET_STATE_SCHED, &t->state))
					BUG();
				t->func(t->data);
				tasklet_unlock(t);
				continue;
			}
			tasklet_unlock(t);
		}

		local_irq_disable();
		t->next = NULL;
		*__get_cpu_var(tasklet_hi_vec).tail = t;
		__get_cpu_var(tasklet_hi_vec).tail = &(t->next);
		__raise_softirq_irqoff(HI_SOFTIRQ);
		local_irq_enable();
	}
}


/*动态初始化tasklet对象*/
void tasklet_init(struct tasklet_struct *t,
		  void (*func)(unsigned long), unsigned long data)
{
	t->next = NULL;
	t->state = 0;
	atomic_set(&t->count, 0);
	t->func = func;
	t->data = data;
}

EXPORT_SYMBOL(tasklet_init);

void tasklet_kill(struct tasklet_struct *t)
{
	if (in_interrupt())
		printk("Attempt to kill tasklet from interrupt\n");

	while (test_and_set_bit(TASKLET_STATE_SCHED, &t->state)) {
		do {
			yield();
		} while (test_bit(TASKLET_STATE_SCHED, &t->state));
	}
	tasklet_unlock_wait(t);
	clear_bit(TASKLET_STATE_SCHED, &t->state);
}

EXPORT_SYMBOL(tasklet_kill);

/*
 * tasklet_hrtimer
 */

/*
 * The trampoline is called when the hrtimer expires. If this is
 * called from the hrtimer interrupt then we schedule the tasklet as
 * the timer callback function expects to run in softirq context. If
 * it's called in softirq context anyway (i.e. high resolution timers
 * disabled) then the hrtimer callback is called right away.
 */
static enum hrtimer_restart __hrtimer_tasklet_trampoline(struct hrtimer *timer)
{
	struct tasklet_hrtimer *ttimer =
		container_of(timer, struct tasklet_hrtimer, timer);

	if (hrtimer_is_hres_active(timer)) {
		tasklet_hi_schedule(&ttimer->tasklet);
		return HRTIMER_NORESTART;
	}
	return ttimer->function(timer);
}

/*
 * Helper function which calls the hrtimer callback from
 * tasklet/softirq context
 */
static void __tasklet_hrtimer_trampoline(unsigned long data)
{
	struct tasklet_hrtimer *ttimer = (void *)data;
	enum hrtimer_restart restart;

	restart = ttimer->function(&ttimer->timer);
	if (restart != HRTIMER_NORESTART)
		hrtimer_restart(&ttimer->timer);
}

/**
 * tasklet_hrtimer_init - Init a tasklet/hrtimer combo for softirq callbacks
 * @ttimer:	 tasklet_hrtimer which is initialized
 * @function:	 hrtimer callback funtion which gets called from softirq context
 * @which_clock: clock id (CLOCK_MONOTONIC/CLOCK_REALTIME)
 * @mode:	 hrtimer mode (HRTIMER_MODE_ABS/HRTIMER_MODE_REL)
 */
void tasklet_hrtimer_init(struct tasklet_hrtimer *ttimer,
			  enum hrtimer_restart (*function)(struct hrtimer *),
			  clockid_t which_clock, enum hrtimer_mode mode)
{
	hrtimer_init(&ttimer->timer, which_clock, mode);
	ttimer->timer.function = __hrtimer_tasklet_trampoline;
	tasklet_init(&ttimer->tasklet, __tasklet_hrtimer_trampoline,
		     (unsigned long)ttimer);
	ttimer->function = function;
}
EXPORT_SYMBOL_GPL(tasklet_hrtimer_init);

/*
 * Remote softirq bits
 */

DEFINE_PER_CPU(struct list_head [NR_SOFTIRQS], softirq_work_list);
EXPORT_PER_CPU_SYMBOL(softirq_work_list);

static void __local_trigger(struct call_single_data *cp, int softirq)
{
	struct list_head *head = &__get_cpu_var(softirq_work_list[softirq]);

	list_add_tail(&cp->list, head);

	/* Trigger the softirq only if the list was previously empty.  */
	if (head->next == &cp->list)
		raise_softirq_irqoff(softirq);
}

#ifdef CONFIG_USE_GENERIC_SMP_HELPERS
static void remote_softirq_receive(void *data)
{
	struct call_single_data *cp = data;
	unsigned long flags;
	int softirq;

	softirq = cp->priv;

	local_irq_save(flags);
	__local_trigger(cp, softirq);
	local_irq_restore(flags);
}

static int __try_remote_softirq(struct call_single_data *cp, int cpu, int softirq)
{
	if (cpu_online(cpu)) {
		cp->func = remote_softirq_receive;
		cp->info = cp;
		cp->flags = 0;
		cp->priv = softirq;

		__smp_call_function_single(cpu, cp, 0);
		return 0;
	}
	return 1;
}
#else /* CONFIG_USE_GENERIC_SMP_HELPERS */
static int __try_remote_softirq(struct call_single_data *cp, int cpu, int softirq)
{
	return 1;
}
#endif

/**
 * __send_remote_softirq - try to schedule softirq work on a remote cpu
 * @cp: private SMP call function data area
 * @cpu: the remote cpu
 * @this_cpu: the currently executing cpu
 * @softirq: the softirq for the work
 *
 * Attempt to schedule softirq work on a remote cpu.  If this cannot be
 * done, the work is instead queued up on the local cpu.
 *
 * Interrupts must be disabled.
 */
void __send_remote_softirq(struct call_single_data *cp, int cpu, int this_cpu, int softirq)
{
	if (cpu == this_cpu || __try_remote_softirq(cp, cpu, softirq))
		__local_trigger(cp, softirq);
}
EXPORT_SYMBOL(__send_remote_softirq);

/**
 * send_remote_softirq - try to schedule softirq work on a remote cpu
 * @cp: private SMP call function data area
 * @cpu: the remote cpu
 * @softirq: the softirq for the work
 *
 * Like __send_remote_softirq except that disabling interrupts and
 * computing the current cpu is done for the caller.
 */
void send_remote_softirq(struct call_single_data *cp, int cpu, int softirq)
{
	unsigned long flags;
	int this_cpu;

	local_irq_save(flags);
	this_cpu = smp_processor_id();
	__send_remote_softirq(cp, cpu, this_cpu, softirq);
	local_irq_restore(flags);
}
EXPORT_SYMBOL(send_remote_softirq);

static int __cpuinit remote_softirq_cpu_notify(struct notifier_block *self,
					       unsigned long action, void *hcpu)
{
	/*
	 * If a CPU goes away, splice its entries to the current CPU
	 * and trigger a run of the softirq
	 */
	if (action == CPU_DEAD || action == CPU_DEAD_FROZEN) {
		int cpu = (unsigned long) hcpu;
		int i;

		local_irq_disable();
		for (i = 0; i < NR_SOFTIRQS; i++) {
			struct list_head *head = &per_cpu(softirq_work_list[i], cpu);
			struct list_head *local_head;

			if (list_empty(head))
				continue;

			local_head = &__get_cpu_var(softirq_work_list[i]);
			list_splice_init(head, local_head);
			raise_softirq_irqoff(i);
		}
		local_irq_enable();
	}

	return NOTIFY_OK;
}

static struct notifier_block __cpuinitdata remote_softirq_cpu_notifier = {
	.notifier_call	= remote_softirq_cpu_notify,
};

/*系统初始化期间调用该函数为TASKLET_SOFTIRQ和HI_SOFTIRQ安装了执行函数*/
void __init softirq_init(void)
{
	int cpu;

	/*用来初始化管理tasklet链表的变量tasklet_vec和tasklet_hi_vec*/
	for_each_possible_cpu(cpu) {
		int i;

		per_cpu(tasklet_vec, cpu).tail =
			&per_cpu(tasklet_vec, cpu).head;
		per_cpu(tasklet_hi_vec, cpu).tail =
			&per_cpu(tasklet_hi_vec, cpu).head;
		for (i = 0; i < NR_SOFTIRQS; i++)
			INIT_LIST_HEAD(&per_cpu(softirq_work_list[i], cpu));
	}

	register_hotcpu_notifier(&remote_softirq_cpu_notifier);

	/*用来给TASKLET_SOFTIRQ和HI_SOFTIRQ安装对应的执行函数*/
	open_softirq(TASKLET_SOFTIRQ, tasklet_action);
	open_softirq(HI_SOFTIRQ, tasklet_hi_action);
}

static int ksoftirqd(void * __bind_cpu)
{
	set_current_state(TASK_INTERRUPTIBLE);

	current->flags |= PF_KSOFTIRQD;
	while (!kthread_should_stop()) {
		preempt_disable();
		if (!local_softirq_pending()) {
			preempt_enable_no_resched();
			schedule();
			preempt_disable();
		}

		__set_current_state(TASK_RUNNING);

		while (local_softirq_pending()) {// �ں��̱߳�����ʱ������ж�λ����
			/* Preempt disable stops cpu going offline.
			   If already offline, we'll be on wrong CPU:
			   don't process */
			if (cpu_is_offline((long)__bind_cpu))
				goto wait_to_die;
			do_softirq();
			preempt_enable_no_resched();
			cond_resched();//�����л�
			preempt_disable();
			rcu_sched_qs((long)__bind_cpu);
		} /* ���û�й�������жϣ��������õ�ǰ����״̬
		 TASK_INTERRUPTIBLE */
		preempt_enable();
		set_current_state(TASK_INTERRUPTIBLE);
	}
	__set_current_state(TASK_RUNNING);
	return 0;

wait_to_die:
	preempt_enable();
	/* Wait for kthread_stop */
	set_current_state(TASK_INTERRUPTIBLE);
	while (!kthread_should_stop()) {
		schedule();
		set_current_state(TASK_INTERRUPTIBLE);
	}
	__set_current_state(TASK_RUNNING);
	return 0;
}

#ifdef CONFIG_HOTPLUG_CPU
/*
 * tasklet_kill_immediate is called to remove a tasklet which can already be
 * scheduled for execution on @cpu.
 *
 * Unlike tasklet_kill, this function removes the tasklet
 * _immediately_, even if the tasklet is in TASKLET_STATE_SCHED state.
 *
 * When this function is called, @cpu must be in the CPU_DEAD state.
 */
void tasklet_kill_immediate(struct tasklet_struct *t, unsigned int cpu)
{
	struct tasklet_struct **i;

	BUG_ON(cpu_online(cpu));
	BUG_ON(test_bit(TASKLET_STATE_RUN, &t->state));

	if (!test_bit(TASKLET_STATE_SCHED, &t->state))
		return;

	/* CPU is dead, so no lock needed. */
	for (i = &per_cpu(tasklet_vec, cpu).head; *i; i = &(*i)->next) {
		if (*i == t) {
			*i = t->next;
			/* If this was the tail element, move the tail ptr */
			if (*i == NULL)
				per_cpu(tasklet_vec, cpu).tail = i;
			return;
		}
	}
	BUG();
}

static void takeover_tasklets(unsigned int cpu)
{
	/* CPU is dead, so no lock needed. */
	local_irq_disable();

	/* Find end, append list for that CPU. */
	if (&per_cpu(tasklet_vec, cpu).head != per_cpu(tasklet_vec, cpu).tail) {
		*(__get_cpu_var(tasklet_vec).tail) = per_cpu(tasklet_vec, cpu).head;
		__get_cpu_var(tasklet_vec).tail = per_cpu(tasklet_vec, cpu).tail;
		per_cpu(tasklet_vec, cpu).head = NULL;
		per_cpu(tasklet_vec, cpu).tail = &per_cpu(tasklet_vec, cpu).head;
	}
	raise_softirq_irqoff(TASKLET_SOFTIRQ);

	if (&per_cpu(tasklet_hi_vec, cpu).head != per_cpu(tasklet_hi_vec, cpu).tail) {
		*__get_cpu_var(tasklet_hi_vec).tail = per_cpu(tasklet_hi_vec, cpu).head;
		__get_cpu_var(tasklet_hi_vec).tail = per_cpu(tasklet_hi_vec, cpu).tail;
		per_cpu(tasklet_hi_vec, cpu).head = NULL;
		per_cpu(tasklet_hi_vec, cpu).tail = &per_cpu(tasklet_hi_vec, cpu).head;
	}
	raise_softirq_irqoff(HI_SOFTIRQ);

	local_irq_enable();
}
#endif /* CONFIG_HOTPLUG_CPU */

static int __cpuinit cpu_callback(struct notifier_block *nfb,
				  unsigned long action,
				  void *hcpu)
{
	int hotcpu = (unsigned long)hcpu;
	struct task_struct *p;

	switch (action) {
	case CPU_UP_PREPARE:
	case CPU_UP_PREPARE_FROZEN:
		p = kthread_create(ksoftirqd, hcpu, "ksoftirqd/%d", hotcpu);
		if (IS_ERR(p)) {
			printk("ksoftirqd for %i failed\n", hotcpu);
			return NOTIFY_BAD;
		}
		kthread_bind(p, hotcpu);
  		per_cpu(ksoftirqd, hotcpu) = p;
 		break;
	case CPU_ONLINE:
	case CPU_ONLINE_FROZEN:
		wake_up_process(per_cpu(ksoftirqd, hotcpu));
		break;
#ifdef CONFIG_HOTPLUG_CPU
	case CPU_UP_CANCELED:
	case CPU_UP_CANCELED_FROZEN:
		if (!per_cpu(ksoftirqd, hotcpu))
			break;
		/* Unbind so it can run.  Fall thru. */
		kthread_bind(per_cpu(ksoftirqd, hotcpu),
			     cpumask_any(cpu_online_mask));
	case CPU_DEAD:
	case CPU_DEAD_FROZEN: {
		struct sched_param param = { .sched_priority = MAX_RT_PRIO-1 };

		p = per_cpu(ksoftirqd, hotcpu);
		per_cpu(ksoftirqd, hotcpu) = NULL;
		sched_setscheduler_nocheck(p, SCHED_FIFO, &param);
		kthread_stop(p);
		takeover_tasklets(hotcpu);
		break;
	}
#endif /* CONFIG_HOTPLUG_CPU */
 	}
	return NOTIFY_OK;
}

static struct notifier_block __cpuinitdata cpu_nfb = {
	.notifier_call = cpu_callback
};

static __init int spawn_ksoftirqd(void)
{
	void *cpu = (void *)(long)smp_processor_id();
	int err = cpu_callback(&cpu_nfb, CPU_UP_PREPARE, cpu);

	BUG_ON(err == NOTIFY_BAD);
	cpu_callback(&cpu_nfb, CPU_ONLINE, cpu);
	register_cpu_notifier(&cpu_nfb);
	return 0;
}
early_initcall(spawn_ksoftirqd);

#ifdef CONFIG_SMP
/*
 * Call a function on all processors
 */
int on_each_cpu(void (*func) (void *info), void *info, int wait)
{
	int ret = 0;

	preempt_disable();
	ret = smp_call_function(func, info, wait);
	local_irq_disable();
	func(info);
	local_irq_enable();
	preempt_enable();
	return ret;
}
EXPORT_SYMBOL(on_each_cpu);
#endif

/*
 * [ These __weak aliases are kept in a separate compilation unit, so that
 *   GCC does not inline them incorrectly. ]
 */

int __init __weak early_irq_init(void)
{
	return 0;
}

int __init __weak arch_probe_nr_irqs(void)
{
	return 0;
}

int __init __weak arch_early_irq_init(void)
{
	return 0;
}

int __weak arch_init_chip_data(struct irq_desc *desc, int node)
{
	return 0;
}
