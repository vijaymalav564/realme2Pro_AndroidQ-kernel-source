/*
 * Copyright (c) 2016, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __CORE_CTL_H
#define __CORE_CTL_H

#ifdef CONFIG_SCHED_CORE_CTL
void core_ctl_check(u64 wallclock);
int core_ctl_set_boost(bool boost);
#ifdef CONFIG_PRODUCT_REALME_RMX1801
//INDIA.Performance add declaration
int hypnus_set_min_max_cpus(unsigned int index, unsigned int min, unsigned int max);
#endif /* CONFIG_PRODUCT_REALME_RMX1801 */
#else
static inline void core_ctl_check(u64 wallclock) {}
static inline int core_ctl_set_boost(bool boost)
{
	return 0;
}
#ifdef CONFIG_PRODUCT_REALME_RMX1801
//INDIA.Performance add declaration
int hypnus_set_min_max_cpus(unsigned int index, unsigned int min, unsigned int max);
#endif /* CONFIG_PRODUCT_REALME_RMX1801 */
#endif
#endif
