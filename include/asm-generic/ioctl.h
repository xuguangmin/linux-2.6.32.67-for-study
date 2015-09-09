#ifndef _ASM_GENERIC_IOCTL_H
#define _ASM_GENERIC_IOCTL_H

/* ioctl command encoding: 32 bits total, command in lower 16 bits,
 * size of the parameter structure in the lower 14 bits of the
 * upper 16 bits.
 * Encoding the size of the parameter structure in the ioctl request
 * is useful for catching programs compiled with old versions
 * and to avoid overwriting user space outside the user buffer area.
 * The highest 2 bits are reserved for indicating the ``access mode''.
 * NOTE: This limits the max parameter size to 16kB -1 !
 */

/*
 * The following is for compatibility across the various Linux
 * platforms.  The generic ioctl numbering scheme doesn't really enforce
 * a type field.  De facto, however, the top 8 bits of the lower 16
 * bits are indeed used as a type field, so we might just as well make
 * this explicit here.  Please be sure to use the decoding macros
 * below from now on.
 */

/* 为构造ioctl的cmd参数,内核使用了一个32位无符号整数并将其分成四个部分

   31   29            16 15            8 7              0
   |DIR |     SIZE      |     TYPE      |      NR       |

 * NR:为功能号,长度为8位(_IOC_NRBITS)
 * TYPE:为一ASCII字符,假定对每个驱动程序而言都是唯一的,长度是8位(_IOC_TYPEBITS)
 *      实际的宏定义中常常含有MAGIC字样,所以有时也称为魔数
 * SIZE:表示ioctl调用中arg参数的大小,该字段的长度与体系架构相关,通常是14位
        (_IOC_SIZEBITS),其实内核在ioctl的调用中并没有用到该字段
 * DIR:表示cmd的类型:read,write和read-write,长度是2位,这个字段用于表示在ioctl
       调用过程中用户空间和内核空间数据传输的方向,此处方向的定义是从用户空间的
       视角触发,内核为该字段定义的宏有:_IOC_NONE,表示在ioctl调用过程中,用户空间
       和内核空间没有需要传递的参数:_IOC_WRITE,表示在ioctl调用过程中,用户空间
       需要向内核空间写入数据;_IOC_READ,表示在ioctl调用过程中,用户空间需要从内核
       空间读取数据;_IOC_WRITE|_IOC_READ,表示在ioctl调用过程中,参数数据在用户
       空间和内核空间进行双向传递
 */

 /* NR:为功能号,长度为8位(_IOC_NRBITS)*/
#define _IOC_NRBITS	8
 /* TYPE:为一ASCII字符,假定对每个驱动程序而言都是唯一的,长度是8位(_IOC_TYPEBITS)
 *  实际的宏定义中常常含有MAGIC字样,所以有时也称为魔数*/
#define _IOC_TYPEBITS	8

/*
 * Let any architecture override either of the following before
 * including this file.
 */

 /* SIZE:表示ioctl调用中arg参数的大小,该字段的长度与体系架构相关,通常是14位
  * (_IOC_SIZEBITS),其实内核在ioctl的调用中并没有用到该字段*/
#ifndef _IOC_SIZEBITS
# define _IOC_SIZEBITS	14
#endif

 /* DIR:表示cmd的类型:read,write和read-write,长度是2位,这个字段用于表示在ioctl
  * 调用过程中用户空间和内核空间数据传输的方向,此处方向的定义是从用户空间的
  * 视角触发*/
#ifndef _IOC_DIRBITS
# define _IOC_DIRBITS	2
#endif

#define _IOC_NRMASK	((1 << _IOC_NRBITS)-1)	/*NR字段的掩码*/
#define _IOC_TYPEMASK	((1 << _IOC_TYPEBITS)-1)/*TYPE字段的掩码*/
#define _IOC_SIZEMASK	((1 << _IOC_SIZEBITS)-1)/*SIZE字段的掩码*/
#define _IOC_DIRMASK	((1 << _IOC_DIRBITS)-1)	/*CMD字段的掩码*/

#define _IOC_NRSHIFT	0				/*NR字段的位移*/
#define _IOC_TYPESHIFT	(_IOC_NRSHIFT+_IOC_NRBITS)	/*TYPE字段的位移*/
#define _IOC_SIZESHIFT	(_IOC_TYPESHIFT+_IOC_TYPEBITS)	/*SIZE字段的位移*/
#define _IOC_DIRSHIFT	(_IOC_SIZESHIFT+_IOC_SIZEBITS)	/*CMD字段的位移*/

/*
 * Direction bits, which any architecture can choose to override
 * before including this file.
 */

       
       
/*表示在ioctl调用过程中,用户空间和内核空间没有需要传递的参数.*/
#ifndef _IOC_NONE
# define _IOC_NONE	0U	
#endif

/*表示在ioctl调用过程中,用户空间需要向内核空间写入数据*/
#ifndef _IOC_WRITE
# define _IOC_WRITE	1U
#endif

/*表示在ioctl调用过程中,用户空间需要从内核空间读取数据*/
#ifndef _IOC_READ
# define _IOC_READ	2U
#endif

/*宏_IOC将NR,TYPE,SIZE和DIR组合成一个cmd参数*/
#define _IOC(dir,type,nr,size) \
	(((dir)  << _IOC_DIRSHIFT) | \
	 ((type) << _IOC_TYPESHIFT) | \
	 ((nr)   << _IOC_NRSHIFT) | \
	 ((size) << _IOC_SIZESHIFT))

#ifdef __KERNEL__
/* provoke compile error for invalid uses of size argument */
extern unsigned int __invalid_size_argument_for_IOC;
/*对宏参数size进行检测,只在定义了__KERNEL__的情况下有效,否则退化为sizeof运算符*/
#define _IOC_TYPECHECK(t) \
	((sizeof(t) == sizeof(t[1]) && \
	  sizeof(t) < (1 << _IOC_SIZEBITS)) ? \
	  sizeof(t) : __invalid_size_argument_for_IOC)
#else
#define _IOC_TYPECHECK(t) (sizeof(t))
#endif

/**
 * EXAMPLE:DEMODEV_IOCINT,该命令从用户空间向内核空间传递一个int型参数
 *  
 */
/* used to create numbers */
/*构造无参数的命令编号*/
#define _IO(type,nr)		_IOC(_IOC_NONE,(type),(nr),0)
/*构造从驱动程序中读取数据的命令编号*/
#define _IOR(type,nr,size)	_IOC(_IOC_READ,(type),(nr),(_IOC_TYPECHECK(size)))
/*构造向驱动程序中写入数据的命令编号*/
#define _IOW(type,nr,size)	_IOC(_IOC_WRITE,(type),(nr),(_IOC_TYPECHECK(size)))
/*用于双向传输*/
#define _IOWR(type,nr,size)	_IOC(_IOC_READ|_IOC_WRITE,(type),(nr),(_IOC_TYPECHECK(size)))
#define _IOR_BAD(type,nr,size)	_IOC(_IOC_READ,(type),(nr),sizeof(size))
#define _IOW_BAD(type,nr,size)	_IOC(_IOC_WRITE,(type),(nr),sizeof(size))
#define _IOWR_BAD(type,nr,size)	_IOC(_IOC_READ|_IOC_WRITE,(type),(nr),sizeof(size))

/* used to decode ioctl numbers.. */
/*从命令参数中解析出数据方向，即写进还是读出*/
#define _IOC_DIR(nr)		(((nr) >> _IOC_DIRSHIFT) & _IOC_DIRMASK)
/*从命令参数中解析出幻数type*/
#define _IOC_TYPE(nr)		(((nr) >> _IOC_TYPESHIFT) & _IOC_TYPEMASK)
/*从命令参数中解析出序数number*/
#define _IOC_NR(nr)		(((nr) >> _IOC_NRSHIFT) & _IOC_NRMASK)
/*从命令参数中解析出用户数据大小*/
#define _IOC_SIZE(nr)		(((nr) >> _IOC_SIZESHIFT) & _IOC_SIZEMASK)

/* ...and for the drivers/sound files... */

#define IOC_IN		(_IOC_WRITE << _IOC_DIRSHIFT)
#define IOC_OUT		(_IOC_READ << _IOC_DIRSHIFT)
#define IOC_INOUT	((_IOC_WRITE|_IOC_READ) << _IOC_DIRSHIFT)
#define IOCSIZE_MASK	(_IOC_SIZEMASK << _IOC_SIZESHIFT)
#define IOCSIZE_SHIFT	(_IOC_SIZESHIFT)

#endif /* _ASM_GENERIC_IOCTL_H */
