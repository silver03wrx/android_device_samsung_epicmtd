/* linux/arch/arm/mach-s5pv210/cpu.c
 *
 * Copyright (c) 2010 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/list.h>
#include <linux/timer.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/sysdev.h>
#include <linux/platform_device.h>
#include <linux/sched.h>

#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach/irq.h>

#include <asm/proc-fns.h>
#include <mach/map.h>
#include <mach/regs-clock.h>
#include <mach/idle.h>

#include <plat/cpu.h>
#include <plat/devs.h>
#include <plat/clock.h>
#include <plat/s5pv210.h>

/* Initial IO mappings */

static struct map_desc s5pv210_iodesc[] __initdata = {
	{
		.virtual	= (unsigned long)S5P_VA_SYSTIMER,
		.pfn		= __phys_to_pfn(S5PV210_PA_SYSTIMER),
		.length		= SZ_1M,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)VA_VIC2,
		.pfn		= __phys_to_pfn(S5PV210_PA_VIC2),
		.length		= SZ_16K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)VA_VIC3,
		.pfn		= __phys_to_pfn(S5PV210_PA_VIC3),
		.length		= SZ_16K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)S5P_VA_SROMC,
		.pfn		= __phys_to_pfn(S5PV210_PA_SROMC),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)S3C_VA_OTG,
		.pfn		= __phys_to_pfn(S5PV210_PA_OTG),
		.length		= SZ_1M,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)S3C_VA_OTGSFR,
		.pfn		= __phys_to_pfn(S5PV210_PA_OTGSFR),
		.length		= SZ_1M,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)S5P_VA_AUDSS,
		.pfn		= __phys_to_pfn(S5PV210_PA_AUDSS),
		.length		= SZ_1M,
		.type		= MT_DEVICE,
	},
       	{
		.virtual	= (unsigned long)S5P_VA_RTC,
		.pfn		= __phys_to_pfn(S5PV210_PA_RTC),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	},
};

void (*s5pv21x_idle)(void);

void s5pv210_default_idle(void)
{
	printk("default idle function\n");
}

static void s5pv210_idle(void)
{
#if 0
	if (!need_resched())
		cpu_do_idle();

	local_irq_enable();
#else

	unsigned int tmp;
#if defined(CONFIG_CPU_IDLE_MONITORING)
	set_gpio_monitor_cpuidle();

	tmp = __raw_readl(S5PC11X_GPH2DAT) & ~(0x1 << 6);
	tmp |= (0x1 << 6);
	__raw_writel(tmp, S5PC11X_GPH2DAT);
#endif
/*
 * 1. Set CFG_DIDLE field of IDLE_CFG. 
 * (0x0 for IDLE and 0x1 for DEEP-IDLE)
 * 2. Set TOP_LOGIC field of IDLE_CFG to 0x2
 * 3. Set CFG_STANDBYWFI field of PWR_CFG to 2'b01.
 * 4. Set PMU_INT_DISABLE bit of OTHERS register to 1'b01 to prevent interrupts from
 *    occurring while entering IDLE mode.
 * 5. Execute Wait For Interrupt instruction (WFI).
 */
	tmp = __raw_readl(S5P_IDLE_CFG);
	tmp &=~ ((3<<30)|(3<<28)|(1<<0));	// No DEEP IDLE
	tmp |= ((2<<30)|(2<<28));		// TOP logic : ON
	__raw_writel(tmp, S5P_IDLE_CFG);

	tmp = __raw_readl(S5P_PWR_CFG);
	tmp &= S5P_CFG_WFI_CLEAN;
	__raw_writel(tmp, S5P_PWR_CFG);

	tmp = __raw_readl(S5P_OTHERS);
	tmp &= ~(1<<0);
	__raw_writel(tmp, S5P_OTHERS);
	
	cpu_do_idle();
//	local_irq_enable();
#if defined(CONFIG_CPU_IDLE_MONITORING)
	tmp = __raw_readl(S5PC11X_GPH2DAT) & ~(0x1 << 6);
	__raw_writel(tmp, S5PC11X_GPH2DAT);
	restore_gpio_monitor();
#endif
	local_irq_enable();

#endif //0
}

/* s5pv210_map_io
 *
 * register the standard cpu IO areas
*/

void __init s5pv210_map_io(void)
{
	iotable_init(s5pv210_iodesc, ARRAY_SIZE(s5pv210_iodesc));

	/* set s5pc110 idle function */
	s5pv21x_idle = s5pv210_idle;
}

void __init s5pv210_init_clocks(int xtal)
{
	printk(KERN_DEBUG "%s: initializing clocks\n", __func__);

	s3c24xx_register_baseclocks(xtal);
	s5p_register_clocks(xtal);
	s5pv210_register_clocks();
	s5pv210_setup_clocks();
}

void __init s5pv210_init_irq(void)
{
	u32 vic[4];	/* S5PV210 supports 4 VIC */

	/* All the VICs are fully populated. */
	vic[0] = ~0;
	vic[1] = ~0;
	vic[2] = ~0;
	vic[3] = ~0;

	s5p_init_irq(vic, ARRAY_SIZE(vic));
}

struct sysdev_class s5pv210_sysclass = {
	.name	= "s5pv210-core",
};

static struct sys_device s5pv210_sysdev = {
	.cls	= &s5pv210_sysclass,
};

static int __init s5pv210_core_init(void)
{
	return sysdev_class_register(&s5pv210_sysclass);
}

core_initcall(s5pv210_core_init);

int __init s5pv210_init(void)
{
	printk(KERN_INFO "S5PV210: Initializing architecture\n");

	/* set idle function */
	pm_idle = s5pv210_idle;

	return sysdev_register(&s5pv210_sysdev);
}