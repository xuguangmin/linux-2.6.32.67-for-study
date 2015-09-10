#ifndef __ASM_GENERIC_POLL_H
#define __ASM_GENERIC_POLL_H

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
