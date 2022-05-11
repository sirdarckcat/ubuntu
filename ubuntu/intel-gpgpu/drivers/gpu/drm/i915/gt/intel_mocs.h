/*
 * INTEL CONFIDENTIAL
 *
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * This software and the related documents are Intel copyrighted materials,
 * and your use of them is governed by the express license under which they
 * were provided to you ("LICENSE"). Unless the LICENSE provides otherwise,
 * you may not use, modify, copy, publish, distribute, disclose or transmit
 * this software or the related documents without Intel's prior written
 * permission.
 *
 * This software and the related documents are provided as is, with no
 * express or implied warranties, other than those that are expressly
 * stated in the License.
 *
 */
/* SPDX-License-Identifier: MIT */
/*
 * Copyright © 2015 Intel Corporation
 */

#ifndef INTEL_MOCS_H
#define INTEL_MOCS_H

/**
 * DOC: Memory Objects Control State (MOCS)
 *
 * Motivation:
 * In previous Gens the MOCS settings was a value that was set by user land as
 * part of the batch. In Gen9 this has changed to be a single table (per ring)
 * that all batches now reference by index instead of programming the MOCS
 * directly.
 *
 * The one wrinkle in this is that only PART of the MOCS tables are included
 * in context (The GFX_MOCS_0 - GFX_MOCS_64 and the LNCFCMOCS0 - LNCFCMOCS32
 * registers). The rest are not (the settings for the other rings).
 *
 * This table needs to be set at system start-up because the way the table
 * interacts with the contexts and the GmmLib interface.
 *
 *
 * Implementation:
 *
 * The tables (one per supported platform) are defined in intel_mocs.c
 * and are programmed in the first batch after the context is loaded
 * (with the hardware workarounds). This will then let the usual
 * context handling keep the MOCS in step.
 */

struct intel_engine_cs;
struct intel_gt;

void intel_mocs_init(struct intel_gt *gt);
void intel_mocs_init_engine(struct intel_engine_cs *engine);
void intel_set_mocs_index(struct intel_gt *gt);

#endif
