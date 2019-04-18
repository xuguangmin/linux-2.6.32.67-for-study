#ifndef _ASM_X86_TLBFLUSH_H
#define _ASM_X86_TLBFLUSH_H

#include <linux/mm.h>
#include <linux/sched.h>

#include <asm/processor.h>
#include <asm/system.h>

#ifdef CONFIG_PARAVIRT
#include <asm/paravirt.h>
#else
 /* intel 处理器上使用的使TLB无效的宏，intel处理器提供了两种使TLB无效的技术
   * 1, 在向CR3寄存器写入值时所有pentium处理器自动刷新相对于非全局页的TLB表项
   * 2, invlpg汇编语言指令使映射指定线性地址的单个TLB表项无效
   */

//将CR3寄存器的当前值重新写回CR3
#define __flush_tlb() __native_flush_tlb()  
//通过清除CR4的PGE标志禁用全局页，将CR3寄存器的当前值重新写回CR3，并再次设置PGE标志
#define __flush_tlb_global() __native_flush_tlb_global()
//以addr为参数执行invlpg汇编语言指令
#define __flush_tlb_single(addr) __native_flush_tlb_single(addr)
#endif

static inline void __native_flush_tlb(void)
{
	native_write_cr3(native_read_cr3());
}

static inline void __native_flush_tlb_global(void)
{
	unsigned long flags;
	unsigned long cr4;

	/*
	 * Read-modify-write to CR4 - protect it from preemption and
	 * from interrupts. (Use the raw variant because this code can
	 * be called from deep inside debugging code.)
	 */
	raw_local_irq_save(flags);

	cr4 = native_read_cr4();
	/* clear PGE */
	native_write_cr4(cr4 & ~X86_CR4_PGE);
	/* write old PGE again and flush TLBs */
	native_write_cr4(cr4);

	raw_local_irq_restore(flags);
}

static inline void __native_flush_tlb_single(unsigned long addr)
{
	asm volatile("invlpg (%0)" ::"r" (addr) : "memory");
}

/* 刷新所有TLB表项(包括那些全局页对应的TLB表项，
  * 即那些Global标志被置位的页)*/
static inline void __flush_tlb_all(void)
{
	if (cpu_has_pge)
		__flush_tlb_global();
	else
		__flush_tlb();
}

static inline void __flush_tlb_one(unsigned long addr)
{
	if (cpu_has_invlpg)
		__flush_tlb_single(addr);
	else
		__flush_tlb();
}

#ifdef CONFIG_X86_32
# define TLB_FLUSH_ALL	0xffffffff
#else
# define TLB_FLUSH_ALL	-1ULL
#endif

/*
 * TLB flushing:
 *
 *  - flush_tlb() flushes the current mm struct TLBs
 *  - flush_tlb_all() flushes all processes TLBs
 *  - flush_tlb_mm(mm) flushes the specified mm context TLB's
 *  - flush_tlb_page(vma, vmaddr) flushes one page
 *  - flush_tlb_range(vma, start, end) flushes a range of pages
 *  - flush_tlb_kernel_range(start, end) flushes a range of kernel pages
 *  - flush_tlb_others(cpumask, mm, va) flushes TLBs on other cpus
 *
 * ..but the i386 has somewhat limited tlb flushing capabilities,
 * and page-granular flushes are available only on i486 and up.
 *
 * x86-64 can only flush individual pages or full VMs. For a range flush
 * we always do the full VM. Might be worth trying if for a small
 * range a few INVLPGs in a row are a win.
 */

#ifndef CONFIG_SMP

/*刷新当前进程拥有的非全局页相关的所有TLB表项
* 使用时机: 执行进程切换时*/
#define flush_tlb() __flush_tlb()
/* 刷新所有TLB表项(包括那些全局页对应的TLB表项，
  * 即那些Global标志被置位的页)
  * 使用时机:改变内核页表项时*/
#define flush_tlb_all() __flush_tlb_all()
#define local_flush_tlb() __flush_tlb()

/* 刷新指定进程拥有的非全局页相关的所有TLB表项
* 使用时机:创建一个新的子进程时*/
static inline void flush_tlb_mm(struct mm_struct *mm)
{
	if (mm == current->active_mm)
		__flush_tlb();
}

static inline void flush_tlb_page(struct vm_area_struct *vma,
				  unsigned long addr)
{
	if (vma->vm_mm == current->active_mm)
		__flush_tlb_one(addr);
}

/* 刷新指定进程的线性地址间隔对应的TLB表项
  * 使用时机:释放某个进程的线性地址间隔时*/
static inline void flush_tlb_range(struct vm_area_struct *vma,
				   unsigned long start, unsigned long end)
{
	if (vma->vm_mm == current->active_mm)
		__flush_tlb();
}

static inline void native_flush_tlb_others(const struct cpumask *cpumask,
					   struct mm_struct *mm,
					   unsigned long va)
{
}

static inline void reset_lazy_tlbstate(void)
{
}

#else  /* SMP */

#include <asm/smp.h>

#define local_flush_tlb() __flush_tlb()

extern void flush_tlb_all(void);
extern void flush_tlb_current_task(void);
extern void flush_tlb_mm(struct mm_struct *);
extern void flush_tlb_page(struct vm_area_struct *, unsigned long);

/*刷新当前进程拥有的非全局页相关的所有TLB表项
  * 使用时机: 执行进程切换时*/
#define flush_tlb()	flush_tlb_current_task()

/* 刷新指定进程的线性地址间隔对应的TLB表项
  * 使用时机:释放某个进程的线性地址间隔时*/
static inline void flush_tlb_range(struct vm_area_struct *vma,
				   unsigned long start, unsigned long end)
{
	flush_tlb_mm(vma->vm_mm);
}

void native_flush_tlb_others(const struct cpumask *cpumask,
			     struct mm_struct *mm, unsigned long va);

#define TLBSTATE_OK	1  // 非懒惰TLB模式
#define TLBSTATE_LAZY	2  // 懒惰TLB模式

struct tlb_state {
	struct mm_struct *active_mm;  //指向当前进程内存描述符
	int state;   // TLBSTATE_OK  or TLBSTATE_LAZY
};
DECLARE_PER_CPU_SHARED_ALIGNED(struct tlb_state, cpu_tlbstate);

static inline void reset_lazy_tlbstate(void)
{
	percpu_write(cpu_tlbstate.state, 0);
	percpu_write(cpu_tlbstate.active_mm, &init_mm);
}

#endif	/* SMP */

#ifndef CONFIG_PARAVIRT
#define flush_tlb_others(mask, mm, va)	native_flush_tlb_others(mask, mm, va)
#endif

/*刷新给定线性地址范围内的所有TLB表项
    (包括那些全局页对应的TLB表项)
  * 使用时机:更换一个范围内的内核页表项时*/
static inline void flush_tlb_kernel_range(unsigned long start,
					  unsigned long end)
{
	flush_tlb_all();
}

extern void zap_low_mappings(bool early);

#endif /* _ASM_X86_TLBFLUSH_H */
