#ifndef _LINUX_CDEV_H
#define _LINUX_CDEV_H

/*linux中每个字符设备都抽象成一个cdev结构的变量*/
#include <linux/kobject.h>
#include <linux/kdev_t.h>
#include <linux/list.h>

struct file_operations;
struct inode;
struct module;

/*字符设备的抽象，仅仅为了满足对字符设备驱动程序框架结构设计的需要，现实中一个具体字符硬件设备的数据结构的抽象往往要复杂的多，struct cdev常常作为一种内嵌的成员变量出现在实际设备的数据结构中  如：
struct my_keypad_dev{
	int a;
	int b;
	int c;
	...
	//内嵌的stuct cdev数据结构
	struct cdev cdev;
}*/
struct cdev {
	struct kobject kobj;	//每个cdev都是一个kobject
	struct module *owner;	//字符设备驱动程序所在的内核模块对象指针
	const struct file_operations *ops; //操纵这个字符设备文件的方法
	struct list_head list;	/* 与cdev对应的字符设备文件的inode-i_devices的
				 * 链表头
				 * 用来将系统中的字符设备形成链表*/
	dev_t dev;		//字符设备的设备号，由主设备号和次设备号构成
	unsigned int count;	/* 隶属于同一主设备号的次设备的个数，
				 * 用于表示由当前设备驱动程序控制的实际同类设备的数量*/
};

/*
	一个 cdev 一般它有两种定义初始化方式：静态的和动态的。
	静态内存定义初始化：
	struct cdev my_cdev;
	cdev_init(&my_cdev, &fops);
	my_cdev.owner = THIS_MODULE;
	动态内存定义初始化：
	struct cdev *my_cdev = cdev_alloc();
	my_cdev->ops = &fops;
	my_cdev->owner = THIS_MODULE;
	or
	struct *p = kzalloc(sizeof(struct cdev), GFP_KERNEL)
*/
void cdev_init(struct cdev *, const struct file_operations *);

struct cdev *cdev_alloc(void);

void cdev_put(struct cdev *p);

/*
初始化 cdev 后，需要把它添加到系统中去。为此可以调用 cdev_add() 函数。传入 cdev 结构的指针，起始设备编号，以及设备编号范围。用于向系统添加一个cdev，完成字符设备的注册*/
int cdev_add(struct cdev *, dev_t, unsigned);

/*
当一个字符设备驱动不再需要的时候（比如模块卸载），就可以用 cdev_del() 函数来释放 cdev 占用的内存*/
void cdev_del(struct cdev *);

int cdev_index(struct inode *inode);

void cd_forget(struct inode *);

extern struct backing_dev_info directly_mappable_cdev_bdi;

#endif
