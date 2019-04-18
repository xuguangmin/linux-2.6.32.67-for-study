#ifndef _ASM_X86_PGTABLE_64_DEFS_H
#define _ASM_X86_PGTABLE_64_DEFS_H

#ifndef __ASSEMBLY__
#include <linux/types.h>

/*
 * These are used to make use of C type-checking..
 */
typedef unsigned long	pteval_t;
typedef unsigned long	pmdval_t;
typedef unsigned long	pudval_t;
typedef unsigned long	pgdval_t;
typedef unsigned long	pgprotval_t;

typedef struct { pteval_t pte; } pte_t;

#endif	/* !__ASSEMBLY__ */

#define SHARED_KERNEL_PMD	0
#define PAGETABLE_LEVELS	4

/*
 * PGDIR_SHIFT determines what a top-level page table entry can map
 */
#define PGDIR_SHIFT	39  //确定页全局目录项能映射的区域大小的对数
#define PTRS_PER_PGD	512//计算页全局目录中其中表项的个数

/*
 * 3rd level page
 */
#define PUD_SHIFT	30 /* 确定页上级目录项能映射的区域大小的对数9+9+12 */
#define PTRS_PER_PUD	512   //计算其中表项的个数

/*
 * PMD_SHIFT determines the size of the area a middle-level
 * page table can map
 * 在80X86上，PUD_SHIFT总是等于PMD_SHIFT,而PUD_SIZE则等于2MB或4MB
 */
#define PMD_SHIFT	21   /* 指定线性地址的Offset字段和Table字段的总位数 */
#define PTRS_PER_PMD	512    //计算其中表项的个数

/*
 * entries per page directory level
 */
#define PTRS_PER_PTE	512    //计算其中表项的个数
#define PMD_SIZE	(_AC(1, UL) << PMD_SHIFT)
#define PMD_MASK	(~(PMD_SIZE - 1))
/*用于计算页全局目录中一个单独表项所能映射的区域大小*/
#define PUD_SIZE	(_AC(1, UL) << PUD_SHIFT)
/*用于屏蔽Offset字段，Table字段，Middle Dir字段和Upper Dir字段的所有位*/
#define PUD_MASK	(~(PUD_SIZE - 1))
/*用于计算页全局目录项中一个单独表项所能映射区域的大小*/
#define PGDIR_SIZE	(_AC(1, UL) << PGDIR_SHIFT)
/*用于屏蔽Offset,Table,Middle Dir,Upper Dir字段的所有位*/
#define PGDIR_MASK	(~(PGDIR_SIZE - 1))

/* See Documentation/x86/x86_64/mm.txt for a description of the memory map. */
#define MAXMEM		 _AC(__AC(1, UL) << MAX_PHYSMEM_BITS, UL)
#define VMALLOC_START    _AC(0xffffc90000000000, UL)
#define VMALLOC_END      _AC(0xffffe8ffffffffff, UL)
#define VMEMMAP_START	 _AC(0xffffea0000000000, UL)
#define MODULES_VADDR    _AC(0xffffffffa0000000, UL)
#define MODULES_END      _AC(0xffffffffff000000, UL)
#define MODULES_LEN   (MODULES_END - MODULES_VADDR)
#define ESPFIX_PGD_ENTRY _AC(-2, UL)
#define ESPFIX_BASE_ADDR (ESPFIX_PGD_ENTRY << PGDIR_SHIFT)

#endif /* _ASM_X86_PGTABLE_64_DEFS_H */
