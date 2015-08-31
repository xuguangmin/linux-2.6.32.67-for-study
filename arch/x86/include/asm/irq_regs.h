/*
 * Per-cpu current frame pointer - the location of the last exception frame on
 * the stack, stored in the per-cpu area.
 *
 * Jeremy Fitzhardinge <jeremy@goop.org>
 */
#ifndef _ASM_X86_IRQ_REGS_H
#define _ASM_X86_IRQ_REGS_H

#include <asm/percpu.h>

#define ARCH_HAS_OWN_IRQ_REGS

DECLARE_PER_CPU(struct pt_regs *, irq_regs);

static inline struct pt_regs *get_irq_regs(void)
{
	return percpu_read(irq_regs);
}

/*set_irq_regs将一个per_CPU型的指针变量__irq_regs保存到old_regs中，然后将__irq_regs赋予了一个新值regs,这样中断处理过程中，系统中每个ｃｐｕ都可以通过__irq_regs来访问系统保存的中断现场*/
static inline struct pt_regs *set_irq_regs(struct pt_regs *new_regs)
{
	struct pt_regs *old_regs;

	old_regs = get_irq_regs();
	percpu_write(irq_regs, new_regs);

	return old_regs;
}

#endif /* _ASM_X86_IRQ_REGS_32_H */
