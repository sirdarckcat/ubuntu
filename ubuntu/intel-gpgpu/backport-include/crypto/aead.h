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
#ifndef __BACKPORT_CRYPTO_AEAD_H
#define __BACKPORT_CRYPTO_AEAD_H
#include_next <crypto/aead.h>
#include <linux/version.h>

#if LINUX_VERSION_IS_LESS(4,2,0)
#define aead_request_set_ad LINUX_I915_BACKPORT(aead_request_set_ad)
static inline void aead_request_set_ad(struct aead_request *req,
				       unsigned int assoclen)
{
	req->assoclen = assoclen;
}

#define crypto_aead_reqsize LINUX_I915_BACKPORT(crypto_aead_reqsize)
unsigned int crypto_aead_reqsize(struct crypto_aead *tfm);

struct aead_request *crypto_backport_convert(struct aead_request *req);

static inline int backport_crypto_aead_encrypt(struct aead_request *req)
{
	return crypto_aead_encrypt(crypto_backport_convert(req));
}
#define crypto_aead_encrypt LINUX_I915_BACKPORT(crypto_aead_encrypt)

static inline int backport_crypto_aead_decrypt(struct aead_request *req)
{
	return crypto_aead_decrypt(crypto_backport_convert(req));
}
#define crypto_aead_decrypt LINUX_I915_BACKPORT(crypto_aead_decrypt)

#endif /* LINUX_VERSION_IS_LESS(4,2,0) */

#endif /* __BACKPORT_CRYPTO_AEAD_H */
