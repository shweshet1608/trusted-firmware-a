/*
 * Copyright (c) 2026, Qualcomm Technologies, Inc. and/or its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef QTI_CPUCP_CPU_CONFIG_H
#define QTI_CPUCP_CPU_CONFIG_H

#include <clkdom_config.h>

/*
 * One entry per core cluster: cpumask identifies the cluster's cores, cpu_id
 * and l3_id are the clock domains that must be enabled for a core in that
 * cluster (either may already be initialised elsewhere, e.g. by the boot
 * firmware; see clkdom_init_status[]).
 */
struct clkdom_parent_map {
	unsigned int cpumask;
	unsigned int cpu_id;
	unsigned int l3_id;
};

extern const struct clkdom_parent_map clkdom_parent_maps[CLKDOM_PARENT_MAP_COUNT];
extern unsigned int clkdom_init_status[CD_MAX];

#endif /* QTI_CPUCP_CPU_CONFIG_H */
