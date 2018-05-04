/*
 * INET		An implementation of the TCP/IP protocol suite for the LINUX
 *		operating system.  INET is implemented using the  BSD Socket
 *		interface as the means of communication with the user level.
 *
 *		Definitions for the UDP protocol.
 *
 * Version:	@(#)udp.h	1.0.2	04/28/93
 *
 * Author:	Fred N. van Kempen, <waltje@uWalt.NL.Mugnet.ORG>
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 */
#ifndef _LINUX_UDP_H
#define _LINUX_UDP_H

#include <linux/types.h>

//udpÊý¾Ý±¨Ð­ÒéÍ·
struct udphdr {
	__be16	source;	//Ô´¶Ë¿Ú
	__be16	dest;	//Ä¿µÄ¶Ë¿Ú
	__be16	len;		// Êý¾Ý°ü³¤¶È
	__sum16	check;	// UDPÐ£ÑéºÍµÄ¼ÆËã°üÀ¨Ð­ÒéÍ·ºÍÊý¾Ý
};

/* UDP socket options */
#define UDP_CORK	1	/* Never send partially complete segments */
#define UDP_ENCAP	100	/* Set the socket to accept encapsulated packets */

/* UDP encapsulation types */
#define UDP_ENCAP_ESPINUDP_NON_IKE	1 /* draft-ietf-ipsec-nat-t-ike-00/01 */
#define UDP_ENCAP_ESPINUDP	2 /* draft-ietf-ipsec-udp-encaps-06 */
#define UDP_ENCAP_L2TPINUDP	3 /* rfc2661 */

#ifdef __KERNEL__
#include <net/inet_sock.h>
#include <linux/skbuff.h>
#include <net/netns/hash.h>

static inline struct udphdr *udp_hdr(const struct sk_buff *skb)
{
	return (struct udphdr *)skb_transport_header(skb);
}

#define UDP_HTABLE_SIZE		128

static inline int udp_hashfn(struct net *net, const unsigned num)
{
	return (num + net_hash_mix(net)) & (UDP_HTABLE_SIZE - 1);
}

//udpÐ­Òé½á¹¹Ìå
struct udp_sock {
	/* inet_sock has to be the first member */
	struct inet_sock inet;		//ÔÚstruct inet_sock½á¹¹»ù´¡ÉÏÌí¼ÓåudpÐ­Òé×¨ÓÐÊôÐÔ
	int		 pending;		/* µ±Ç°ÊÇ·ñÓÐµÈ´ý·¢ËÍµÄÊý¾Ý±¨ü*/
	unsigned int	 corkflag;	/* ÊÇ·ñÐèÒªÔÝÊ±×èÈûÌ×½Ó×Ö */
  	__u16		 encap_type;	/* Is this an Encapsulation socket? */
	/*
	 * Following member retains the information to create a UDP header
	 * when the socket is uncorked.
	 */
	__u16		 len;		/*´ý·¢ËÍÊý¾Ý±¨×Ü³¤¶È */
	/*
	 * Fields specific to UDP-Lite.
	 */
	__u16		 pcslen;		/*for udp-lite socket ,¼ÇÂ¼´ý·¢ËÍÊý¾Ý°ü³¤¶È*/
	__u16		 pcrlen;		/*for udp-lite socket,¼ÇÂ¼´ý·¢ËÍÊý¾Ý°ü³¤¶È*/
/* indicator bits used by pcflag: */
#define UDPLITE_BIT      0x1  		/* set by udplite proto init function */
#define UDPLITE_SEND_CC  0x2  		/* set via udplite setsockopt         */
#define UDPLITE_RECV_CC  0x4		/* set via udplite setsocktopt        */
	__u8		 pcflag;        /* ±ê¼Ç¸ÃÌ×½Ó×ÖÊÇ·ñÎª udp-liteÐ­ÒéÌ×½Ó×Ö if > 0    */
	__u8		 unused[3];
	/*
	 * For encapsulation sockets.
	 */
	int (*encap_rcv)(struct sock *sk, struct sk_buff *skb);
};
/*
	Udp-liteÊÊÓÃÓÚÍøÂç²î´íÂÊ±È½Ï´ó£¬µ«ÊÇ¶ÔÇáÎ¢²î´í²»Ãô¸ÐµÄÇé¿öÏÂ£¬
	ÈçÊµÊ±ÊÓÆµµÄ´«Êä¡£Linux¶Ôudp-liteÐ­ÒéµÄÖ§³ÖÒ²ÊÇÍ¨¹ýÔÚÔ­À´µÄudpÐ­
	ÒéµÄ»ù´¡ÉÏÌí¼ÓÁËÒ»¸ösetsockoptÑ¡ÏîÀ´ÊµÏÖ¿ØÖÆ·¢ËÍºÍ½ÓÊÕµÄchecksum coverage¡£
	Int val = 20;
	Setsockopt(s, SOL_UDPLITE,  UDPLITE_SEND_CSCOV,  &val,  sizeof(val));
	
	Int min = 20;
	Setsockopt(s, SOL_UDPLITE,  UDPLITE_RECV_CSCOV,  &min,  sizeof(min));
	´´½¨Ò»¸öUDP-LiteÌ×½Ó×Ö£º
	S = socket(PF_INET,  SOCK_DGRAM, IPPROTO_UDPLITE);

*/


static inline struct udp_sock *udp_sk(const struct sock *sk)
{
	return (struct udp_sock *)sk;
}

#define IS_UDPLITE(__sk) (udp_sk(__sk)->pcflag)

#endif

#endif	/* _LINUX_UDP_H */
