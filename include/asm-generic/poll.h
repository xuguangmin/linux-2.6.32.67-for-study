#ifndef __ASM_GENERIC_POLL_H
#define __ASM_GENERIC_POLL_H

/**
 *	EXAMPLE:驱动程序实现的poll例程的示例
 	//定义一个用于读取的等待队列demo_inq
	static DECLARE_WAIT_QUEUE_HEAD(demo_inq);

	//驱动程序实现的poll例程
	unsigned int demo_poll(struct file *filp, struct poll_table_struct * wait)
	{
		struct demo_buf_list * list = filp->private_data;
		//初始化mask为0,表明目前关于设备的数据的状态没有发生任何变化
		unsigned int mask = 0;
		...
		//调用poll_wait将来自内核中的一个等待节点加入demo_inq队列
		poll_wait(filp, &demo_inq, wait);
		//判断缓冲区是否可读
		if(list->head != list->tail)
			mask |= POLLIN|POLLRDNORM;
		return mask;
	}
	//设备驱动程序实现的中断处理例程
	irqreturn_t demo_isr(int irq, void * dev_id)
	{
		...
		//如果缓冲区可读,调用wake_up函数唤醒阻塞在poll上的进程 
		wake_up_interruptible(&demo_inq);
		...
	}
 */





/* These are specified by iBCS2 */
/*poll例程状态位*/
#define POLLIN		0x0001	/*非高优先级的数据(带外数据out-of_band)可以被无阻塞的读取*/
#define POLLPRI		0x0002	/*高优先级的数据(带外数据out-of_band)可以被无阻塞的读取*/
#define POLLOUT		0x0004	/*数据可以无阻塞的写入*/
#define POLLERR		0x0008	/*设备发生了错误*/
#define POLLHUP		0x0010	/*与设备的链接已经断开*/
#define POLLNVAL	0x0020

/* The rest seem to be more-or-less nonstandard. Check them! */
#define POLLRDNORM	0x0040	/*正常数据可以无阻塞的读取*/
#define POLLRDBAND	0x0080
#ifndef POLLWRNORM
#define POLLWRNORM	0x0100	/*正常数据可以无阻塞的写入*/
#endif
#ifndef POLLWRBAND
#define POLLWRBAND	0x0200
#endif
#ifndef POLLMSG
#define POLLMSG		0x0400
#endif
#ifndef POLLREMOVE
#define POLLREMOVE	0x1000
#endif
#ifndef POLLRDHUP
#define POLLRDHUP       0x2000
#endif

#define POLLFREE	0x4000	/* currently only for epoll */

struct pollfd {
	int fd;	/*文件描述符集合*/
	short events;	/*等待的事件*/
	short revents;	/*实际发生的事件*/
};

#endif	/* __ASM_GENERIC_POLL_H */
