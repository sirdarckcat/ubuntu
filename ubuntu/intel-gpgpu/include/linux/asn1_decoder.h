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
/* Automatically created during backport process */
#ifndef CPTCFG_BPAUTO_ASN1_DECODER
#include_next <linux/asn1_decoder.h>
#else
#undef asn1_ber_decoder
#define asn1_ber_decoder LINUX_I915_BACKPORT(asn1_ber_decoder)
#include <linux/backport-asn1_decoder.h>
#endif /* CPTCFG_BPAUTO_ASN1_DECODER */
