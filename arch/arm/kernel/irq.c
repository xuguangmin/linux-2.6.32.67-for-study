/*
 *  linux/arch/arm/kernel/irq.c
 *
 *  Copyright (C) 1992 Linus Torvalds
 *  Modifications for ARM processor Copyright (C) 1995-2000 Russell King.
 *
 *  Support for Dynamic Tick Timer Copyright (C) 2004-2005 Nokia Corporation.
 *  Dynamic Tick Timer written by Tony Lindgren <tony@atomide.com> and
 *  Tuukka Tikkanen <tuukka.tikkanen@elektrobit.com>.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  This file contains the code used by various IRQ handling routines:
 *  asking for different IRQ's should be done through these routines
 *  instead of just grabbing them. Thus setups with different IRQ numbers
 *  shouldn't result in any weird surprises, and installing new handlers
 *  should be easier.
 *
 *  IRQ's are in fact implemented a bit like signal handlers for the kernel.
 *  Naturally it's not a 1:1 relation, but there are similarities.
 */
#include <linux/kernel_stat.h>
#include <linux/module.h>
#include <linux/signal.h>
#include <linux/ioport.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/slab.h>
#include <linux/random.h>
#include <linux/smp.h>
#include <linux/init.h>
#include <linux/seq_file.h>
#include <linux/errno.h>
#include <linux/list.h>
#include <linux/kallsyms.h>
#include <linux/proc_fs.h>

#include <asm/system.h>
#include <asm/mach/irq.h>
#include <asm/mach/time.h>

/*
 * No architecture-specific irq_finish function defined in arm/arch/irqs.h.
 */
#ifndef irq_finish
#define irq_finish(irq) do { } while (0)
#endif

void (*init_arch_irq)(void) __initdata = NULL;
unsigned long irq_err_count;

int show_interrupts(struct seq_file *p, void *v)
{
	int i = *(loff_t *) v, cpu;
	struct irqaction * action;
	unsigned long flags;

	if (i == 0) {
		char cpuname[12];

		seq_printf(p, "    ");
		for_each_present_cpu(cpu) {
			sprintf(cpuname, "CPU%d", cpu);
			seq_printf(p, " %10s", cpuname);
		}
		seq_putc(p, '\n');
	}

	if (i < NR_IRQS) {
		spin_lock_irqsave(&irq_desc[i].lock, flags);
		action = irq_desc[i].action;
		if (!action)
			goto unlock;

		seq_printf(p, "%3d: ", i);
		for_each_present_cpu(cpu)
			seq_printf(p, "%10u ", kstat_irqs_cpu(i, cpu));
		seq_printf(p, " %10s", irq_desc[i].chip->name ? : "-");
		seq_printf(p, "  %s", action->name);
		for (action = action->next; action; action = action->next)
			seq_printf(p, ", %s", action->name);

		seq_putc(p, '\n');
unlock:
		spin_unlock_irqrestore(&irq_desc[i].lock, flags);
	} else if (i == NR_IRQS) {
#ifdef CONFIG_FIQ
		show_fiq_list(p, v);
#endif
#ifdef CONFIG_SMP
		show_ipi_list(p);
		show_local_irqs(p);
#endif
		seq_printf(p, "Err: %10lu\n", irq_err_count);
	}
	return 0;
}

/*
 * do_IRQ handles all hardware IRQ's.  Decoded IRQs should not
 * come via this function.  Instead, they should provide their
 * own 'handler'
 */
 /* 当有外部中断发生时，预先设计好的处理器硬件逻辑会把当前任务上下文寄存器保存在一个
  * 特定的中断栈中，屏蔽掉处理器响应外部中断的能力，在结束部分，
  * 硬件逻辑根据中断向量表中的外部中断对应的入口地址，开始调用由操作系统提供的通用中断处理函数
  * 通用中断处理函数调用此函数, 中断处理的绝大部分流程都浓缩在了这个Ｃ函数中，
  * 当这个函数返回时,通用中断处理函数余下的部分完成中断现场恢复的工作，标志者中断处理流程的结束, 被中断的任务开始继续执行
  * @irq:通用中断处理函数从ＰＩＣ中得到的软件中断号
  * @regs是保存下来的被中断任务的执行现场*/
asmlinkage void __exception asm_do_IRQ(unsigned int irq, struct pt_regs *regs)
{
	/*set_irq_regs将一个per_CPU型的指针变量__irq_regs保存到old_regs中，然后将__irq_regs赋予了一个新值regs,这样中断处理过程中，系统中每个ｃｐｕ都可以通过__irq_regs来访问系统保存的中断现场*/
	struct pt_regs *old_regs = set_irq_regs(regs);

	/*HARDIRQ部分的开始,告诉系统进入冲断处理的上半部分，与irq_enter对应的是irq_exit,irq_enter会更新系统中的一些统计量，同时会把当前栈中的preemtp_count变量加上HARDIRQ_OFFSET来标识一个HARDIRQ中上下文*/
	irq_enter();

	/*
	 * Some hardware gives randomly wrong interrupts.  Rather
	 * than crashing, do something sensible.
	 */
	if (unlikely(irq >= NR_IRQS)) {
		if (printk_ratelimit())
			printk(KERN_WARNING "Bad IRQ%u\n", irq);
		ack_bad_irq(irq);
	} else {
		/*负责对当前发生的中断进行实际的处理*/
		generic_handle_irq(irq);
	}

	/* AT91 specific workaround */
	irq_finish(irq);

	/*SOFTIRQ在此函数中完成,中断处理的下班部分在此函数完成*/
	irq_exit();
	/*函数结束调用set_irq_regs来恢复__irq_regs，__irq_regs一般用来在调试或者诊断时打印当前栈的信息，也可以通过这些保存的中断现场寄存器判断出被中断的进程当时运行在用户态还是内核态*/
	set_irq_regs(old_regs);
}

void set_irq_flags(unsigned int irq, unsigned int iflags)
{
	struct irq_desc *desc;
	unsigned long flags;

	if (irq >= NR_IRQS) {
		printk(KERN_ERR "Trying to set irq flags for IRQ%d\n", irq);
		return;
	}

	desc = irq_desc + irq;
	spin_lock_irqsave(&desc->lock, flags);
	desc->status |= IRQ_NOREQUEST | IRQ_NOPROBE | IRQ_NOAUTOEN;
	if (iflags & IRQF_VALID)
		desc->status &= ~IRQ_NOREQUEST;
	if (iflags & IRQF_PROBE)
		desc->status &= ~IRQ_NOPROBE;
	if (!(iflags & IRQF_NOAUTOEN))
		desc->status &= ~IRQ_NOAUTOEN;
	spin_unlock_irqrestore(&desc->lock, flags);
}

void __init init_IRQ(void)
{
	int irq;

	for (irq = 0; irq < NR_IRQS; irq++)
		irq_desc[irq].status |= IRQ_NOREQUEST | IRQ_NOPROBE;

	init_arch_irq();
}

#ifdef CONFIG_HOTPLUG_CPU

static void route_irq(struct irq_desc *desc, unsigned int irq, unsigned int cpu)
{
	pr_debug("IRQ%u: moving from cpu%u to cpu%u\n", irq, desc->node, cpu);

	spin_lock_irq(&desc->lock);
	desc->chip->set_affinity(irq, cpumask_of(cpu));
	spin_unlock_irq(&desc->lock);
}

/*
 * The CPU has been marked offline.  Migrate IRQs off this CPU.  If
 * the affinity settings do not allow other CPUs, force them onto any
 * available CPU.
 */
void migrate_irqs(void)
{
	unsigned int i, cpu = smp_processor_id();

	for (i = 0; i < NR_IRQS; i++) {
		struct irq_desc *desc = irq_desc + i;

		if (desc->node == cpu) {
			unsigned int newcpu = cpumask_any_and(desc->affinity,
							      cpu_online_mask);
			if (newcpu >= nr_cpu_ids) {
				if (printk_ratelimit())
					printk(KERN_INFO "IRQ%u no longer affine to CPU%u\n",
					       i, cpu);

				cpumask_setall(desc->affinity);
				newcpu = cpumask_any_and(desc->affinity,
							 cpu_online_mask);
			}

			route_irq(desc, i, newcpu);
		}
	}
}
#endif /* CONFIG_HOTPLUG_CPU */
