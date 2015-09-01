#ifndef _LINUX_IRQRETURN_H
#define _LINUX_IRQRETURN_H

/**
 * enum irqreturn
 * @IRQ_NONE		interrupt was not from this device
 * @IRQ_HANDLED		interrupt was handled by this device
 * @IRQ_WAKE_THREAD	handler requests to wake the handler thread
 */
 /*中断处理例程返回值类型*/
enum irqreturn {
	IRQ_NONE,	/* 中断处理例程发现正在处理一个不是自己的设备触发的中断
			 * ,此时唯一要做的就是返回该值*/
	IRQ_HANDLED,	/* 中断处理例程成功的处理了自己设备的中断,返回该值*/
	IRQ_WAKE_THREAD,/* 中断处理例程被用来作唤醒一个等待在他的irq上
			 * 的一个进程使用,此时返回该值*/
};

typedef enum irqreturn irqreturn_t;
#define IRQ_RETVAL(x)	((x) != IRQ_NONE)

#endif
