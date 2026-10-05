/*
 * Copyright (c) 2026, Qualcomm Technologies, Inc. and/or its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef QTI_CPUCP_H
#define QTI_CPUCP_H

/*
 * Lean CPUCP host interface for TF-A. Only the clock-domain enable path that
 * is required during secondary-core cold boot is implemented natively. The
 * full CPUCP firmware-load / DCVS machinery still lives in qtiseclib.
 */

#ifdef QTI_CPUCP_ENABLED

/*
 * Bring up the SCMI channel to CPUCP. Must be called exactly once, by the
 * boot core, before any SCMI protocol (clock domain, or others added later)
 * is used over the channel.
 */
void cpucp_scmi_init(void);

/*
 * Request to enable the clock domain(s) owning the calling core on cold
 * boot. Returns an SCMI status code: SCMI_E_INVALID_PARAM if the calling
 * core has no clock domain mapping, SCMI_E_SUCCESS otherwise.
 */
int cpucp_clkdom_init(void);

/* Bring the CPUCP (EPSS/RISC-V MUC) microcontroller out of reset on cold boot. */
void qti_cpucp_init(void);

#else

/*
 * CPUCP is not built for this platform, so the BL31 cold-boot start is a no-op.
 * Provided as an inline stub so common callers need no build-time guard.
 */
static inline void qti_cpucp_init(void)
{
}

#endif /* QTI_CPUCP_ENABLED */

#endif /* QTI_CPUCP_H */
