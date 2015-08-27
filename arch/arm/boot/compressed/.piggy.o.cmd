cmd_arch/arm/boot/compressed/piggy.o := arm-linux-gcc -Wp,-MD,arch/arm/boot/compressed/.piggy.o.d  -nostdinc -isystem /home/xm/FriendlyARM/toolschain/4.5.1/bin/../lib/gcc/arm-none-linux-gnueabi/4.5.1/include -Iinclude  -I/home/xm/KuaiPan/kernel-update/kernel/linux-2.6.32.67/arch/arm/include -include include/linux/autoconf.h -D__KERNEL__ -mlittle-endian -Iarch/arm/mach-s3c6400/include -Iarch/arm/mach-s3c6410/include -Iarch/arm/plat-s3c64xx/include -Iarch/arm/plat-s3c/include -D__ASSEMBLY__ -mabi=aapcs-linux -mno-thumb-interwork -funwind-tables  -D__LINUX_ARM_ARCH__=6 -march=armv6k -mtune=arm1136j-s -include asm/unified.h -msoft-float -gdwarf-2     -Wa,-march=all   -c -o arch/arm/boot/compressed/piggy.o arch/arm/boot/compressed/piggy.S

deps_arch/arm/boot/compressed/piggy.o := \
  arch/arm/boot/compressed/piggy.S \
  /home/xm/KuaiPan/kernel-update/kernel/linux-2.6.32.67/arch/arm/include/asm/unified.h \
    $(wildcard include/config/arm/asm/unified.h) \
    $(wildcard include/config/thumb2/kernel.h) \

arch/arm/boot/compressed/piggy.o: $(deps_arch/arm/boot/compressed/piggy.o)

$(deps_arch/arm/boot/compressed/piggy.o):
