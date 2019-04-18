#include <linux/mm_types.h>
#include <linux/rbtree.h>
#include <linux/rwsem.h>
#include <linux/spinlock.h>
#include <linux/list.h>
#include <linux/cpumask.h>

#include <asm/atomic.h>
#include <asm/pgtable.h>
/*
  * ϵͳ�����е��ں��̶߳�����ͬһ��ӳ�䣬��mm_struct���͵Ľṹ
  * init_mm,�ں��߳�init��mm_struct���ݽṹ����init_mm
  */
struct mm_struct init_mm = {
	.mm_rb		= RB_ROOT,
	.pgd		= swapper_pg_dir, //ʹ��ҳȫ��Ŀ¼���ʼ��
	.mm_users	= ATOMIC_INIT(2),
	.mm_count	= ATOMIC_INIT(1),
	.mmap_sem	= __RWSEM_INITIALIZER(init_mm.mmap_sem),
	.page_table_lock =  __SPIN_LOCK_UNLOCKED(init_mm.page_table_lock),
	.mmlist		= LIST_HEAD_INIT(init_mm.mmlist),
	.cpu_vm_mask	= CPU_MASK_ALL,
};
