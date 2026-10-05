/*
 * Copyright (c) 2026, Qualcomm Technologies, Inc. and/or its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>
#include <stdint.h>

#include <common/debug.h>
#include <cpu_config.h>
#include <cpucp.h>
#include <cpucp_hwio.h>
#include <drivers/arm/css/scmi.h>
#include <lib/mmio.h>
#include <lib/spinlock.h>
#include <plat/common/platform.h>

/*
 * Lean CPUCP host driver for TF-A. Only the clock-domain enable path that is
 * required during secondary-core cold boot is implemented. CPUCP speaks the
 * SCMI protocol over a shared-memory mailbox, so the generic TF-A SCMI driver
 * (drivers/arm/css/scmi) is reused for both the transport and the SCMI Clock
 * Management "config set" command; this file only wires up the CPUCP
 * mailbox/doorbell and the clock-domain parent map.
 */

/* CLOCK_CONFIG_SET attributes: enable the clock. */
#define SCMI_CLOCK_CONFIG_SET_ENABLE		0x1

/* CPUCP doorbell: APSS -> CPUCP IPC interrupt trigger bit. */
#define CPUCP_DB_MODIFY_MASK		\
	(1U << APSS_SHARED_TZ_IPC_INTERRUPT_OSM_IPC_SHFT)
#define CPUCP_DB_PRESERVE_MASK		(~CPUCP_DB_MODIFY_MASK)

/* CPUCP SCMI channel poll interval and command timeout, in micro-seconds. */
#define CPUCP_SCMI_POLL_DELAY_US		1U
#define CPUCP_SCMI_TIMEOUT_US			(500U * 1000U)

static scmi_lock_t cpucp_scmi_lock;
static spinlock_t cpucp_clkdom_init_spinlock;
static void *cpucp_scmi_handle;

/* Ring the CPUCP doorbell to hand the SCMI channel over for processing. */
static void cpucp_ring_doorbell(struct scmi_channel_plat_info *plat_info)
{
	mmio_write_32(plat_info->db_reg_addr, plat_info->db_modify_mask);
}

static scmi_channel_plat_info_t cpucp_scmi_plat_info = {
	.scmi_mbx_mem = CPUCP_IPC_SEC_BUF_BASE,
	.db_reg_addr = APSS_SHARED_TZ_IPC_INTERRUPT_ADDR,
	.db_preserve_mask = CPUCP_DB_PRESERVE_MASK,
	.db_modify_mask = CPUCP_DB_MODIFY_MASK,
	.ring_doorbell = &cpucp_ring_doorbell,
	.delay = CPUCP_SCMI_POLL_DELAY_US,
	.timeout = CPUCP_SCMI_TIMEOUT_US,
};

static scmi_channel_t cpucp_scmi_channel = {
	.info = &cpucp_scmi_plat_info,
	.lock = &cpucp_scmi_lock,
};

/*
 * cpucp_scmi_init - bring up the SCMI channel to CPUCP.
 *
 * Must be called exactly once, by the boot core, before any other core can
 * start issuing SCMI commands over the channel (e.g. before secondary cores
 * are brought online).
 */
void cpucp_scmi_init(void)
{
	cpucp_scmi_handle = scmi_init(&cpucp_scmi_channel, NULL);
	assert(cpucp_scmi_handle != NULL);
}

/*
 * clkdom_enable - issue the SCMI "config set" enable command for @clkdom if
 * it has not already been initialised. Caller must hold
 * cpucp_clkdom_init_spinlock.
 */
static void clkdom_enable(unsigned int clkdom)
{
	int ret;

	if (clkdom_init_status[clkdom] == 0U) {
		ret = scmi_clock_config_set(cpucp_scmi_handle, clkdom,
					     SCMI_CLOCK_CONFIG_SET_ENABLE, 0U);
		if (ret == SCMI_E_SUCCESS) {
			clkdom_init_status[clkdom] = 1U;
		} else {
			ERROR("Failed to enable clock domain %u (%d)\n",
			      clkdom, ret);
		}
	}
}

/*
 * cpucp_clkdom_init - request CPUCP to enable the CPU and L3 clock domains
 * owning the calling core during cold boot.
 *
 * Looks up the parent clock domains for the calling core's cluster (its CPU
 * domain and the L3 domain it shares with the rest of the cluster) and
 * enables whichever of the two have not already been initialised. Either
 * domain may already be up (e.g. brought up by the boot firmware instead of
 * over SCMI), so each is checked independently. Requires cpucp_scmi_init()
 * to have already been called.
 *
 * Returns SCMI_E_INVALID_PARAM if the calling core does not belong to any
 * entry in clkdom_parent_maps[], SCMI_E_SUCCESS otherwise.
 */
int cpucp_clkdom_init(void)
{
	unsigned int core_pos = plat_my_core_pos();
	uint32_t linear_id;
	unsigned int i;

	assert(cpucp_scmi_handle != NULL);

	/* CPU masks are 32-bit, so the core index must fit a 32-bit shift. */
	assert(core_pos < 32U);
	linear_id = 1U << core_pos;

	spin_lock(&cpucp_clkdom_init_spinlock);

	for (i = 0U; i < CLKDOM_PARENT_MAP_COUNT; i++) {
		if ((linear_id & clkdom_parent_maps[i].cpumask) == 0U) {
			continue;
		}

		clkdom_enable(clkdom_parent_maps[i].l3_id);
		clkdom_enable(clkdom_parent_maps[i].cpu_id);
		break;
	}

	spin_unlock(&cpucp_clkdom_init_spinlock);

	return (i < CLKDOM_PARENT_MAP_COUNT) ? SCMI_E_SUCCESS : SCMI_E_INVALID_PARAM;
}
