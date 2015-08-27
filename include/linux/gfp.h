#ifndef __LINUX_GFP_H
#define __LINUX_GFP_H

#include <linux/mmzone.h>
#include <linux/stddef.h>
#include <linux/linkage.h>
#include <linux/topology.h>
#include <linux/mmdebug.h>


/* 
 * 以'__'打头的GFP掩码只限于在内存管理组件内部的代码使用，
 * gfp_mask掩码以"GFP_"的形式出现
 */

struct vm_area_struct;

/*
 * GFP bitmasks..
 *
 * Zone modifiers (see linux/mmzone.h - low three bits)
 *
 * Do not put any conditional on these. If necessary modify the definitions
 * without the underscores and use the consistently. The definitions here may
 * be used in bit comparisons.
 */
 /*在ZONE_DMA标识的内存区域中查找空闲页*/
#define __GFP_DMA	((__force gfp_t)0x01u)
/*在ZONE_HIGHMEM标识的内存区域查找空闲页*/
#define __GFP_HIGHMEM	((__force gfp_t)0x02u)
/*在ZONE_DMA32标识的内存区域查找空闲页*/
#define __GFP_DMA32	((__force gfp_t)0x04u)
/*内核将分配的物理页标记为可移动的*/
#define __GFP_MOVABLE	((__force gfp_t)0x08u)  /* Page is movable */
#define GFP_ZONEMASK	(__GFP_DMA|__GFP_HIGHMEM|__GFP_DMA32|__GFP_MOVABLE)
/*
 * Action modifiers - doesn't change the zoning
 *
 * __GFP_REPEAT: Try hard to allocate the memory, but the allocation attempt
 * _might_ fail.  This depends upon the particular VM implementation.
 *
 * __GFP_NOFAIL: The VM implementation _must_ retry infinitely: the caller
 * cannot handle allocation failures.
 *
 * __GFP_NORETRY: The VM implementation must not retry indefinitely.
 *
 * __GFP_MOVABLE: Flag that this page will be movable by the page migration
 * mechanism or reclaimed
 */

 /* 当前正在向内核申请页分配的进程被阻塞，
  * 意味着调度器可以在此请求期间调度另外一个进程执行*/
#define __GFP_WAIT	((__force gfp_t)0x10u)	/* Can wait and reschedule? */

/* 内核允许使用紧急分配链表中的保留内存页。该请求必须以原子方式完成，
 * 意味着请求过程不允许被中断*/
#define __GFP_HIGH	((__force gfp_t)0x20u)	/* Should access emergency pools? */

/*内核在查找空闲页过程中可以进行I/O操作，如此内核可以将换出的页写到磁盘*/
#define __GFP_IO	((__force gfp_t)0x40u)	/* Can start physical IO? */

/*查找空闲页的过程中允许执行文件系统相关操作*/
#define __GFP_FS	((__force gfp_t)0x80u)	/* Can call down to low-level FS? */

/*从非缓存的‘冷页’中分配*/
#define __GFP_COLD	((__force gfp_t)0x100u)	/* Cache-cold page required */

/*禁止分配失败时的警告*/
#define __GFP_NOWARN	((__force gfp_t)0x200u)	/* Suppress page allocation failure warning */

/*如果分配行为失败，可以自动尝试再次分配，尝试若干次后会终止*/
#define __GFP_REPEAT	((__force gfp_t)0x400u)	/* See above */

/* 分配失败后一直尝试，直到分配成功为止，分配函数的调用者无法处理分配失败的情况，
 * 2.6.39注释以后新版本不再使用该掩码*/
#define __GFP_NOFAIL	((__force gfp_t)0x800u)	/* See above */

/*如果分配失败，不会进行重试操作*/
#define __GFP_NORETRY	((__force gfp_t)0x1000u)/* See above */

/*增加符合页元数据*/
#define __GFP_COMP	((__force gfp_t)0x4000u)/* Add compound page metadata */

/*用0填充成功分配出来的物理页*/
#define __GFP_ZERO	((__force gfp_t)0x8000u)/* Return zeroed page on success */

/*不要使用仅紧急分配使用的保留分配链表*/
#define __GFP_NOMEMALLOC ((__force gfp_t)0x10000u) /* Don't use emergency reserves */

/* 只能在当前进程允许运行的各个CPU所关联的节点分配内存，
 * 该标识只有在NUMA系统上才有意义*/
#define __GFP_HARDWALL   ((__force gfp_t)0x20000u) /* Enforce hardwall cpuset memory allocs */
#define __GFP_THISNODE	((__force gfp_t)0x40000u)/* No fallback, no policies */
#define __GFP_RECLAIMABLE ((__force gfp_t)0x80000u) /* Page is reclaimable */

#ifdef CONFIG_KMEMCHECK
#define __GFP_NOTRACK	((__force gfp_t)0x200000u)  /* Don't track with kmemcheck */
#else
#define __GFP_NOTRACK	((__force gfp_t)0)
#endif

/*
 * This may seem redundant, but it's a way of annotating false positives vs.
 * allocations that simply cannot be supported (e.g. page tables).
 */
#define __GFP_NOTRACK_FALSE_POSITIVE (__GFP_NOTRACK)

#define __GFP_BITS_SHIFT 22	/* Room for 22 __GFP_FOO bits */
#define __GFP_BITS_MASK ((__force gfp_t)((1 << __GFP_BITS_SHIFT) - 1))

/*常见的gfp_mask掩码*/
/* This equals 0, but use constants in case they ever change */
#define GFP_NOWAIT	(GFP_ATOMIC & ~__GFP_HIGH)


/*
 * 分配优先级
 * __GFP_HIGHMEM:先在ZONE_HIGHMEM域中查找空闲页，如果无法满足当前分配，
 *    页分配器到ZONE_NORMAL域中继续查找，如果依然无法满足当前分配，
      分配器到ZONE_DMA域，或者成功或者失败。
 * __GFP_NORMAL:没有指定__GFP_HIGHMEM或者__GFP_DMA默认相当于__GFP_NORMAL,其次在Z      ONE_DMA域
 * __GFP_DMA:只能在ZONE_DMA中分配物理页面，如果无法满足，则分配失败。
 */

/* GFP_ATOMIC means both !wait (__GFP_WAIT not set) and use emergency pool */
/* 内核模块中最常使用的,用于原子分配，也是上面几个掩码中唯一不带__GFP_WAIT的
 * 此掩码告诉分配器，在分配内存页时，绝对不能终端当前进场或者把当前进场移出
 * 调度器。必要的情况下可以使用仅限紧急情况使用的保留内存页。在驱动程序中，
 * 一般在中断处理例程或者非进场上下文的代码中使用GFP_ATOMIC掩码进行内存分配，
 * 因为这两种情况下分配都必须保证当前进程不能睡眠*/
#define GFP_ATOMIC	(__GFP_HIGH)

/*GFP_NOIO GFP_NOFS都带有__GFP_WAIT,因此可以被中断，前者在分配过程中禁止I/O操作，后者禁止文件系统相关的函数调用*/
#define GFP_NOIO	(__GFP_WAIT)
#define GFP_NOFS	(__GFP_WAIT | __GFP_IO)
/* 
 * 内核模块中最常使用的掩码之一，带有该掩码的内存分配可能导致单签进程进入睡眠状态*/
#define GFP_KERNEL	(__GFP_WAIT | __GFP_IO | __GFP_FS)
#define GFP_TEMPORARY	(__GFP_WAIT | __GFP_IO | __GFP_FS | \
			 __GFP_RECLAIMABLE)

/*用于为用户空间分配内存页，可能引起进程的休眠*/
#define GFP_USER	(__GFP_WAIT | __GFP_IO | __GFP_FS | __GFP_HARDWALL)

/*对GFP_USER的一个扩展，可以使用非线性映射的高端内存*/
#define GFP_HIGHUSER	(__GFP_WAIT | __GFP_IO | __GFP_FS | __GFP_HARDWALL | \
			 __GFP_HIGHMEM)
#define GFP_HIGHUSER_MOVABLE	(__GFP_WAIT | __GFP_IO | __GFP_FS | \
				 __GFP_HARDWALL | __GFP_HIGHMEM | \
				 __GFP_MOVABLE)

#ifdef CONFIG_NUMA
#define GFP_THISNODE	(__GFP_THISNODE | __GFP_NOWARN | __GFP_NORETRY)
#else
#define GFP_THISNODE	((__force gfp_t)0)
#endif

/* This mask makes up all the page movable related flags */
#define GFP_MOVABLE_MASK (__GFP_RECLAIMABLE|__GFP_MOVABLE)

/* Control page allocator reclaim behavior */
#define GFP_RECLAIM_MASK (__GFP_WAIT|__GFP_HIGH|__GFP_IO|__GFP_FS|\
			__GFP_NOWARN|__GFP_REPEAT|__GFP_NOFAIL|\
			__GFP_NORETRY|__GFP_NOMEMALLOC)

/* Control slab gfp mask during early boot */
#define GFP_BOOT_MASK __GFP_BITS_MASK & ~(__GFP_WAIT|__GFP_IO|__GFP_FS)

/* Control allocation constraints */
#define GFP_CONSTRAINT_MASK (__GFP_HARDWALL|__GFP_THISNODE)

/* Do not use these with a slab allocator */
#define GFP_SLAB_BUG_MASK (__GFP_DMA32|__GFP_HIGHMEM|~__GFP_BITS_MASK)

/* Flag - indicates that the buffer will be suitable for DMA.  Ignored on some
   platforms, used as appropriate on others */

/*限制页面分配器只能在ZONE_DMA域中分配空闲物理页面，用于分配使用于DMA缓冲区的内存*/
#define GFP_DMA		__GFP_DMA

/* 4GB DMA on some platforms */
/*在ZONE_DMA32标识的内存区域中查找空闲页*/
#define GFP_DMA32	__GFP_DMA32	

/* Convert GFP flags to their corresponding migrate type */
static inline int allocflags_to_migratetype(gfp_t gfp_flags)
{
	WARN_ON((gfp_flags & GFP_MOVABLE_MASK) == GFP_MOVABLE_MASK);

	if (unlikely(page_group_by_mobility_disabled))
		return MIGRATE_UNMOVABLE;

	/* Group based on mobility */
	return (((gfp_flags & __GFP_MOVABLE) != 0) << 1) |
		((gfp_flags & __GFP_RECLAIMABLE) != 0);
}

#ifdef CONFIG_HIGHMEM
#define OPT_ZONE_HIGHMEM ZONE_HIGHMEM/*在ZONE_HIGHMEM标识的内存区域中查找空闲页*/
#else
#define OPT_ZONE_HIGHMEM ZONE_NORMAL/**/
#endif

#ifdef CONFIG_ZONE_DMA
#define OPT_ZONE_DMA ZONE_DMA
#else
#define OPT_ZONE_DMA ZONE_NORMAL
#endif

#ifdef CONFIG_ZONE_DMA32
#define OPT_ZONE_DMA32 ZONE_DMA32
#else
#define OPT_ZONE_DMA32 ZONE_NORMAL
#endif

/*
 * GFP_ZONE_TABLE is a word size bitstring that is used for looking up the
 * zone to use given the lowest 4 bits of gfp_t. Entries are ZONE_SHIFT long
 * and there are 16 of them to cover all possible combinations of
 * __GFP_DMA, __GFP_DMA32, __GFP_MOVABLE and __GFP_HIGHMEM
 *
 * The zone fallback order is MOVABLE=>HIGHMEM=>NORMAL=>DMA32=>DMA.
 * But GFP_MOVABLE is not only a zone specifier but also an allocation
 * policy. Therefore __GFP_MOVABLE plus another zone selector is valid.
 * Only 1bit of the lowest 3 bit (DMA,DMA32,HIGHMEM) can be set to "1".
 *
 *       bit       result
 *       =================
 *       0x0    => NORMAL
 *       0x1    => DMA or NORMAL
 *       0x2    => HIGHMEM or NORMAL
 *       0x3    => BAD (DMA+HIGHMEM)
 *       0x4    => DMA32 or DMA or NORMAL
 *       0x5    => BAD (DMA+DMA32)
 *       0x6    => BAD (HIGHMEM+DMA32)
 *       0x7    => BAD (HIGHMEM+DMA32+DMA)
 *       0x8    => NORMAL (MOVABLE+0)
 *       0x9    => DMA or NORMAL (MOVABLE+DMA)
 *       0xa    => MOVABLE (Movable is valid only if HIGHMEM is set too)
 *       0xb    => BAD (MOVABLE+HIGHMEM+DMA)
 *       0xc    => DMA32 (MOVABLE+HIGHMEM+DMA32)
 *       0xd    => BAD (MOVABLE+DMA32+DMA)
 *       0xe    => BAD (MOVABLE+DMA32+HIGHMEM)
 *       0xf    => BAD (MOVABLE+DMA32+HIGHMEM+DMA)
 *
 * ZONES_SHIFT must be <= 2 on 32 bit platforms.
 */

#if 16 * ZONES_SHIFT > BITS_PER_LONG
#error ZONES_SHIFT too large to create GFP_ZONE_TABLE integer
#endif

#define GFP_ZONE_TABLE ( \
	(ZONE_NORMAL << 0 * ZONES_SHIFT)				\
	| (OPT_ZONE_DMA << __GFP_DMA * ZONES_SHIFT) 			\
	| (OPT_ZONE_HIGHMEM << __GFP_HIGHMEM * ZONES_SHIFT)		\
	| (OPT_ZONE_DMA32 << __GFP_DMA32 * ZONES_SHIFT)			\
	| (ZONE_NORMAL << __GFP_MOVABLE * ZONES_SHIFT)			\
	| (OPT_ZONE_DMA << (__GFP_MOVABLE | __GFP_DMA) * ZONES_SHIFT)	\
	| (ZONE_MOVABLE << (__GFP_MOVABLE | __GFP_HIGHMEM) * ZONES_SHIFT)\
	| (OPT_ZONE_DMA32 << (__GFP_MOVABLE | __GFP_DMA32) * ZONES_SHIFT)\
)

/*
 * GFP_ZONE_BAD is a bitmap for all combination of __GFP_DMA, __GFP_DMA32
 * __GFP_HIGHMEM and __GFP_MOVABLE that are not permitted. One flag per
 * entry starting with bit 0. Bit is set if the combination is not
 * allowed.
 */
#define GFP_ZONE_BAD ( \
	1 << (__GFP_DMA | __GFP_HIGHMEM)				\
	| 1 << (__GFP_DMA | __GFP_DMA32)				\
	| 1 << (__GFP_DMA32 | __GFP_HIGHMEM)				\
	| 1 << (__GFP_DMA | __GFP_DMA32 | __GFP_HIGHMEM)		\
	| 1 << (__GFP_MOVABLE | __GFP_HIGHMEM | __GFP_DMA)		\
	| 1 << (__GFP_MOVABLE | __GFP_DMA32 | __GFP_DMA)		\
	| 1 << (__GFP_MOVABLE | __GFP_DMA32 | __GFP_HIGHMEM)		\
	| 1 << (__GFP_MOVABLE | __GFP_DMA32 | __GFP_DMA | __GFP_HIGHMEM)\
)

/* 根据gfp_mask制定在囊二域中分配物理页面
 * 如果没有在gfp_mask中明确制定__GFP_DMA或者__GFP_HIGHMEM,那么默认在ZONE_NORMAL中分配物理页
 * 如果ZONE_NORMAL中现有空闲页不足以满足当前的分配，那么页分配器会到ZONE_DMA域中查找空闲页，而不会到ZONE_HIGHMEM中查找*/
static inline enum zone_type gfp_zone(gfp_t flags)
{
	enum zone_type z;
	int bit = flags & GFP_ZONEMASK;

	z = (GFP_ZONE_TABLE >> (bit * ZONES_SHIFT)) &
					 ((1 << ZONES_SHIFT) - 1);

	if (__builtin_constant_p(bit))
		MAYBE_BUILD_BUG_ON((GFP_ZONE_BAD >> bit) & 1);
	else {
#ifdef CONFIG_DEBUG_VM
		BUG_ON((GFP_ZONE_BAD >> bit) & 1);
#endif
	}
	return z;
}

/*
 * There is only one page-allocator function, and two main namespaces to
 * it. The alloc_page*() variants return 'struct page *' and as such
 * can allocate highmem pages, the *get*page*() variants return
 * virtual kernel addresses to the allocated page(s).
 */

static inline int gfp_zonelist(gfp_t flags)
{
	if (NUMA_BUILD && unlikely(flags & __GFP_THISNODE))
		return 1;

	return 0;
}

/*
 * We get the zone list from the current node and the gfp_mask.
 * This zone list contains a maximum of MAXNODES*MAX_NR_ZONES zones.
 * There are two zonelists per node, one for all zones with memory and
 * one containing just zones from the node the zonelist belongs to.
 *
 * For the normal case of non-DISCONTIGMEM systems the NODE_DATA() gets
 * optimized to &contig_page_data at compile-time.
 */
static inline struct zonelist *node_zonelist(int nid, gfp_t flags)
{
	return NODE_DATA(nid)->node_zonelists + gfp_zonelist(flags);
}

#ifndef HAVE_ARCH_FREE_PAGE
static inline void arch_free_page(struct page *page, int order) { }
#endif
#ifndef HAVE_ARCH_ALLOC_PAGE
static inline void arch_alloc_page(struct page *page, int order) { }
#endif

struct page *
__alloc_pages_nodemask(gfp_t gfp_mask, unsigned int order,
		       struct zonelist *zonelist, nodemask_t *nodemask);

static inline struct page *
__alloc_pages(gfp_t gfp_mask, unsigned int order,
		struct zonelist *zonelist)
{
	return __alloc_pages_nodemask(gfp_mask, order, zonelist, NULL);
}

static inline struct page *alloc_pages_node(int nid, gfp_t gfp_mask,
						unsigned int order)
{
	/* Unknown node is current node */
	if (nid < 0)
		nid = numa_node_id();

	return __alloc_pages(gfp_mask, order, node_zonelist(nid, gfp_mask));
}

static inline struct page *alloc_pages_exact_node(int nid, gfp_t gfp_mask,
						unsigned int order)
{
	VM_BUG_ON(nid < 0 || nid >= MAX_NUMNODES);

	return __alloc_pages(gfp_mask, order, node_zonelist(nid, gfp_mask));
}

#ifdef CONFIG_NUMA
extern struct page *alloc_pages_current(gfp_t gfp_mask, unsigned order);

static inline struct page *
alloc_pages(gfp_t gfp_mask, unsigned int order)
{
	return alloc_pages_current(gfp_mask, order);
}
extern struct page *alloc_page_vma(gfp_t gfp_mask,
			struct vm_area_struct *vma, unsigned long addr);
#else
/*页面分配器,分配2的order次方个连续的物理页面并返回起始页的 page实例*/
#define alloc_pages(gfp_mask, order) \
		alloc_pages_node(numa_node_id(), gfp_mask, order)
#define alloc_page_vma(gfp_mask, vma, addr) alloc_pages(gfp_mask, 0)
#endif

/*只用于分配一个物理页面，alloc_page()是order=0时alloc_pages的简化形式，
只分配单个页面,如果系统没有足够的空间满足alloc_page的分配，函数将返回NULL*/
#define alloc_page(gfp_mask) alloc_pages(gfp_mask, 0)

/*该函数不能在高端内存分配页面*/
extern unsigned long __get_free_pages(gfp_t gfp_mask, unsigned int order);
extern unsigned long get_zeroed_page(gfp_t gfp_mask);

void *alloc_pages_exact(size_t size, gfp_t gfp_mask);
void free_pages_exact(void *virt, size_t size);


/* 如果只想分配单个物理页面可以用这个函数，他是order=0时
 * __get_free_pages的简化形式*/
#define __get_free_page(gfp_mask) \
		__get_free_pages((gfp_mask),0)

/*用于从ZONE_DMA区域中分配物理页，返回页面所在线性地址*/
#define __get_dma_pages(gfp_mask, order) \
		__get_free_pages((gfp_mask) | GFP_DMA,(order))

extern void __free_pages(struct page *page, unsigned int order);
extern void free_pages(unsigned long addr, unsigned int order);
extern void free_hot_page(struct page *page);

#define __free_page(page) __free_pages((page), 0)
#define free_page(addr) free_pages((addr),0)

void page_alloc_init(void);
void drain_zone_pages(struct zone *zone, struct per_cpu_pages *pcp);
void drain_all_pages(void);
void drain_local_pages(void *dummy);

extern gfp_t gfp_allowed_mask;

static inline void set_gfp_allowed_mask(gfp_t mask)
{
	gfp_allowed_mask = mask;
}

#endif /* __LINUX_GFP_H */
