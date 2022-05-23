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
/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright © 2016 Intel Corporation
 */

#include "mock_context.h"
#include "selftests/mock_drm.h"
#include "selftests/mock_gtt.h"

struct i915_gem_context *
mock_context(struct drm_i915_private *i915,
	     const char *name)
{
	struct i915_gem_context *ctx;
	struct i915_gem_engines *e;

	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return NULL;

	kref_init(&ctx->ref);
	INIT_LIST_HEAD(&ctx->link);
	ctx->i915 = i915;

	mutex_init(&ctx->mutex);

	spin_lock_init(&ctx->stale.lock);
	INIT_LIST_HEAD(&ctx->stale.engines);

	i915_gem_context_set_persistence(ctx);

	mutex_init(&ctx->engines_mutex);
	e = default_engines(ctx);
	if (IS_ERR(e))
		goto err_free;
	RCU_INIT_POINTER(ctx->engines, e);

	INIT_RADIX_TREE(&ctx->handles_vma, GFP_KERNEL);
	mutex_init(&ctx->lut_mutex);

	if (name) {
		struct i915_ppgtt *ppgtt;

		strncpy(ctx->name, name, sizeof(ctx->name) - 1);

		ppgtt = mock_ppgtt(i915, name);
		if (!ppgtt)
			goto err_put;

		mutex_lock(&ctx->mutex);
		__set_ppgtt(ctx, &ppgtt->vm);
		mutex_unlock(&ctx->mutex);

		i915_vm_put(&ppgtt->vm);
	}

	return ctx;

err_free:
	kfree(ctx);
	return NULL;

err_put:
	i915_gem_context_set_closed(ctx);
	i915_gem_context_put(ctx);
	return NULL;
}

void mock_context_close(struct i915_gem_context *ctx)
{
	context_close(ctx);
}

void mock_init_contexts(struct drm_i915_private *i915)
{
	init_contexts(&i915->gem.contexts);
}

struct i915_gem_context *
live_gt_context(struct intel_gt *gt, struct file *file)
{
	struct i915_gem_context *ctx;
	int err;
	u32 id;

	ctx = i915_gem_context_create_for_gt(gt, 0);
	if (IS_ERR(ctx))
		return ctx;

	i915_gem_context_set_no_error_capture(ctx);

	err = gem_context_register(ctx, to_drm_file(file)->driver_priv, &id);
	if (err < 0)
		goto err_ctx;

	return ctx;

err_ctx:
	context_close(ctx);
	return ERR_PTR(err);
}

struct i915_gem_context *
live_context(struct drm_i915_private *i915, struct file *file)
{
	return live_gt_context(to_gt(i915), file);
}

struct i915_gem_context *
live_context_for_engine(struct intel_engine_cs *engine, struct file *file)
{
	struct i915_gem_engines *engines;
	struct i915_gem_context *ctx;
	struct intel_context *ce;

	engines = alloc_engines(1);
	if (!engines)
		return ERR_PTR(-ENOMEM);

	ctx = live_gt_context(engine->gt, file);
	if (IS_ERR(ctx)) {
		__free_engines(engines, 0);
		return ctx;
	}

	ce = intel_context_create(engine);
	if (IS_ERR(ce)) {
		__free_engines(engines, 0);
		return ERR_CAST(ce);
	}

	intel_context_set_gem(ce, ctx);
	engines->engines[0] = ce;
	engines->num_engines = 1;

	mutex_lock(&ctx->engines_mutex);
	i915_gem_context_set_user_engines(ctx);
	engines = rcu_replace_pointer(ctx->engines, engines, 1);
	mutex_unlock(&ctx->engines_mutex);

	engines_idle_release(ctx, engines);

	return ctx;
}

struct i915_gem_context *
kernel_context(struct drm_i915_private *i915)
{
	struct i915_gem_context *ctx;

	ctx = i915_gem_context_create_for_gt(to_gt(i915), 0);
	if (IS_ERR(ctx))
		return ctx;

	i915_gem_context_clear_bannable(ctx);
	i915_gem_context_set_persistence(ctx);
	i915_gem_context_set_no_error_capture(ctx);

	return ctx;
}

void kernel_context_close(struct i915_gem_context *ctx)
{
	context_close(ctx);
}
