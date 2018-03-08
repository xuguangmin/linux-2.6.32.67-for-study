#ifndef _LINUX_SOCKET_H
#define _LINUX_SOCKET_H

/*
 * Desired design of maximum size and alignment (see RFC2553)
 */
#define _K_SS_MAXSIZE	128	/* Implementation specific max size */
#define _K_SS_ALIGNSIZE	(__alignof__ (struct sockaddr *))
				/* Implementation specific desired alignment */

struct __kernel_sockaddr_storage {
	unsigned short	ss_family;		/* address family */
	/* Following field(s) are implementation specific */
	char		__data[_K_SS_MAXSIZE - sizeof(unsigned short)];
				/* space to achieve desired size, */
				/* _SS_MAXSIZE value minus size of ss_family */
} __attribute__ ((aligned(_K_SS_ALIGNSIZE)));	/* force desired alignment */

#ifdef __KERNEL__

#include <asm/socket.h>			/* arch-dependent defines	*/
#include <linux/sockios.h>		/* the SIOCxxx I/O controls	*/
#include <linux/uio.h>			/* iovec support		*/
#include <linux/types.h>		/* pid_t			*/
#include <linux/compiler.h>		/* __user			*/

#ifdef __KERNEL__
# ifdef CONFIG_PROC_FS
struct seq_file;
extern void socket_seq_show(struct seq_file *seq);
# endif
#endif /* __KERNEL__ */

typedef unsigned short	sa_family_t;

/*
 *	1003.1g requires sa_family_t and that sa_data is char.
 */
 
struct sockaddr {
	sa_family_t	sa_family;	/* address family, AF_xxx	*/
	char		sa_data[14];	/* 14 bytes of protocol address	*/
};

struct linger {
	int		l_onoff;	/* Linger active		*/
	int		l_linger;	/* How long to linger for	*/
};

#define sockaddr_storage __kernel_sockaddr_storage

/*
 *	As we do 4.4BSD message passing we use a 4.4BSD message passing
 *	system, not 4.3. Thus msg_accrights(len) are now missing. They
 *	belong in an obscure libc emulation or the bin.
 */
 /* 描述用户空间负载数据*/
struct msghdr {
	void	*	msg_name;	/* Socket name			*/
	int		msg_namelen;	/* Length of name		*/
	struct iovec *	msg_iov;	/* Data blocks			*/
	__kernel_size_t	msg_iovlen;	/* Number of blocks		*/
	void 	*	msg_control;	/* Per protocol magic (eg BSD file descriptor passing) */
	__kernel_size_t	msg_controllen;	/* Length of cmsg list */
	unsigned	msg_flags;
};

/*
 *	POSIX 1003.1g - ancillary data object information
 *	Ancillary data consits of a sequence of pairs of
 *	(cmsghdr, cmsg_data[])
 */

struct cmsghdr {
	__kernel_size_t	cmsg_len;	/* data byte count, including hdr */
        int		cmsg_level;	/* originating protocol */
        int		cmsg_type;	/* protocol-specific type */
};

/*
 *	Ancilliary data object information MACROS
 *	Table 5-14 of POSIX 1003.1g
 */

#define __CMSG_NXTHDR(ctl, len, cmsg) __cmsg_nxthdr((ctl),(len),(cmsg))
#define CMSG_NXTHDR(mhdr, cmsg) cmsg_nxthdr((mhdr), (cmsg))

#define CMSG_ALIGN(len) ( ((len)+sizeof(long)-1) & ~(sizeof(long)-1) )

#define CMSG_DATA(cmsg)	((void *)((char *)(cmsg) + CMSG_ALIGN(sizeof(struct cmsghdr))))
#define CMSG_SPACE(len) (CMSG_ALIGN(sizeof(struct cmsghdr)) + CMSG_ALIGN(len))
#define CMSG_LEN(len) (CMSG_ALIGN(sizeof(struct cmsghdr)) + (len))

#define __CMSG_FIRSTHDR(ctl,len) ((len) >= sizeof(struct cmsghdr) ? \
				  (struct cmsghdr *)(ctl) : \
				  (struct cmsghdr *)NULL)
#define CMSG_FIRSTHDR(msg)	__CMSG_FIRSTHDR((msg)->msg_control, (msg)->msg_controllen)
#define CMSG_OK(mhdr, cmsg) ((cmsg)->cmsg_len >= sizeof(struct cmsghdr) && \
			     (cmsg)->cmsg_len <= (unsigned long) \
			     ((mhdr)->msg_controllen - \
			      ((char *)(cmsg) - (char *)(mhdr)->msg_control)))

/*
 *	Get the next cmsg header
 *
 *	PLEASE, do not touch this function. If you think, that it is
 *	incorrect, grep kernel sources and think about consequences
 *	before trying to improve it.
 *
 *	Now it always returns valid, not truncated ancillary object
 *	HEADER. But caller still MUST check, that cmsg->cmsg_len is
 *	inside range, given by msg->msg_controllen before using
 *	ancillary object DATA.				--ANK (980731)
 */
 
static inline struct cmsghdr * __cmsg_nxthdr(void *__ctl, __kernel_size_t __size,
					       struct cmsghdr *__cmsg)
{
	struct cmsghdr * __ptr;

	__ptr = (struct cmsghdr*)(((unsigned char *) __cmsg) +  CMSG_ALIGN(__cmsg->cmsg_len));
	if ((unsigned long)((char*)(__ptr+1) - (char *) __ctl) > __size)
		return (struct cmsghdr *)0;

	return __ptr;
}

static inline struct cmsghdr * cmsg_nxthdr (struct msghdr *__msg, struct cmsghdr *__cmsg)
{
	return __cmsg_nxthdr(__msg->msg_control, __msg->msg_controllen, __cmsg);
}

/* "Socket"-level control message types: */

#define	SCM_RIGHTS	0x01		/* rw: access rights (array of int) */
#define SCM_CREDENTIALS 0x02		/* rw: struct ucred		*/
#define SCM_SECURITY	0x03		/* rw: security label		*/

struct ucred {
	__u32	pid;
	__u32	uid;
	__u32	gid;
};

//linux 支持的协议族
/* Supported address families. */
#define AF_UNSPEC	0	/* 无特定地址族 */
#define AF_UNIX		1	/* Unix 域套接字		*/
#define AF_LOCAL	1	/* AF_UNIX 的POSIX 名 	*/
#define AF_INET		2	/* Internet,  TCP/IP 协议族 	*/
#define AF_AX25		3	/* AX.25 协议		*/
#define AF_IPX		4	/* Novell IPX 	协议套接字		*/
#define AF_APPLETALK	5	/* AppleTalk DDP 	套接字	*/
#define AF_NETROM	6	/*  NET/ROM 	套接字 */
#define AF_BRIDGE	7	/* 多协议桥套接字 	*/
#define AF_ATMPVC	8	/* ATM PVCs	套接字		*/
#define AF_X25		9	/* 为 X.25 套接字预留 	*/
#define AF_INET6	10	/* IP V6	套接字		*/
#define AF_ROSE		11	/* X.25 PLP	套接字*/
#define AF_DECnet	12	/* 为DECnet 预留的套接字	*/
#define AF_NETBEUI	13	/* 为802.2LLC 预留的套接字*/
#define AF_SECURITY	14	/* 安全回调 pseudo 地址族 */
#define AF_KEY		15      /* PF_KEY 关键管理API 套接字 */
#define AF_NETLINK	16	/* NETLINK 协议栈，在应用层与内核之间传递信息 */
#define AF_ROUTE	AF_NETLINK /* 仿4.4BSD 路由套接字的别名，与linux中的AF_NETLINK一样 */
#define AF_PACKET	17	/* Packet 族套接字		*/
#define AF_ASH		18	/* Ash	套接字			*/
#define AF_ECONET	19	/* Acorn Econet	套接字		*/
#define AF_ATMSVC	20	/* ATM SVCs	套接字		*/
#define AF_RDS		21	/* RDS 套接字 			*/
#define AF_SNA		22	/* Linux SNA SVC 套接字 */
#define AF_IRDA		23	/* IRDA 套接字			*/
#define AF_PPPOX	24	/* PPPoX 套接字		*/
#define AF_WANPIPE	25	/* Wanpipe API 套接字 */
#define AF_LLC		26	/* Linux LLC	套接字		*/
#define AF_CAN		29	/* Controller Area Network   (CAN)套接字   */
#define AF_TIPC		30	/* TIPC 套接字			*/
#define AF_BLUETOOTH	31	/* Bluetooth 套接字 		*/
#define AF_IUCV		32	/* IUCV 套接字			*/
#define AF_RXRPC	33	/* RxRPC 套接字 		*/
#define AF_ISDN		34	/* mISDN 套接字 		*/
#define AF_PHONET	35	/* Phonet 套接字		*/
#define AF_IEEE802154	36	/* IEEE802154 sockets		*/
#define AF_MAX		37	/* 目前未用 */

/* Protocol families, same as address families. */
#define PF_UNSPEC	AF_UNSPEC
#define PF_UNIX		AF_UNIX
#define PF_LOCAL	AF_LOCAL
#define PF_INET		AF_INET
#define PF_AX25		AF_AX25
#define PF_IPX		AF_IPX
#define PF_APPLETALK	AF_APPLETALK
#define	PF_NETROM	AF_NETROM
#define PF_BRIDGE	AF_BRIDGE
#define PF_ATMPVC	AF_ATMPVC
#define PF_X25		AF_X25
#define PF_INET6	AF_INET6
#define PF_ROSE		AF_ROSE
#define PF_DECnet	AF_DECnet
#define PF_NETBEUI	AF_NETBEUI
#define PF_SECURITY	AF_SECURITY
#define PF_KEY		AF_KEY
#define PF_NETLINK	AF_NETLINK
#define PF_ROUTE	AF_ROUTE
#define PF_PACKET	AF_PACKET
#define PF_ASH		AF_ASH
#define PF_ECONET	AF_ECONET
#define PF_ATMSVC	AF_ATMSVC
#define PF_RDS		AF_RDS
#define PF_SNA		AF_SNA
#define PF_IRDA		AF_IRDA
#define PF_PPPOX	AF_PPPOX
#define PF_WANPIPE	AF_WANPIPE
#define PF_LLC		AF_LLC
#define PF_CAN		AF_CAN
#define PF_TIPC		AF_TIPC
#define PF_BLUETOOTH	AF_BLUETOOTH
#define PF_IUCV		AF_IUCV
#define PF_RXRPC	AF_RXRPC
#define PF_ISDN		AF_ISDN
#define PF_PHONET	AF_PHONET
#define PF_IEEE802154	AF_IEEE802154
#define PF_MAX		AF_MAX

/* Maximum queue length specifiable by listen.  */
#define SOMAXCONN	128

/* Flags we can use with send/ and recv. 
   Added those for 1003.1g not all are supported yet
 */
 
#define MSG_OOB		1
#define MSG_PEEK	2
#define MSG_DONTROUTE	4
#define MSG_TRYHARD     4       /* Synonym for MSG_DONTROUTE for DECnet */
#define MSG_CTRUNC	8
#define MSG_PROBE	0x10	/* Do not send. Only probe path f.e. for MTU */
#define MSG_TRUNC	0x20
#define MSG_DONTWAIT	0x40	/* Nonblocking io		 */
#define MSG_EOR         0x80	/* End of record */
#define MSG_WAITALL	0x100	/* Wait for a full request */
#define MSG_FIN         0x200
#define MSG_SYN		0x400
#define MSG_CONFIRM	0x800	/* Confirm path validity */
#define MSG_RST		0x1000
#define MSG_ERRQUEUE	0x2000	/* Fetch message from error queue */
#define MSG_NOSIGNAL	0x4000	/* Do not generate SIGPIPE */
#define MSG_MORE	0x8000	/* Sender will send more */
#define MSG_SENDPAGE_NOTLAST 0x20000 /* sendpage() internal : not the last page */
#define MSG_EOF         MSG_FIN

#define MSG_CMSG_CLOEXEC 0x40000000	/* Set close_on_exit for file
					   descriptor received through
					   SCM_RIGHTS */
#if defined(CONFIG_COMPAT)
#define MSG_CMSG_COMPAT	0x80000000	/* This message needs 32 bit fixups */
#else
#define MSG_CMSG_COMPAT	0		/* We never have 32 bit fixups */
#endif


/* Setsockoptions(2) level. Thanks to BSD these must match IPPROTO_xxx */
#define SOL_IP		0
/* #define SOL_ICMP	1	No-no-no! Due to Linux :-) we cannot use SOL_ICMP=1 */
#define SOL_TCP		6
#define SOL_UDP		17
#define SOL_IPV6	41
#define SOL_ICMPV6	58
#define SOL_SCTP	132
#define SOL_UDPLITE	136     /* UDP-Lite (RFC 3828) */
#define SOL_RAW		255
#define SOL_IPX		256
#define SOL_AX25	257
#define SOL_ATALK	258
#define SOL_NETROM	259
#define SOL_ROSE	260
#define SOL_DECNET	261
#define	SOL_X25		262
#define SOL_PACKET	263
#define SOL_ATM		264	/* ATM layer (cell level) */
#define SOL_AAL		265	/* ATM Adaption Layer (packet level) */
#define SOL_IRDA        266
#define SOL_NETBEUI	267
#define SOL_LLC		268
#define SOL_DCCP	269
#define SOL_NETLINK	270
#define SOL_TIPC	271
#define SOL_RXRPC	272
#define SOL_PPPOL2TP	273
#define SOL_BLUETOOTH	274
#define SOL_PNPIPE	275
#define SOL_RDS		276
#define SOL_IUCV	277

/* IPX options */
#define IPX_TYPE	1

#ifdef __KERNEL__
extern int memcpy_fromiovec(unsigned char *kdata, struct iovec *iov, int len);
extern int memcpy_fromiovecend(unsigned char *kdata, const struct iovec *iov,
			       int offset, int len);
extern int csum_partial_copy_fromiovecend(unsigned char *kdata, 
					  struct iovec *iov, 
					  int offset, 
					  unsigned int len, __wsum *csump);

extern int verify_iovec(struct msghdr *m, struct iovec *iov, struct sockaddr *address, int mode);
extern int memcpy_toiovec(struct iovec *v, unsigned char *kdata, int len);
extern int memcpy_toiovecend(const struct iovec *v, unsigned char *kdata,
			     int offset, int len);
extern int move_addr_to_user(struct sockaddr *kaddr, int klen, void __user *uaddr, int __user *ulen);
extern int move_addr_to_kernel(void __user *uaddr, int ulen, struct sockaddr *kaddr);
extern int put_cmsg(struct msghdr*, int level, int type, int len, void *data);

#endif
#endif /* not kernel and not glibc */
#endif /* _LINUX_SOCKET_H */
