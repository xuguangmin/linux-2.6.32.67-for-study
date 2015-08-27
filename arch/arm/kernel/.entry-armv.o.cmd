cmd_arch/arm/kernel/entry-armv.o := arm-linux-gcc -Wp,-MD,arch/arm/kernel/.entry-armv.o.d  -nostdinc -isystem /home/xm/FriendlyARM/toolschain/4.5.1/bin/../lib/gcc/arm-none-linux-gnueabi/4.5.1/include -Iinclude  -I/home/xm/KuaiPan/kernel-update/kernel/linux-2.6.32.67/arch/arm/include -include include/linux/autoconf.h -D__KERNEL__ -mlittle-endian -Iarch/arm/mach-s3c6400/include -Iarch/arm/mach-s3c6410/include -Iarch/arm/plat-s3c64xx/include -Iarch/arm/plat-s3c/include -D__ASSEMBLY__ -mabi=aapcs-linux -mno-thumb-interwork -funwind-tables  -D__LINUX_ARM_ARCH__=6 -march=armv6k -mtune=arm1136j-s -include asm/unified.h -msoft-float -gdwarf-2       -c -o arch/arm/kernel/entry-armv.o arch/arm/kernel/entry-armv.S

deps_arch/arm/kernel/entry-armv.o := \
  arch/arm/kernel/entry-armv.S \
    $(wildcard include/config/smp.h) \
    $(wildcard include/config/local/timers.h) \
    $(wildcard include/config/kprobes.h) \
    $(wildcard include/config/aeabi.h) \
    $(wildcard include/config/thumb2/kernel.h) \
    $(wildcard include/config/preempt.h) \
    $(wildcard include/config/trace/irqflags.h) \
    $(wildcard include/config/needs/syscall/for/cmpxchg.h) \
    $(wildcard include/config/mmu.h) \
    $(wildcard include/config/cpu/endian/be8.h) \
    $(wildcard include/config/neon.h) \
    $(wildcard include/config/cpu/arm610.h) \
    $(wildcard include/config/cpu/arm710.h) \
    $(wildcard include/config/iwmmxt.h) \
    $(wildcard include/config/crunch.h) \
    $(wildcard include/config/vfp.h) \
    $(wildcard include/config/has/tls/reg.h) \
    $(wildcard include/config/tls/reg/emul.h) \
    $(wildcard include/config/arm/thumb.h) \
  /home/xm/KuaiPan/kernel-update/kernel/linux-2.6.32.67/arch/arm/include/asm/unified.h \
    $(wildcard include/config/arm/asm/unified.h) \
  /home/xm/KuaiPan/kernel-update/kernel/linux-2.6.32.67/arch/arm/include/asm/memory.h \
    $(wildcard include/config/page/offset.h) \
    $(wildcard include/config/highmem.h) \
    $(wildcard include/config/dram/size.h) \
    $(wildcard include/config/dram/base.h) \
    $(wildcard include/config/zone/dma.h) \
    $(wildcard include/config/discontigmem.h) \
  include/linux/compiler.h \
    $(wildcard include/config/trace/branch/profiling.h) \
    $(wildcard include/config/profile/all/branches.h) \
    $(wildcard include/config/enable/must/check.h) \
    $(wildcard include/config/enable/warn/deprecated.h) \
  include/linux/const.h \
  arch/arm/mach-s3c6400/include/mach/memory.h \
  /home/xm/KuaiPan/kernel-update/kernel/linux-2.6.32.67/arch/arm/include/asm/sizes.h \
  include/asm-generic/memory_model.h \
    $(wildcard include/config/flatmem.h) \
    $(wildcard include/config/sparsemem/vmemmap.h) \
    $(wildcard include/config/sparsemem.h) \
  /home/xm/KuaiPan/kernel-update/kernel/linux-2.6.32.67/arch/arm/include/asm/glue.h \
    $(wildcard include/config/cpu/abrt/lv4t.h) \
    $(wildcard include/config/cpu/abrt/ev4.h) \
    $(wildcard include/config/cpu/abrt/ev4t.h) \
    $(wildcard include/config/cpu/abrt/ev5tj.h) \
    $(wildcard include/config/cpu/abrt/ev5t.h) \
    $(wildcard include/config/cpu/abrt/ev6.h) \
    $(wildcard include/config/cpu/abrt/ev7.h) \
    $(wildcard include/config/cpu/pabrt/legacy.h) \
    $(wildcard include/config/cpu/pabrt/v6.h) \
    $(wildcard include/config/cpu/pabrt/v7.h) \
  /home/xm/KuaiPan/kernel-update/kernel/linux-2.6.32.67/arch/arm/include/asm/vfpmacros.h \
    $(wildcard include/config/vfpv3.h) \
  /home/xm/KuaiPan/kernel-update/kernel/linux-2.6.32.67/arch/arm/include/asm/vfp.h \
  arch/arm/mach-s3c6400/include/mach/entry-macro.S \
  /home/xm/KuaiPan/kernel-update/kernel/linux-2.6.32.67/arch/arm/include/asm/hardware/vic.h \
  arch/arm/mach-s3c6400/include/mach/map.h \
  arch/arm/plat-s3c/include/plat/map-base.h \
  arch/arm/plat-s3c64xx/include/plat/irqs.h \
  /home/xm/KuaiPan/kernel-update/kernel/linux-2.6.32.67/arch/arm/include/asm/thread_notify.h \
  /home/xm/KuaiPan/kernel-update/kernel/linux-2.6.32.67/arch/arm/include/asm/unwind.h \
    $(wildcard include/config/arm/unwind.h) \
  /home/xm/KuaiPan/kernel-update/kernel/linux-2.6.32.67/arch/arm/include/asm/unistd.h \
    $(wildcard include/config/oabi/compat.h) \
  arch/arm/kernel/entry-header.S \
    $(wildcard include/config/frame/pointer.h) \
    $(wildcard include/config/alignment/trap.h) \
    $(wildcard include/config/cpu/32v6k.h) \
    $(wildcard include/config/cpu/v6.h) \
  include/linux/init.h \
    $(wildcard include/config/modules.h) \
    $(wildcard include/config/hotplug.h) \
  include/linux/linkage.h \
  /home/xm/KuaiPan/kernel-update/kernel/linux-2.6.32.67/arch/arm/include/asm/linkage.h \
  /home/xm/KuaiPan/kernel-update/kernel/linux-2.6.32.67/arch/arm/include/asm/assembler.h \
    $(wildcard include/config/cpu/feroceon.h) \
  /home/xm/KuaiPan/kernel-update/kernel/linux-2.6.32.67/arch/arm/include/asm/ptrace.h \
  /home/xm/KuaiPan/kernel-update/kernel/linux-2.6.32.67/arch/arm/include/asm/hwcap.h \
  include/asm/asm-offsets.h \
  /home/xm/KuaiPan/kernel-update/kernel/linux-2.6.32.67/arch/arm/include/asm/errno.h \
  include/asm-generic/errno.h \
  include/asm-generic/errno-base.h \
  /home/xm/KuaiPan/kernel-update/kernel/linux-2.6.32.67/arch/arm/include/asm/thread_info.h \
    $(wildcard include/config/arm/thumbee.h) \
  /home/xm/KuaiPan/kernel-update/kernel/linux-2.6.32.67/arch/arm/include/asm/fpstate.h \

arch/arm/kernel/entry-armv.o: $(deps_arch/arm/kernel/entry-armv.o)

$(deps_arch/arm/kernel/entry-armv.o):
