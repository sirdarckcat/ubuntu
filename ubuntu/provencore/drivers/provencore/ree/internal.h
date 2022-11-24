// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020-2021, ProvenRun S.A.S
 */
/**
 * @file internal.h
 * @brief Internal provencore driver definitions
 *
 * This file is supposed to be shared between all provencore driver files only.
 *
 * @author Alexandre Berdery
 * @date October 6th, 2020 (creation)
 * @copyright (c) 2020-2021, Prove & Run and/or its affiliates.
 *   All rights reserved.
 */

#ifndef PNC_INTERNAL_H_INCLUDED
#define PNC_INTERNAL_H_INCLUDED

#ifdef pr_fmt
#undef pr_fmt
#endif

/* Prefix all driver output with the same string */
#define pr_fmt(fmt) "pncree: " fmt

#endif /* PNC_INTERNAL_H_INCLUDED */
