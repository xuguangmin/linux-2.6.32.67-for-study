/*
 * kernel userspace event delivery
 *
 * Copyright (C) 2004 Red Hat, Inc.  All rights reserved.
 * Copyright (C) 2004 Novell, Inc.  All rights reserved.
 * Copyright (C) 2004 IBM, Inc. All rights reserved.
 *
 * Licensed under the GNU GPL v2.
 *
 * Authors:
 *	Robert Love		<rml@novell.com>
 *	Kay Sievers		<kay.sievers@vrfy.org>
 *	Arjan van de Ven	<arjanv@redhat.com>
 *	Greg Kroah-Hartman	<greg@kroah.com>
 */

#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/kobject.h>
#include <linux/module.h>

#include <linux/socket.h>
#include <linux/skbuff.h>
#include <linux/netlink.h>
#include <net/sock.h>


u64 uevent_seqnum;
/*通过调用call_usermodehelper来达到从内核空间运行一个用户空间进程的目的,用户空间进程的
 *二进制文件由uevent_helper提供,该变量是一字符数组*/
char uevent_helper[UEVENT_HELPER_PATH_LEN] = CONFIG_UEVENT_HELPER_PATH;
static DEFINE_SPINLOCK(sequence_lock);
#if defined(CONFIG_NET)
static struct sock *uevent_sock;
#endif

/* the strings here must match the enum in include/linux/kobject.h */
static const char *kobject_actions[] = {
	[KOBJ_ADD] =		"add",
	[KOBJ_REMOVE] =		"remove",
	[KOBJ_CHANGE] =		"change",
	[KOBJ_MOVE] =		"move",
	[KOBJ_ONLINE] =		"online",
	[KOBJ_OFFLINE] =	"offline",
};

/**
 * kobject_action_type - translate action string to numeric type
 *
 * @buf: buffer containing the action string, newline is ignored
 * @len: length of buffer
 * @type: pointer to the location to store the action type
 *
 * Returns 0 if the action string was recognized.
 */
int kobject_action_type(const char *buf, size_t count,
			enum kobject_action *type)
{
	enum kobject_action action;
	int ret = -EINVAL;

	if (count && (buf[count-1] == '\n' || buf[count-1] == '\0'))
		count--;

	if (!count)
		goto out;

	for (action = 0; action < ARRAY_SIZE(kobject_actions); action++) {
		if (strncmp(kobject_actions[action], buf, count) != 0)
			continue;
		if (kobject_actions[action][count] != '\0')
			continue;
		*type = action;
		ret = 0;
		break;
	}
out:
	return ret;
}

/**
 * kobject_uevent_env - send an uevent with environmental data
 *
 * @action: action that is happening
 * @kobj: struct kobject that the action is happening to
 * @envp_ext: pointer to environmental data
 *
 * Returns 0 if kobject_uevent() is completed with success or the
 * corresponding error when it fails.
 */
 /*kobject_uevent的核心功能函数*/
int kobject_uevent_env(struct kobject *kobj, enum kobject_action action,
		       char *envp_ext[])
{
	struct kobj_uevent_env *env;
	const char *action_string = kobject_actions[action];
	const char *devpath = NULL;
	const char *subsystem;
	struct kobject *top_kobj;
	struct kset *kset;
	struct kset_uevent_ops *uevent_ops;
	u64 seq;
	int i = 0;
	int retval = 0;

	pr_debug("kobject: '%s' (%p): %s\n",
		 kobject_name(kobj), kobj, __func__);

	/* search the kset we belong to */
	/*此处的while循环用来查找kobj所隶属的最顶层kset*/
	top_kobj = kobj;
	while (!top_kobj->kset && top_kobj->parent)
		top_kobj = top_kobj->parent;

	/*如果当前kobj没有隶属的kset,那么它将不能使用uevent机制*/
	if (!top_kobj->kset) {
		pr_debug("kobject: '%s' (%p): %s: attempted to send uevent "
			 "without kset!\n", kobject_name(kobj), kobj,
			 __func__);
		return -EINVAL;
	}

	/*得到kobj所属的顶层kset的uevent操作集对象uevent_ops*/
	kset = top_kobj->kset;
	uevent_ops = kset->uevent_ops;

	/* skip the event, if uevent_suppress is set*/
	/*如果kobj->uevent_suppress=1表明该kobj不希望使用uevent机制*/
	if (kobj->uevent_suppress) {
		pr_debug("kobject: '%s' (%p): %s: uevent_suppress "
				 "caused the event to drop!\n",
				 kobject_name(kobj), kobj, __func__);
		return 0;
	}
	/* skip the event, if the filter returns zero. */
	/*首先调用filter函数,如果函数返回0,表明kobj希望发送的event消息被顶层kset过滤掉了*/
	if (uevent_ops && uevent_ops->filter)
		if (!uevent_ops->filter(kset, kobj)) {
			pr_debug("kobject: '%s' (%p): %s: filter function "
				 "caused the event to drop!\n",
				 kobject_name(kobj), kobj, __func__);
			return 0;
		}

	/* originating subsystem */
	if (uevent_ops && uevent_ops->name)
		subsystem = uevent_ops->name(kset, kobj);
	else
		subsystem = kobject_name(&kset->kobj);
	if (!subsystem) {
		pr_debug("kobject: '%s' (%p): %s: unset subsystem caused the "
			 "event to drop!\n", kobject_name(kobj), kobj,
			 __func__);
		return 0;
	}

	/*准备使用uevent机制向用户空间发送event消息,通过add_uevent_var添加环境变量信息*/
	/* environment buffer */
	env = kzalloc(sizeof(struct kobj_uevent_env), GFP_KERNEL);
	if (!env)
		return -ENOMEM;

	/* complete object path */
	devpath = kobject_get_path(kobj, GFP_KERNEL);
	if (!devpath) {
		retval = -ENOENT;
		goto exit;
	}

	/* default keys */
	retval = add_uevent_var(env, "ACTION=%s", action_string);
	if (retval)
		goto exit;
	retval = add_uevent_var(env, "DEVPATH=%s", devpath);
	if (retval)
		goto exit;
	retval = add_uevent_var(env, "SUBSYSTEM=%s", subsystem);
	if (retval)
		goto exit;

	/* keys passed in from the caller */
	if (envp_ext) {
		for (i = 0; envp_ext[i]; i++) {
			retval = add_uevent_var(env, "%s", envp_ext[i]);
			if (retval)
				goto exit;
		}
	}

	/*此处在向用户空间发送event消息之前,给kset最后一次机会以完成一些私人事情*/
	/* let the kset specific function add its stuff */
	if (uevent_ops && uevent_ops->uevent) {
		/*处理完环境变量之后,调用kset对象的uevent_ops操作集中的uevent函数,这是
		 *内核赋予kset通过该函数完成自己特定功能的最后一次机会*/
		retval = uevent_ops->uevent(kset, kobj, env);
		if (retval) {
			pr_debug("kobject: '%s' (%p): %s: uevent() returned "
				 "%d\n", kobject_name(kobj), kobj,
				 __func__, retval);
			goto exit;
		}
	}

	/*
	 * Mark "add" and "remove" events in the object to ensure proper
	 * events to userspace during automatic cleanup. If the object did
	 * send an "add" event, "remove" will automatically generated by
	 * the core, if not already done by the caller.
	 */
	if (action == KOBJ_ADD)
		kobj->state_add_uevent_sent = 1;
	else if (action == KOBJ_REMOVE)
		kobj->state_remove_uevent_sent = 1;

	/* we will send an event, so request a new sequence number */
	spin_lock(&sequence_lock);
	seq = ++uevent_seqnum;
	spin_unlock(&sequence_lock);
	retval = add_uevent_var(env, "SEQNUM=%llu", (unsigned long long)seq);
	if (retval)
		goto exit;

/*如果配置了CONFIG_NET宏,表明内核打算使用netlink机制实现uevent消息的发送,这部分代码通过
 *netlink的方式向用户空间广播当前kset对象中的uevent消息*/
#if defined(CONFIG_NET)
	/* send netlink message */
	if (uevent_sock) {
		struct sk_buff *skb;
		size_t len;

		/* allocate message with the maximum possible size */
		len = strlen(action_string) + strlen(devpath) + 2;
		skb = alloc_skb(len + env->buflen, GFP_KERNEL);
		if (skb) {
			char *scratch;

			/* add header */
			scratch = skb_put(skb, len);
			sprintf(scratch, "%s@%s", action_string, devpath);

			/* copy keys to our continuous event payload buffer */
			for (i = 0; i < env->envp_idx; i++) {
				len = strlen(env->envp[i]) + 1;
				scratch = skb_put(skb, len);
				strcpy(scratch, env->envp[i]);
			}

			NETLINK_CB(skb).dst_group = 1;
			retval = netlink_broadcast(uevent_sock, skb, 0, 1,
						   GFP_KERNEL);
			/* ENOBUFS should be handled in userspace */
			if (retval == -ENOBUFS || retval == -ESRCH)
				retval = 0;
		} else
			retval = -ENOMEM;
	}
#endif

	/*使用uevent_helper机制实现uevent*/
	/* call uevent_helper, usually only enabled during early boot */
	if (uevent_helper[0]) {
		char *argv [3];

		argv [0] = uevent_helper;
		argv [1] = (char *)subsystem;
		argv [2] = NULL;
		retval = add_uevent_var(env, "HOME=/");
		if (retval)
			goto exit;
		retval = add_uevent_var(env,
					"PATH=/sbin:/bin:/usr/sbin:/usr/bin");
		if (retval)
			goto exit;

		/*通过调用call_usermodehelper来达到从内核空间运行一个用户空间进程的目的,
		 *用户空间进程的二进制文件由uevent_helper提供,该变量是一字符数组*/
		retval = call_usermodehelper(argv[0], argv,
					     env->envp, UMH_WAIT_EXEC);
	}

exit:
	kfree(devpath);
	kfree(env);
	return retval;
}
EXPORT_SYMBOL_GPL(kobject_uevent_env);

/**
 * kobject_uevent - notify userspace by ending an uevent
 *
 * @action: action that is happening
 * @kobj: struct kobject that the action is happening to
 *
 * Returns 0 if kobject_uevent() is completed with success or the
 * corresponding error when it fails.
 */
/*热插拔在内核中通过kobject_uevent函数来实现,它通过发送一个uevent消息和调用
 *call_usermodulehelper来与用户名称空间进行沟通.kobject_uevent所实现的功能和linux系统中
 *用以实现热插拔的特性息息相关,它是udev和/sbin/hotplug等工具赖以工作的基石*/
int kobject_uevent(struct kobject *kobj, enum kobject_action action)
{
	/*枚举型变量,定义了kset对象的一些状态变化,此处使用的是KOBJ_ADD*/
	return kobject_uevent_env(kobj, action, NULL);
}
EXPORT_SYMBOL_GPL(kobject_uevent);

/**
 * add_uevent_var - add key value string to the environment buffer
 * @env: environment buffer structure
 * @format: printf format for the key=value pair
 *
 * Returns 0 if environment variable was added successfully or -ENOMEM
 * if no space was available.
 */
int add_uevent_var(struct kobj_uevent_env *env, const char *format, ...)
{
	va_list args;
	int len;

	if (env->envp_idx >= ARRAY_SIZE(env->envp)) {
		WARN(1, KERN_ERR "add_uevent_var: too many keys\n");
		return -ENOMEM;
	}

	va_start(args, format);
	len = vsnprintf(&env->buf[env->buflen],
			sizeof(env->buf) - env->buflen,
			format, args);
	va_end(args);

	if (len >= (sizeof(env->buf) - env->buflen)) {
		WARN(1, KERN_ERR "add_uevent_var: buffer size too small\n");
		return -ENOMEM;
	}

	env->envp[env->envp_idx++] = &env->buf[env->buflen];
	env->buflen += len + 1;
	return 0;
}
EXPORT_SYMBOL_GPL(add_uevent_var);

#if defined(CONFIG_NET)
static int __init kobject_uevent_init(void)
{
	uevent_sock = netlink_kernel_create(&init_net, NETLINK_KOBJECT_UEVENT,
					    1, NULL, NULL, THIS_MODULE);
	if (!uevent_sock) {
		printk(KERN_ERR
		       "kobject_uevent: unable to create netlink socket!\n");
		return -ENODEV;
	}
	netlink_set_nonroot(NETLINK_KOBJECT_UEVENT, NL_NONROOT_RECV);
	return 0;
}

postcore_initcall(kobject_uevent_init);
#endif
