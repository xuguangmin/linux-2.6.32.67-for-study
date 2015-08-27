cmd_arch/arm/kernel/debug.o := arm-linux-gcc -Wp,-MD,arch/arm/kernel/.debug.o.d  -nostdinc -isystem /home/xm/FriendlyARM/toolschain/4.5.1/bin/../lib/gcc/arm-none-linux-gnueabi/4.5.1/include -Iinclude  -I/home/xm/KuaiPan/kernel-update/kernel/linux-2.6.32.67/arch/arm/include -include include/linux/autoconf.h -D__KERNEL__ -mlittle-endian -Iarch/arm/mach-s3c6400/include -Iarch/arm/mach-s3c6410/include -Iarch/arm/plat-s3c64xx/include -Iarch/arm/plat-s3c/include -D__ASSEMBLY__ -mabi=aapcs-linux -mno-thumb-interwork -funwind-tables  -D__LINUX_ARM_ARCH__=6 -march=armv6k -mtune=arm1136j-s -include asm/unified.h -msoft-float -gdwarf-2       -c -o arch/arm/kernel/debug.o arch/arm/kernel/debug.S

deps_arch/arm/kernel/debug.o := \
  arch/arm/kernel/debug.S \
    $(wildcard include/config/debug/icedcc.h) \
    $(wildcard include/config/cpu/v6.h) \
    $(wildcard include/config/cpu/xscale.h) \
  /home/xm/KuaiPan/kernel-update/kernel/linux-2.6.32.67/arch/arm/include/asm/unified.h \
    $(wildcard include/config/arm/asm/unified.h) \
    $(wildcard include/config/thumb2/kernel.h) \
  include/linux/linkage.h \
  include/linux/compiler.h \
    $(wildcard include/config/trace/branch/profiling.h) \
    $(wildcard include/config/profile/all/branches.h) \
    $(wildcard include/config/enable/must/check.h) \
    $(wildcard include/config/enable/warn/deprecated.h) \
  /home/xm/KuaiPan/kernel-update/kernel/linux-2.6.32.67/arch/arm/include/asm/linkage.h \
  arch/arm/mach-s3c6400/include/mach/debug-macro.S \
    $(wildcard include/config/debug/s3c/uart.h) \
  arch/arm/mach-s3c6400/include/mach/map.h \
  arch/arm/plat-s3c/include/plat/map-base.h \
  arch/arm/plat-s3c/include/plat/regs-serial.h \
  arch/arm/plat-s3c/include/plat/debug-macro.S \

arch/arm/kernel/debug.o: $(deps_arch/arm/kernel/debug.o)

$(deps_arch/arm/kernel/debug.o):
