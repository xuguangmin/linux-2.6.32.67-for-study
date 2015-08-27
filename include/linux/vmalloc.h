#ifndef _LINUX_VMALLOC_H
#define _LINUX_VMALLOC_H

#include <linux/spinlock.h>
#include <linux/init.h>
#include <asm/page.h>		/* pgprot_t */

struct vm_area_struct;		/* vma defining user mapping in mm_types.h */

/* bits in flags of vmalloc's vm_struct below */
#define VM_IOREMAP	0x00000001	/* ioremap() and friends */
#define VM_ALLOC	0x00000002	/* vmalloc() */
#define VM_MAP		0x00000004	/* vmap()ed pages */
#define VM_USERMAP	0x00000008	/* suitable for remap_vmalloc_range */
#define VM_VPAGES	0x00000010	/* buffer for pages was vmalloc'ed */
#define VM_UNLIST	0x00000020	/* vm_struct is not listed in vmlist */
/* bits [20..32] reserved for arch specific ioremap internals */

/*
 * Maximum alignment for ioremap() regions.
 * Can be overriden by arch-specific value.
 */
#ifndef IOREMAP_MAX_ORDER
#define IOREMAP_MAX_ORDER	(7 + PAGE_SHIFT)	/* 128 pages */
#endif

/*结构表示vmalloc区中每一个分配出来的虚拟内存块*/
struct vm_struct {
	struct vm_struct	*next;	/* 用来把vmalloc区中所有已分配的
					 * struct vm_struct对象构成链表
					 * 该链表的表头为全局变量
					 * struct vm_struct *vmlist
					 */
	void			*addr;/*对应虚拟内存块的起始地址,应该时页对齐*/
	unsigned long		size; /*虚拟内存块的大小,总是页面大小的整数倍*/
	unsigned long		flags;/* 当前虚拟内存块映射特性的标志
				       * VM_ALLOC标志表示当前虚拟内存块是给vmalloc函数使用，映射的是实际物理内存(RAM);VM_IOREMAP表示当前虚拟内存块是给ioremap相关函数使用，映射的是I/O空间地址，也就是设备内存*/
	struct page		**pages;/*是被映射的物理内存页面所形成的数组收地址*/
	unsigned int		nr_pages;/*映射的物理页数量*/
	unsigned long		phys_addr;/*映射的I/O空间起始地址，页对齐*/
	void			*caller;
};

/*
 *	Highlevel APIs for driver use
 */
extern void vm_unmap_ram(const void *mem, unsigned int count);
extern void *vm_map_ram(struct page **pages, unsigned int count,
				int node, pgprot_t prot);
extern void vm_unmap_aliases(void);

#ifdef CONFIG_MMU
extern void __init vmalloc_init(void);
#else
static inline void vmalloc_init(void)
{
}
#endif

/*内核在调用伙伴系统获取物理内存页时，使用了GFP_KERNEL|GFP_HIGHMEM标志，GFP_KERNEL意味着vmalloc函数在执行过程中可能睡眠,因此不能在中断上下文中调用*/
extern void *vmalloc(unsigned long size);
extern void *vmalloc_user(unsigned long size);
extern void *vmalloc_node(unsigned long size, int node);
extern void *vmalloc_exec(unsigned long size);
extern void *vmalloc_32(unsigned long size);
extern void *vmalloc_32_user(unsigned long size);
extern void *__vmalloc(unsigned long size, gfp_t gfp_mask, pgprot_t prot);
extern void *__vmalloc_area(struct vm_struct *area, gfp_t gfp_mask,
				pgprot_t prot);
/*释放vmalloc分配的虚拟地址块*/
extern void vfree(const void *addr);

extern void *vmap(struct page **pages, unsigned int count,
			unsigned long flags, pgprot_t prot);
extern void vunmap(const void *addr);

extern int remap_vmalloc_range(struct vm_area_struct *vma, void *addr,
							unsigned long pgoff);
void vmalloc_sync_all(void);
 
/*
 *	Lowlevel-APIs (not for driver use!)
 */

static inline size_t get_vm_area_size(const struct vm_struct *area)
{
	/* return actual size without guard page */
	return area->size - PAGE_SIZE;
}

extern struct vm_struct *get_vm_area(unsigned long size, unsigned long flags);
extern struct vm_struct *get_vm_area_caller(unsigned long size,
					unsigned long flags, void *caller);
extern struct vm_struct *__get_vm_area(unsigned long size, unsigned long flags,
					unsigned long start, unsigned long end);
extern struct vm_struct *__get_vm_area_caller(unsigned long size,
					unsigned long flags,
					unsigned long start, unsigned long end,
					void *caller);
extern struct vm_struct *get_vm_area_node(unsigned long size,
					  unsigned long flags, int node,
					  gfp_t gfp_mask);
extern struct vm_struct *remove_vm_area(const void *addr);

extern int map_vm_area(struct vm_struct *area, pgprot_t prot,
			struct page ***pages);
extern int map_kernel_range_noflush(unsigned long start, unsigned long size,
				    pgprot_t prot, struct page **pages);
extern void unmap_kernel_range_noflush(unsigned long addr, unsigned long size);
extern void unmap_kernel_range(unsigned long addr, unsigned long size);

/* Allocate/destroy a 'vmalloc' VM area. */
extern struct vm_struct *alloc_vm_area(size_t size);
extern void free_vm_area(struct vm_struct *area);

/* for /dev/kmem */
extern long vread(char *buf, char *addr, unsigned long count);
extern long vwrite(char *buf, char *addr, unsigned long count);

/*
 *	Internals.  Dont't use..
 */
extern rwlock_t vmlist_lock;
extern struct vm_struct *vmlist;
extern __init void vm_area_register_early(struct vm_struct *vm, size_t align);

#ifndef CONFIG_HAVE_LEGACY_PER_CPU_AREA
struct vm_struct **pcpu_get_vm_areas(const unsigned long *offsets,
				     const size_t *sizes, int nr_vms,
				     size_t align, gfp_t gfp_mask);
#endif

void pcpu_free_vm_areas(struct vm_struct **vms, int nr_vms);

#endif /* _LINUX_VMALLOC_H */
