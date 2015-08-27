cmd_arch/arm/kernel/entry-common.o := arm-linux-gcc -Wp,-MD,arch/arm/kernel/.entry-common.o.d  -nostdinc -isystem /home/xm/FriendlyARM/toolschain/4.5.1/bin/../lib/gcc/arm-none-linux-gnueabi/4.5.1/include -Iinclude  -I/home/xm/KuaiPan/kernel-update/kernel/linux-2.6.32.67/arch/arm/include -include include/linux/autoconf.h -D__KERNEL__ -mlittle-endian -Iarch/arm/mach-s3c6400/include -Iarch/arm/mach-s3c6410/include -Iarch/arm/plat-s3c64xx/include -Iarch/arm/plat-s3c/include -D__ASSEMBLY__ -mabi=aapcs-linux -mno-thumb-interwork -funwind-tables  -D__LINUX_ARM_ARCH__=6 -march=armv6k -mtune=arm1136j-s -include asm/unified.h -msoft-float -gdwarf-2       -c -o arch/arm/kernel/entry-common.o arch/arm/kernel/entry-common.S

deps_arch/arm/kernel/entry-common.o := \
  arch/arm/kernel/entry-common.S \
    $(wildcard include/config/function/tracer.h) \
    $(wildcard include/config/dynamic/ftrace.h) \
    $(wildcard include/config/cpu/arm710.h) \
    $(wildcard include/config/oabi/compat.h) \
    $(wildcard include/config/arm/thumb.h) \
    $(wildcard include/config/cpu/endian/be8.h) \
    $(wildcard include/config/aeabi.h) \
    $(wildcard include/config/alignment/trap.h) \
  /home/xm/KuaiPan/kernel-update/kernel/linux-2.6.32.67/arch/arm/include/asm/unified.h \
    $(wildcard include/config/arm/asm/unified.h) \
    $(wildcard include/config/thumb2/kernel.h) \
  /home/xm/KuaiPan/kernel-update/kernel/linux-2.6.32.67/arch/arm/include/asm/unistd.h \
  /home/xm/KuaiPan/kernel-update/kernel/linux-2.6.32.67/arch/arm/include/asm/ftrace.h \
    $(wildcard include/config/frame/pointer.h) \
    $(wildcard include/config/arm/unwind.h) \
  arch/arm/mach-s3c6400/include/mach/entry-macro.S \
  /home/xm/KuaiPan/kernel-update/kernel/linux-2.6.32.67/arch/arm/include/asm/hardware/vic.h \
  arch/arm/mach-s3c6400/include/mach/map.h \
  arch/arm/plat-s3c/include/plat/map-base.h \
  arch/arm/plat-s3c64xx/include/plat/irqs.h \
  /home/xm/KuaiPan/kernel-update/kernel/linux-2.6.32.67/arch/arm/include/asm/unwind.h \
  arch/arm/kernel/entry-header.S \
    $(wildcard include/config/cpu/32v6k.h) \
    $(wildcard include/config/cpu/v6.h) \
  include/linux/init.h \
    $(wildcard include/config/modules.h) \
    $(wildcard include/config/hotplug.h) \
  include/linux/compiler.h \
    $(wildcard include/config/trace/branch/profiling.h) \
    $(wildcard include/config/profile/all/branches.h) \
    $(wildcard include/config/enable/must/check.h) \
    $(wildcard include/config/enable/warn/deprecated.h) \
  include/linux/linkage.h \
  /home/xm/KuaiPan/kernel-update/kernel/linux-2.6.32.67/arch/arm/include/asm/linkage.h \
  /home/xm/KuaiPan/kernel-update/kernel/linux-2.6.32.67/arch/arm/include/asm/assembler.h \
    $(wildcard include/config/cpu/feroceon.h) \
    $(wildcard include/config/trace/irqflags.h) \
    $(wildcard include/config/smp.h) \
  /home/xm/KuaiPan/kernel-update/kernel/linux-2.6.32.67/arch/arm/include/asm/ptrace.h \
  /home/xm/KuaiPan/kernel-update/kernel/linux-2.6.32.67/arch/arm/include/asm/hwcap.h \
  include/asm/asm-offsets.h \
  /home/xm/KuaiPan/kernel-update/kernel/linux-2.6.32.67/arch/arm/include/asm/errno.h \
  include/asm-generic/errno.h \
  include/asm-generic/errno-base.h \
  /home/xm/KuaiPan/kernel-update/kernel/linux-2.6.32.67/arch/arm/include/asm/thread_info.h \
    $(wildcard include/config/arm/thumbee.h) \
  /home/xm/KuaiPan/kernel-update/kernel/linux-2.6.32.67/arch/arm/include/asm/fpstate.h \
    $(wildcard include/config/vfpv3.h) \
    $(wildcard include/config/iwmmxt.h) \
  arch/arm/kernel/calls.S \

arch/arm/kernel/entry-common.o: $(deps_arch/arm/kernel/entry-common.o)

$(deps_arch/arm/kernel/entry-common.o):
